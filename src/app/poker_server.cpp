#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "agents/cfr_agent.h"
#include "agents/equity_agent.h"
#include "agents/rule_agent.h"
#include "core/deck.h"
#include "core/rng.h"
#include "engine/game_config.h"
#include "engine/game_state.h"
#include "eval/hand_evaluator.h"

namespace fs = std::filesystem;
using namespace poker;

// Simple JSON helper functions
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string json_str(const std::string& key, const std::string& val) {
    return "\"" + key + "\":\"" + json_escape(val) + "\"";
}

static std::string json_int(const std::string& key, long long val) {
    return "\"" + key + "\":" + std::to_string(val);
}

static std::string json_bool(const std::string& key, bool val) {
    return "\"" + key + "\":" + (val ? "true" : "false");
}

static std::string card_to_json(Card c) {
    static constexpr const char* SUIT_NAMES[] = {"clubs", "diamonds", "hearts", "spades"};
    static constexpr const char* SUIT_SYMS[] = {"♣", "♦", "♥", "♠"};
    static constexpr const char* SUIT_COLORS[] = {"green", "blue", "red", "black"};
    static constexpr const char RANKS[] = "23456789TJQKA";

    const int r = c.rank();
    const int su = c.suit();
    const std::string rank_str = (RANKS[r] == 'T') ? "10" : std::string(1, RANKS[r]);
    const std::string code = c.to_string();
    const std::string display_code = rank_str + SUIT_SYMS[su];

    std::string out = "{";
    out += json_str("code", code) + ",";
    out += json_str("displayCode", display_code) + ",";
    out += json_str("rank", rank_str) + ",";
    out += json_int("rankValue", r) + ",";
    out += json_str("suit", SUIT_NAMES[su]) + ",";
    out += json_str("suitSymbol", SUIT_SYMS[su]) + ",";
    out += json_str("suitColor", SUIT_COLORS[su]);
    out += "}";
    return out;
}

static std::string cards_to_json_array(const std::vector<Card>& cards) {
    std::string out = "[";
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) out += ",";
        out += card_to_json(cards[i]);
    }
    out += "]";
    return out;
}

// Simple JSON field extractor for minimal payload parsing
static std::string parse_json_string(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos || json[pos] != '"') return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static long long parse_json_int(const std::string& json, const std::string& key, long long def_val = 0) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return def_val;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return def_val;
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return def_val;
    try {
        return std::stoll(json.substr(pos));
    } catch (...) {
        return def_val;
    }
}

static bool parse_json_bool(const std::string& json, const std::string& key, bool def_val = false) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return def_val;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return def_val;
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return def_val;
    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return def_val;
}

// Strategy Cache
static std::map<std::string, std::shared_ptr<const HoldemStrategy>> g_strat_cache;

static std::shared_ptr<const HoldemStrategy> get_strategy(const std::string& path) {
    auto it = g_strat_cache.find(path);
    if (it != g_strat_cache.end()) return it->second;

    std::ifstream f(path);
    if (!f) return nullptr;
    try {
        auto strat = std::make_shared<const HoldemStrategy>(HoldemStrategy::load(f));
        g_strat_cache[path] = strat;
        return strat;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load strategy '" << path << "': " << e.what() << "\n";
        return nullptr;
    }
}

static int get_match_small_blind(int hand) {
    if (hand <= 5) return 250;
    if (hand <= 9) return 1000;
    if (hand <= 15) return 2500;
    return 5000;
}

static int get_match_big_blind(int hand) {
    if (hand <= 5) return 500;
    if (hand <= 9) return 2000;
    if (hand <= 15) return 5000;
    return 10000;
}

// Global active game session
struct Session {
    std::string game_mode = "infinite"; // "infinite" or "match"
    bool match_over = false;
    std::string match_winner = "";

    std::string bot_type = "cfr";
    std::string bot_version = "versions/run4.cfr";
    std::string bot_label = "Holden (run4.cfr)";
    GameConfig cfg;

    std::unique_ptr<Agent> bot;
    std::unique_ptr<GameState> state;
    Deck deck;
    Rng rng;

    int hand_number = 0;
    int button = 0; // 0 = hero is SB, 1 = bot is SB
    long long total_chips_won = 0;
    int hands_played = 0;
    int shown_board_count = 0;

