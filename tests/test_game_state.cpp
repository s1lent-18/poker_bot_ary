#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

#include "core/deck.h"
#include "core/rng.h"
#include "engine/game_state.h"

using namespace poker;

static std::array<Card, 9> cards(const std::string& s) {
    std::array<Card, 9> out;
    std::istringstream is(s);
    std::string tok;
    int i = 0;
    while (is >> tok) {
        auto c = Card::from_string(tok);
        if (!c || i >= 9) throw std::runtime_error("bad deal string");
        out[i++] = *c;
    }
    if (i != 9) throw std::runtime_error("need 9 cards");
    return out;
}

// p0 = AA, p1 = KK, board 2c 7d 9h Js 3d  (p0 wins at showdown). Player 0 is the button.
static const char* P0_WINS = "As Ad Ks Kd 2c 7d 9h Js 3d";
static const char* P1_WINS = "Ks Kd As Ad 2c 7d 9h Js 3d";

static GameState make(const char* deal = P0_WINS, int button = 0) {
    return GameState(GameConfig{}, button, cards(deal));
}

// Everyone checks/calls to the end.
static void passive_to_end(GameState& s) {
    while (!s.is_terminal()) {
        auto legal = s.legal_actions();
        s.apply(legal[0] == Action::fold() ? legal[1] : legal[0]);
    }
}

TEST(Engine, BlindsAndInitialState) {
    auto s = make();
    EXPECT_EQ(s.pot(), 150);
    EXPECT_EQ(s.stack(0), 9950);  // button = small blind
    EXPECT_EQ(s.stack(1), 9900);
    EXPECT_EQ(s.current_player(), 0);  // SB acts first preflop
    EXPECT_EQ(s.street(), Street::Preflop);
    EXPECT_FALSE(s.is_terminal());
}

TEST(Engine, ButtonCanBePlayerOne) {
    auto s = make(P0_WINS, 1);
    EXPECT_EQ(s.stack(1), 9950);
    EXPECT_EQ(s.current_player(), 1);
}

TEST(Engine, RejectsDuplicateCards) {
    EXPECT_THROW(GameState(GameConfig{}, 0, cards("As As Ks Kd 2c 7d 9h Js 3d")),
                 std::invalid_argument);
}

TEST(Engine, PreflopLegalActions) {
    auto s = make();
    std::vector<Action> expect = {Action::fold(), Action::call(), Action::raise(200),
                                  Action::raise(300), Action::raise(10000)};
    EXPECT_EQ(s.legal_actions(), expect);
}

TEST(Engine, SmallBlindFoldLosesBlind) {
    auto s = make();
    s.apply(Action::fold());
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.payoffs(), (std::array<int, 2>{-50, 50}));
    EXPECT_EQ(s.history(), "f");
}

TEST(Engine, LimpCheckGoesToFlopBigBlindActsFirst) {
    auto s = make();
    s.apply(Action::call());
    EXPECT_FALSE(s.is_terminal());
    EXPECT_EQ(s.street(), Street::Preflop);  // BB still has the option
    EXPECT_EQ(s.current_player(), 1);
    s.apply(Action::check());
    EXPECT_EQ(s.street(), Street::Flop);
    EXPECT_EQ(s.current_player(), 1);  // non-button acts first postflop
    EXPECT_EQ(s.pot(), 200);
    EXPECT_EQ(s.board_visible(), 3);
    EXPECT_EQ(s.history(), "ck/");
}

TEST(Engine, FlopLegalActions) {
    auto s = make();
    s.apply(Action::call());
    s.apply(Action::check());
    // each player has 9900 behind on the flop (100 went in preflop)
    std::vector<Action> expect = {Action::check(), Action::raise(100), Action::raise(200),
                                  Action::raise(9900)};
    EXPECT_EQ(s.legal_actions(), expect);
}

TEST(Engine, CheckDownToShowdownWinnerGetsPot) {
    auto a = make(P0_WINS);
    passive_to_end(a);
    EXPECT_EQ(a.street(), Street::Showdown);
    EXPECT_EQ(a.payoffs(), (std::array<int, 2>{100, -100}));

    auto b = make(P1_WINS);
    passive_to_end(b);
    EXPECT_EQ(b.payoffs(), (std::array<int, 2>{-100, 100}));
}

