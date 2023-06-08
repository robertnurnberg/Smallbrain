#pragma once

#include <vector>

#include "types.h"

PACK(struct TEntry {
    U64 key = 0;
    Score score = 0;
    Move move = NO_MOVE;
    uint8_t depth = 0;
    Flag flag = NONEBOUND;
});

class TranspositionTable {
   private:
    std::vector<TEntry> entries_;

    // 57344 MiB = 2^32 * 14B / (1024 * 1024)
    static constexpr uint64_t MAXHASH = (1ull << 32) * sizeof(TEntry) / (1024 * 1024);

   public:
    TranspositionTable();

    /// @brief store an entry in the TT
    /// @param depth
    /// @param bestvalue
    /// @param b Type of bound
    /// @param key Position hash
    /// @param move
    void store(int depth, Score bestvalue, Flag b, U64 key, Move move);

    /// @brief probe the TT for an entry
    /// @param tte
    /// @param tt_hit
    /// @param key Position hash
    TEntry *probe(bool &tt_hit, Move &ttmove, U64 key);

    /// @brief calculates the TT index of key
    /// @param key
    /// @return
    uint32_t index(U64 key) const;

    /// @brief allocate Transposition Table and initialize entries_
    void allocate(uint64_t size);

    void allocateMB(uint64_t size_mb);

    /// @brief clear the TT
    void clear();

    void prefetch(uint64_t key) const;

    int hashfull() const;
};