    bool auto_rebuy = false; // false = persistent stacks across rounds, true = reset to 10k every round
    int hero_bankroll = 10000;
    int bot_bankroll = 10000;

    std::vector<std::string> event_logs;

    Session() : rng(1337) {}
};

static Session g_session;

static void init_bot() {
    uint64_t seed = g_session.rng.below(1000000000ULL) + 1;
    if (g_session.bot_type == "rule") {
        g_session.bot = std::make_unique<RuleAgent>(seed);
        g_session.bot_label = "RuleBot";
    } else if (g_session.bot_type == "equity") {
        g_session.bot = std::make_unique<EquityAgent>(seed, 2000);
        g_session.bot_label = "EquityBot";
    } else {
        // CFR Bot Holden
        std::string path = g_session.bot_version;
        if (!fs::exists(path) && fs::exists("versions/" + path)) path = "versions/" + path;
        auto s = get_strategy(path);
        if (!s) {
            // Fallback to first available
            if (fs::exists("versions/run4.cfr")) path = "versions/run4.cfr";
            else if (fs::exists("holdem.cfr")) path = "holdem.cfr";
            s = get_strategy(path);
        }
        if (s) {
            g_session.cfg = s->cfg;
            const size_t slash = path.find_last_of('/');
            std::string fname = (slash == std::string::npos ? path : path.substr(slash + 1));
            g_session.bot_label = "Hold'em (" + fname + ")";
            g_session.bot = std::make_unique<CfrAgent>(s, seed, g_session.bot_label);
        } else {
            g_session.bot = std::make_unique<RuleAgent>(seed);
            g_session.bot_label = "RuleBot (fallback)";
        }
    }
}

static std::string hand_rank_description(const std::vector<Card>& cards) {
    if (cards.size() < 5) return "";
    HandScore sc = evaluate(cards);
    return category_name(category_of(sc));
}