TEST(Engine, BoardPlaysIsATie) {
    GameState s(GameConfig{}, 0, cards("2c 3d 4c 5d Ts Js Qs Ks As"));
    passive_to_end(s);
    EXPECT_EQ(s.payoffs(), (std::array<int, 2>{0, 0}));
}

TEST(Engine, MinRaiseTracksLastRaise) {
    auto s = make();
    s.apply(Action::raise(300));  // raise size 200
    auto legal = s.legal_actions();
    ASSERT_GE(legal.size(), 3u);
    EXPECT_EQ(legal[0], Action::fold());
    EXPECT_EQ(legal[1], Action::call());
    for (size_t i = 2; i < legal.size(); ++i) EXPECT_GE(legal[i].amount, 500);  // 300 + 200
    EXPECT_EQ(legal[2], Action::raise(600));  // 0.5 * (400 + 200 call) + 300
}

TEST(Engine, IllegalActionsThrow) {
    auto s = make();
    EXPECT_THROW(s.apply(Action::check()), std::invalid_argument);   // facing the blind
    EXPECT_THROW(s.apply(Action::raise(150)), std::invalid_argument);  // below min / off-menu
    EXPECT_THROW(s.apply(Action::raise(99999)), std::invalid_argument);
}

TEST(Engine, RaiseCallEndsStreetAndHistoryShowsIt) {
    auto s = make();
    s.apply(Action::raise(300));
    s.apply(Action::call());
    EXPECT_EQ(s.street(), Street::Flop);
    EXPECT_EQ(s.pot(), 600);
    EXPECT_EQ(s.history(), "r300c/");
}

TEST(Engine, AllInAndCallRunsOutBoardImmediately) {
    auto s = make(P0_WINS);
    s.apply(Action::raise(10000));
    // BB can only fold or call an all-in
    EXPECT_EQ(s.legal_actions(), (std::vector<Action>{Action::fold(), Action::call()}));
    s.apply(Action::call());
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.board_visible(), 5);
    EXPECT_EQ(s.payoffs(), (std::array<int, 2>{10000, -10000}));
}

TEST(Engine, FoldToAllIn) {
    auto s = make();
    s.apply(Action::raise(10000));
    s.apply(Action::fold());
    EXPECT_EQ(s.payoffs(), (std::array<int, 2>{100, -100}));  // SB wins the BB
}

TEST(Engine, ObservationHidesOpponentCardsAndFutureBoard) {
    auto s = make();
    auto o0 = s.observe(0), o1 = s.observe(1);
    EXPECT_EQ(o0.hole[0], *Card::from_string("As"));
    EXPECT_EQ(o1.hole[0], *Card::from_string("Ks"));
    EXPECT_EQ(o0.board.size(), 0u);
    EXPECT_EQ(o0.to_call, 50);
    EXPECT_EQ(o1.to_call, 0);
    s.apply(Action::call());
    s.apply(Action::check());
    EXPECT_EQ(s.observe(0).board.size(), 3u);
    EXPECT_EQ(s.observe(0).board[0], *Card::from_string("2c"));
}

TEST(Engine, PayoffsBeforeTerminalThrows) {
    auto s = make();
    EXPECT_THROW(s.payoffs(), std::logic_error);
}

TEST(Engine, RandomPlayoutsConserveChipsAndTerminate) {
    Rng rng(31337);
    Deck deck;
    GameConfig cfg;
    for (int hand = 0; hand < 50000; ++hand) {
        deck.shuffle(rng);
        GameState s(cfg, static_cast<int>(rng.below(2)), deck);
        int steps = 0;
        while (!s.is_terminal()) {
            // chips are conserved at every step
            ASSERT_EQ(s.stack(0) + s.stack(1) + s.pot(), 2 * cfg.stack);
            ASSERT_GE(s.stack(0), 0);
            ASSERT_GE(s.stack(1), 0);
            auto legal = s.legal_actions();
            ASSERT_FALSE(legal.empty());
            s.apply(legal[rng.below(legal.size())]);
            ASSERT_LT(++steps, 200) << "hand did not terminate";
        }
        auto pay = s.payoffs();
        ASSERT_EQ(pay[0] + pay[1], 0);
        ASSERT_LE(std::abs(pay[0]), cfg.stack);
    }
}