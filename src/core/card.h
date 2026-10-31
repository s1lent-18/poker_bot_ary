#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace poker {

constexpr int NUM_CARDS = 52;
constexpr int NUM_RANKS = 13;
constexpr int NUM_SUITS = 4;

// Card id is 0..51.  rank = id / 4 (0='2' ... 12='A'),  suit = id % 4 (c,d,h,s)
struct Card {
    uint8_t id = 0;

    constexpr Card() = default;
    constexpr explicit Card(uint8_t id_) : id(id_) {}
    constexpr Card(int rank, int suit) : id(static_cast<uint8_t>(rank * NUM_SUITS + suit)) {}

    constexpr int rank() const { return id / NUM_SUITS; }
    constexpr int suit() const { return id % NUM_SUITS; }

    constexpr bool operator==(const Card& o) const { return id == o.id; }
    constexpr bool operator!=(const Card& o) const { return id != o.id; }
    constexpr bool operator<(const Card& o) const { return id < o.id; }

    // "As", "Td", "2c" ...
    std::string to_string() const {
        static constexpr char RANKS[] = "23456789TJQKA";
        static constexpr char SUITS[] = "cdhs";
        return std::string{RANKS[rank()], SUITS[suit()]};
    }

    // Parses "As", "td", "9H" ... returns nullopt if invalid.
    static std::optional<Card> from_string(std::string_view s) 
    {
        if (s.size() != 2) return std::nullopt;
        int r = rank_from_char(s[0]);
        int su = suit_from_char(s[1]);
        if (r < 0 || su < 0) return std::nullopt;
        return Card(r, su);
    }

private:
    static constexpr int rank_from_char(char c) 
    {
        constexpr char RANKS[] = "23456789TJQKA";
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        for (int i = 0; i < NUM_RANKS; ++i)
            if (RANKS[i] == c) return i;
        return -1;
    }
    static constexpr int suit_from_char(char c) 
    {
        constexpr char SUITS[] = "cdhs";
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        for (int i = 0; i < NUM_SUITS; ++i)
            if (SUITS[i] == c) return i;
        return -1;
    }
};

}  // namespace poker