static std::string state_to_json(const std::vector<std::string>& new_events = {}) {
    auto& s = *g_session.state;
    const int button = s.button();
    const bool is_term = s.is_terminal();
    const auto hole0 = s.hole(0);
    const auto board = s.observe(0).board;

    std::string out = "{";
    out += json_int("handNumber", g_session.hand_number) + ",";
    out += json_int("button", button) + ",";
    out += json_str("street", [&]() {
        switch (s.street()) {
            case Street::Preflop: return "Preflop";
            case Street::Flop: return "Flop";
            case Street::Turn: return "Turn";
            case Street::River: return "River";
            case Street::Showdown: return "Showdown";
        }
        return "Unknown";
    }()) + ",";

    out += json_int("pot", s.pot()) + ",";
    out += json_int("currentBet", std::max(s.bet(0), s.bet(1))) + ",";
    out += json_int("toCall", std::max(0, s.bet(1) - s.bet(0))) + ",";
    out += json_str("toAct", is_term ? "none" : (s.current_player() == 0 ? "hero" : "bot")) + ",";
    out += json_bool("isTerminal", is_term) + ",";
    out += json_str("gameMode", g_session.game_mode) + ",";
    out += "\"match\":{";
    out += json_bool("isMatchMode", g_session.game_mode == "match") + ",";
    out += json_int("matchHandNumber", g_session.hand_number + 1) + ",";
    out += json_int("sb", g_session.cfg.small_blind) + ",";
    out += json_int("bb", g_session.cfg.big_blind) + ",";
    int cur_hand = g_session.hand_number + 1;
    int next_hand = cur_hand <= 5 ? 6 : (cur_hand <= 9 ? 10 : (cur_hand <= 15 ? 16 : 0));
    int next_sb = next_hand > 0 ? get_match_small_blind(next_hand) : 0;
    int next_bb = next_hand > 0 ? get_match_big_blind(next_hand) : 0;
    out += json_int("nextIncreaseHand", next_hand) + ",";
    out += json_int("nextSb", next_sb) + ",";
    out += json_int("nextBb", next_bb) + ",";
    out += json_bool("isMatchOver", g_session.match_over) + ",";
    out += json_str("matchWinner", g_session.match_winner);
    out += "},";

    // Hero
    out += "\"hero\":{";
    out += json_int("stack", s.stack(0)) + ",";
    out += json_int("bet", s.bet(0)) + ",";
    out += json_bool("isButton", button == 0) + ",";
    out += "\"hole\":" + cards_to_json_array({hole0[0], hole0[1]}) + ",";
    std::vector<Card> hero_eval = {hole0[0], hole0[1]};
    hero_eval.insert(hero_eval.end(), board.begin(), board.end());
    out += json_str("handRank", hand_rank_description(hero_eval));
    out += "},";

    // Bot
    out += "\"bot\":{";
    out += json_str("name", g_session.bot_label) + ",";
    out += json_int("stack", s.stack(1)) + ",";
    out += json_int("bet", s.bet(1)) + ",";
    out += json_bool("isButton", button == 1) + ",";
    const auto hole1 = s.hole(1);
    std::vector<Card> bot_eval = {hole1[0], hole1[1]};
    bot_eval.insert(bot_eval.end(), board.begin(), board.end());

    if (is_term && s.history().empty() == false && s.history().back() != 'f') {
        out += "\"hole\":" + cards_to_json_array({hole1[0], hole1[1]}) + ",";
        out += json_str("handRank", hand_rank_description(bot_eval)) + ",";
    } else {
        out += "\"hole\":null," + json_str("handRank", "") + ",";
    }

    if (is_term) {
        out += "\"actualHole\":" + cards_to_json_array({hole1[0], hole1[1]}) + ",";
        out += json_str("actualHandRank", hand_rank_description(bot_eval)) + ",";
        bool folded = !s.history().empty() && s.history().back() == 'f';
        out += json_bool("folded", folded) + ",";
        out += json_str("foldedPlayer", folded ? (s.payoffs()[0] > 0 ? "bot" : "hero") : "none");
    } else {
        out += "\"actualHole\":null,\"actualHandRank\":\"\",\"folded\":false,\"foldedPlayer\":\"none\"";
    }
    out += "},";

    // Board
    out += "\"board\":" + cards_to_json_array(board) + ",";

    // Legal Actions for Hero
    out += "\"legalActions\":[";
    if (!is_term && s.current_player() == 0) {
        auto legal = s.legal_actions();
        const int allin_amount = s.stack(0) + s.bet(0);
        for (size_t i = 0; i < legal.size(); ++i) {
            if (i > 0) out += ",";
            const auto& act = legal[i];
            const bool is_allin = (act.type == ActionType::Raise && act.amount == allin_amount);
            out += "{";
            out += json_str("type", [&]() {
                switch (act.type) {
                    case ActionType::Fold: return "fold";
                    case ActionType::Check: return "check";
                    case ActionType::Call: return "call";
                    case ActionType::Raise: return "raise";
                }
                return "";
            }()) + ",";
            out += json_int("amount", act.amount) + ",";
            out += json_bool("isAllIn", is_allin) + ",";
            out += json_str("label", [&]() {
                switch (act.type) {
                    case ActionType::Fold: return std::string("Fold");
                    case ActionType::Check: return std::string("Check");
                    case ActionType::Call: {
                        int to_call = std::min(s.bet(1) - s.bet(0), s.stack(0));
                        return "Call " + std::to_string(to_call);
                    }
                    case ActionType::Raise: {
                        if (is_allin) return "All-In (" + std::to_string(act.amount) + ")";
                        return "Raise to " + std::to_string(act.amount);
                    }
                }
                return std::string("?");
            }());
            out += "}";
        }
    }
    out += "],";

    // Events in this step
    out += "\"events\":[";
    for (size_t i = 0; i < new_events.size(); ++i) {
        if (i > 0) out += ",";
        out += new_events[i];
    }
    out += "],";

    // Session Stats
    out += "\"session\":{";
    out += json_int("handsPlayed", g_session.hands_played) + ",";
    out += json_int("totalChipsWon", g_session.total_chips_won) + ",";
    out += json_bool("autoRebuy", g_session.auto_rebuy) + ",";
    out += json_int("heroBankroll", g_session.hero_bankroll) + ",";
    out += json_int("botBankroll", g_session.bot_bankroll) + ",";
    double bb_per_hand = g_session.hands_played > 0
        ? static_cast<double>(g_session.total_chips_won) / g_session.hands_played / g_session.cfg.big_blind
        : 0.0;
    out += "\"bbPerHand\":" + std::to_string(bb_per_hand) + ",";
    out += "\"bbPer100\":" + std::to_string(bb_per_hand * 100.0) + ",";

    // Holden telemetry
    if (auto* cfr = dynamic_cast<CfrAgent*>(g_session.bot.get())) {
        out += json_int("holdenDecisions", cfr->decisions()) + ",";
        out += json_int("holdenFallbacks", cfr->fallbacks()) + ",";
        double fallback_rate = cfr->decisions() > 0 ? (100.0 * cfr->fallbacks() / cfr->decisions()) : 0.0;
        out += "\"holdenFallbackPct\":" + std::to_string(fallback_rate);
    } else {
        out += json_int("holdenDecisions", 0) + ",";
        out += json_int("holdenFallbacks", 0) + ",";
        out += "\"holdenFallbackPct\":0.0";
    }
    out += "}";

    // Payoffs if terminal
    if (is_term) {
        auto pay = s.payoffs();
        out += ",\"payoffs\":[" + std::to_string(pay[0]) + "," + std::to_string(pay[1]) + "]";
        out += ",\"winner\":" + std::string(pay[0] > 0 ? "\"hero\"" : (pay[0] < 0 ? "\"bot\"" : "\"split\""));
    }

    out += "}";
    return out;
}

static void deal_new_hand(std::vector<std::string>& events) {
    if (g_session.game_mode == "match") {
        if (g_session.hero_bankroll <= 0 || g_session.bot_bankroll <= 0) {
            g_session.match_over = true;
            g_session.match_winner = (g_session.hero_bankroll > 0) ? "hero" : "bot";
            return;
        }
        g_session.auto_rebuy = false;
        int hand = g_session.hand_number + 1;
        g_session.cfg.small_blind = get_match_small_blind(hand);
        g_session.cfg.big_blind = get_match_big_blind(hand);
    } else {
        if (g_session.auto_rebuy) {
            g_session.hero_bankroll = 10000;
            g_session.bot_bankroll = 10000;
        } else {
            // Persistent stacks: Rebuy if a player drops below the big blind (min buy-in 100 chips)
            if (g_session.hero_bankroll < g_session.cfg.big_blind) {
                g_session.hero_bankroll = 10000;
                std::string r_evt = "{";
                r_evt += json_str("type", "rebuy") + ",";
                r_evt += json_str("player", "hero") + ",";
                r_evt += json_int("amount", 10000);
                r_evt += "}";
                events.push_back(r_evt);
            }
            if (g_session.bot_bankroll < g_session.cfg.big_blind) {
                g_session.bot_bankroll = 10000;
                std::string r_evt = "{";
                r_evt += json_str("type", "rebuy") + ",";
                r_evt += json_str("player", "bot") + ",";
                r_evt += json_int("amount", 10000);
                r_evt += "}";
                events.push_back(r_evt);
            }
        }
    }

    g_session.deck.shuffle(g_session.rng);
    g_session.button = g_session.hand_number % 2;

    g_session.state = std::make_unique<GameState>(
        g_session.cfg, g_session.button, g_session.deck,
        std::array<int, 2>{g_session.hero_bankroll, g_session.bot_bankroll}
    );
    g_session.shown_board_count = 0;

    std::string deal_evt = "{";
    deal_evt += json_str("type", "deal") + ",";
    deal_evt += json_int("handNumber", g_session.hand_number + 1) + ",";
    deal_evt += json_int("button", g_session.button) + ",";
    deal_evt += json_int("sb", g_session.cfg.small_blind) + ",";
    deal_evt += json_int("bb", g_session.cfg.big_blind);
    deal_evt += "}";
    events.push_back(deal_evt);

    // If bot is SB, bot is first to act preflop!
    if (!g_session.state->is_terminal() && g_session.state->current_player() == 1) {
        Action bot_act = g_session.bot->act(g_session.state->observe(1), g_session.state->legal_actions());
        g_session.state->apply(bot_act);

        std::string act_evt = "{";
        act_evt += json_str("type", "action") + ",";
        act_evt += json_str("actor", "bot") + ",";
        act_evt += json_str("action", bot_act.to_string()) + ",";
        act_evt += json_int("amount", bot_act.amount) + ",";
        act_evt += json_int("pot", g_session.state->pot());
        act_evt += "}";
        events.push_back(act_evt);
    }
}

// Process action loop until Hero's turn or terminal
static void run_game_loop(std::vector<std::string>& events) {
    auto& s = *g_session.state;

    // Check board updates
    int cur_visible = s.board_visible();
    if (cur_visible > g_session.shown_board_count) {
        g_session.shown_board_count = cur_visible;
        std::string brd_evt = "{";
        brd_evt += json_str("type", "street_deal") + ",";
        brd_evt += json_int("count", cur_visible) + ",";
        brd_evt += "\"board\":" + cards_to_json_array(s.observe(0).board);
        brd_evt += "}";
        events.push_back(brd_evt);
    }

    while (!s.is_terminal() && s.current_player() == 1) {
        Action act = g_session.bot->act(s.observe(1), s.legal_actions());
        s.apply(act);

        std::string act_evt = "{";
        act_evt += json_str("type", "action") + ",";
        act_evt += json_str("actor", "bot") + ",";
        act_evt += json_str("action", act.to_string()) + ",";
        act_evt += json_int("amount", act.amount) + ",";
        act_evt += json_int("pot", s.pot());
        act_evt += "}";
        events.push_back(act_evt);

        // Check if street advanced
        cur_visible = s.board_visible();
        if (cur_visible > g_session.shown_board_count) {
            g_session.shown_board_count = cur_visible;
            std::string b_evt = "{";
            b_evt += json_str("type", "street_deal") + ",";
            b_evt += json_int("count", cur_visible) + ",";
            b_evt += "\"board\":" + cards_to_json_array(s.observe(0).board);
            b_evt += "}";
            events.push_back(b_evt);
        }
    }

    if (s.is_terminal()) {
        auto pay = s.payoffs();
        g_session.total_chips_won += pay[0];
        g_session.hands_played++;

        // Update persistent bankroll
        g_session.hero_bankroll += pay[0];
        g_session.bot_bankroll += pay[1];

        if (g_session.game_mode == "match") {
            if (g_session.hero_bankroll <= 0) {
                g_session.hero_bankroll = 0;
                g_session.bot_bankroll = 20000;
                g_session.match_over = true;
                g_session.match_winner = "bot";
            } else if (g_session.bot_bankroll <= 0) {
                g_session.bot_bankroll = 0;
                g_session.hero_bankroll = 20000;
                g_session.match_over = true;
                g_session.match_winner = "hero";
            }
        }

        bool folded = !s.history().empty() && s.history().back() == 'f';
        std::string folded_player = folded ? (pay[0] > 0 ? "bot" : "hero") : "none";

        std::string term_evt = "{";
        term_evt += json_str("type", "hand_complete") + ",";
        term_evt += json_int("heroPayoff", pay[0]) + ",";
        term_evt += json_int("botPayoff", pay[1]) + ",";
        term_evt += json_str("winner", pay[0] > 0 ? "hero" : (pay[0] < 0 ? "bot" : "split")) + ",";
        term_evt += json_bool("folded", folded) + ",";
        term_evt += json_str("foldedPlayer", folded_player) + ",";
        term_evt += json_bool("isMatchOver", g_session.match_over) + ",";
        term_evt += json_str("matchWinner", g_session.match_winner);
        term_evt += "}";
        events.push_back(term_evt);
    }
}

// HTTP Response helpers
static void send_http_response(int client_fd, int status_code, const std::string& status_text,
                               const std::string& content_type, const std::string& body) {
    std::ostringstream os;
    os << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
       << "Access-Control-Allow-Headers: Content-Type\r\n"
       << "Connection: close\r\n\r\n"
       << body;
    std::string resp = os.str();
    write(client_fd, resp.data(), resp.size());
}

static void handle_client(int client_fd) {
    char buf[8192];
    ssize_t bytes = read(client_fd, buf, sizeof(buf) - 1);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    buf[bytes] = '\0';
    std::string req(buf, bytes);

    size_t line_end = req.find("\r\n");
    if (line_end == std::string::npos) {
        close(client_fd);
        return;
    }

    std::string first_line = req.substr(0, line_end);
    std::istringstream iss(first_line);
    std::string method, path, proto;
    iss >> method >> path >> proto;

    if (method == "OPTIONS") {
        send_http_response(client_fd, 204, "No Content", "text/plain", "");
        close(client_fd);
        return;
    }

    // Extract body if present
    std::string body;
    size_t body_pos = req.find("\r\n\r\n");
    if (body_pos != std::string::npos) {
        body = req.substr(body_pos + 4);
    }

    // Route: GET /api/bots
    if (method == "GET" && path == "/api/bots") {
        std::string json = "{\"bots\":[";
        json += "{\"id\":\"rule\",\"name\":\"RuleBot\",\"type\":\"rule\",\"description\":\"Rule-based heuristic bot using preflop hand charts and postflop made-hand texture evaluation\"},";
        json += "{\"id\":\"equity\",\"name\":\"EquityBot\",\"type\":\"equity\",\"description\":\"Monte Carlo equity simulator (2000 samples) vs pot odds & narrowing opponent range\"},";
        json += "{\"id\":\"cfr\",\"name\":\"Hold'em (CFR)\",\"type\":\"cfr\",\"description\":\"Counterfactual Regret Minimization equilibrium solver with multiple trained checkpoints\",\"versions\":[";

        // Discover CFR files in versions/
        std::vector<fs::path> cfr_paths;
        if (fs::exists("versions")) {
            for (const auto& entry : fs::directory_iterator("versions")) {
                if (entry.path().extension() == ".cfr") {
                    cfr_paths.push_back(entry.path());
                }
            }
        }
        if (fs::exists("holdem.cfr")) cfr_paths.push_back("holdem.cfr");

        std::sort(cfr_paths.begin(), cfr_paths.end(), [](const fs::path& a, const fs::path& b) {
            return a.filename().string() > b.filename().string();
        });

        for (size_t i = 0; i < cfr_paths.size(); ++i) {
            if (i > 0) json += ",";
            const auto& p = cfr_paths[i];
            auto s = get_strategy(p.string());

            std::string fname = p.filename().string();
            long long iters = s ? s->iterations : 0;
            size_t entries = s ? s->table.size() : 0;

            json += "{";
            json += json_str("id", p.string()) + ",";
            json += json_str("filename", fname) + ",";
            json += json_str("name", fname) + ",";
            json += json_int("iterations", iters) + ",";
            json += json_int("infoSets", entries) + ",";
            json += json_str("raiseMenu", "1x pot, all-in") + ",";
            json += json_str("stack", "100bb");
            json += "}";
        }

        json += "]}]}";
        send_http_response(client_fd, 200, "OK", "application/json", json);
        close(client_fd);
        return;
    }

    // Route: POST /api/game/new
    if (method == "POST" && path == "/api/game/new") {
        std::string bot_type = parse_json_string(body, "bot");
        if (bot_type.empty()) bot_type = "cfr";
        std::string version = parse_json_string(body, "version");
        if (version.empty()) version = "versions/run4.cfr";

        std::string mode = parse_json_string(body, "gameMode");
        if (mode.empty()) mode = "infinite";
        g_session.game_mode = mode;
        g_session.match_over = false;
        g_session.match_winner = "";

        if (mode == "match") {
            g_session.auto_rebuy = false;
            g_session.hero_bankroll = 10000;
            g_session.bot_bankroll = 10000;
            g_session.hand_number = 0;
            g_session.total_chips_won = 0;
            g_session.hands_played = 0;
            g_session.cfg.small_blind = get_match_small_blind(1);
            g_session.cfg.big_blind = get_match_big_blind(1);
        } else {
            g_session.cfg.small_blind = 50;
            g_session.cfg.big_blind = 100;
            if (body.find("autoRebuy") != std::string::npos) {
                g_session.auto_rebuy = parse_json_bool(body, "autoRebuy", false);
            }
            g_session.hero_bankroll = 10000;
            g_session.bot_bankroll = 10000;
            g_session.hand_number = 0;
            g_session.total_chips_won = 0;
            g_session.hands_played = 0;
        }

        g_session.bot_type = bot_type;
        g_session.bot_version = version;

        init_bot();

        // Ensure blinds match the chosen game mode
        if (mode == "match") {
            g_session.cfg.small_blind = get_match_small_blind(1);
            g_session.cfg.big_blind = get_match_big_blind(1);
        } else {
            g_session.cfg.small_blind = 50;
            g_session.cfg.big_blind = 100;
        }

        std::vector<std::string> events;
        deal_new_hand(events);
        run_game_loop(events);

        send_http_response(client_fd, 200, "OK", "application/json", state_to_json(events));
        close(client_fd);
        return;
    }

    // Route: POST /api/game/action
    if (method == "POST" && path == "/api/game/action") {
        if (!g_session.state || g_session.state->is_terminal()) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"No active hand\"}");
            close(client_fd);
            return;
        }

        std::string act_str = parse_json_string(body, "action");
        int amount = static_cast<int>(parse_json_int(body, "amount", 0));

        Action act;
        if (act_str == "fold") act = Action::fold();
        else if (act_str == "check") act = Action::check();
        else if (act_str == "call") act = Action::call();
        else if (act_str == "raise") act = Action::raise(amount);
        else {
            send_http_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"Unknown action\"}");
            close(client_fd);
            return;
        }

        std::vector<std::string> events;
        try {
            g_session.state->apply(act);

            std::string hero_evt = "{";
            hero_evt += json_str("type", "action") + ",";
            hero_evt += json_str("actor", "hero") + ",";
            hero_evt += json_str("action", act.to_string()) + ",";
            hero_evt += json_int("amount", act.amount) + ",";
            hero_evt += json_int("pot", g_session.state->pot());
            hero_evt += "}";
            events.push_back(hero_evt);

            run_game_loop(events);
        } catch (const std::exception& e) {
            send_http_response(client_fd, 400, "Bad Request", "application/json",
                               "{\"error\":\"" + json_escape(e.what()) + "\"}");
            close(client_fd);
            return;
        }

        send_http_response(client_fd, 200, "OK", "application/json", state_to_json(events));
        close(client_fd);
        return;
    }

    // Route: POST /api/game/next
    if (method == "POST" && path == "/api/game/next") {
        if (g_session.game_mode == "match" && g_session.match_over) {
            send_http_response(client_fd, 200, "OK", "application/json", state_to_json({}));
            close(client_fd);
            return;
        }
        if (body.find("autoRebuy") != std::string::npos) {
            g_session.auto_rebuy = parse_json_bool(body, "autoRebuy", g_session.auto_rebuy);
        }
        g_session.hand_number++;
        std::vector<std::string> events;
        deal_new_hand(events);
        run_game_loop(events);

        send_http_response(client_fd, 200, "OK", "application/json", state_to_json(events));
        close(client_fd);
        return;
    }

    // Route: POST /api/game/toggle_rebuy
    if (method == "POST" && path == "/api/game/toggle_rebuy") {
        if (body.find("autoRebuy") != std::string::npos) {
            g_session.auto_rebuy = parse_json_bool(body, "autoRebuy", !g_session.auto_rebuy);
        } else {
            g_session.auto_rebuy = !g_session.auto_rebuy;
        }
        std::string json = "{\"autoRebuy\":" + std::string(g_session.auto_rebuy ? "true" : "false") + "}";
        send_http_response(client_fd, 200, "OK", "application/json", json);
        close(client_fd);
        return;
    }

    // Route: GET /api/game/state
    if (method == "GET" && path == "/api/game/state") {
        if (!g_session.state) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"No active game\"}");
            close(client_fd);
            return;
        }
        send_http_response(client_fd, 200, "OK", "application/json", state_to_json({}));
        close(client_fd);
        return;
    }

    // Unknown endpoint
    send_http_response(client_fd, 404, "Not Found", "application/json", "{\"error\":\"Route not found\"}");
    close(client_fd);
}

int main(int argc, char** argv) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0) port = 8080;
    }

    // Initialize initial default session
    g_session.bot_type = "cfr";
    g_session.bot_version = "versions/run4.cfr";
    init_bot();

    std::vector<std::string> init_events;
    deal_new_hand(init_events);
    run_game_loop(init_events);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << port << "\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "Failed to listen on socket\n";
        close(server_fd);
        return 1;
    }

    std::cout << "[poker_server] Listening on http://127.0.0.1:" << port << "\n";

    for (;;) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;
        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}
