#pragma once

#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"

#include <array>
#include <iostream>
#include <string>

#include "attacks.h"
#include "helper.h"
#include "nnue.h"
#include "tt.h"
#include "types.h"

extern TranspositionTable TTable;

// *******************
// CASTLING
// *******************
class BitField16 {
   public:
    BitField16() : value_(0) {}

    // Sets the value of the specified group to the given value
    void setGroupValue(uint16_t group_index, uint16_t group_value) {
        assert(group_value < 16 && "group_value must be less than 16");
        assert(group_index < 4 && "group_index must be less than 4");

        // calculate the bit position of the start of the group you want to set
        uint16_t startBit = group_index * group_size_;
        uint16_t setMask = static_cast<uint16_t>(group_value << startBit);

        // clear the bits in the group
        value_ &= ~(0xF << startBit);

        // set the bits in the group
        value_ |= setMask;
    }

    uint16_t getGroup(uint16_t group_index) const {
        assert(group_index < 4 && "group_index must be less than 4");
        uint16_t startBit = group_index * group_size_;
        return (value_ >> startBit) & 0xF;
    }

    void clear() { value_ = 0; }
    uint16_t get() const { return value_; }

   private:
    static constexpr uint16_t group_size_ = 4;  // size of each group
    uint16_t value_;
};

enum class CastleSide : uint8_t { KING_SIDE, QUEEN_SIDE };

class CastlingRights {
   public:
    template <Color color, CastleSide castle, File rook_file>
    void setCastlingRight() {
        int file = static_cast<uint16_t>(rook_file) + 1;

        castling_rights.setGroupValue(2 * static_cast<int>(color) + static_cast<int>(castle),
                                      static_cast<uint16_t>(file));
    }

    void setCastlingRight(Color color, CastleSide castle, File rook_file) {
        int file = static_cast<uint16_t>(rook_file) + 1;

        castling_rights.setGroupValue(2 * static_cast<int>(color) + static_cast<int>(castle),
                                      static_cast<uint16_t>(file));
    }

    void clearAllCastlingRights() { castling_rights.clear(); }

    void clearCastlingRight(Color color, CastleSide castle) {
        castling_rights.setGroupValue(2 * static_cast<int>(color) + static_cast<int>(castle), 0);
    }

    void clearCastlingRight(Color color) {
        castling_rights.setGroupValue(2 * static_cast<int>(color), 0);
        castling_rights.setGroupValue(2 * static_cast<int>(color) + 1, 0);
    }

    bool isEmpty() const { return castling_rights.get() == 0; }

    bool hasCastlingRight(Color color) const {
        return castling_rights.getGroup(2 * static_cast<int>(color)) != 0 ||
               castling_rights.getGroup(2 * static_cast<int>(color) + 1) != 0;
    }

    bool hasCastlingRight(Color color, CastleSide castle) const {
        return castling_rights.getGroup(2 * static_cast<int>(color) + static_cast<int>(castle)) !=
               0;
    }

    File getRookFile(Color color, CastleSide castle) const {
        assert(hasCastlingRight(color, castle) && "Castling right does not exist");
        return static_cast<File>(
            castling_rights.getGroup(2 * static_cast<int>(color) + static_cast<int>(castle)) - 1);
    }

    int getHashIndex() const {
        return hasCastlingRight(White, CastleSide::KING_SIDE) +
               2 * hasCastlingRight(White, CastleSide::QUEEN_SIDE) +
               4 * hasCastlingRight(Black, CastleSide::KING_SIDE) +
               8 * hasCastlingRight(Black, CastleSide::QUEEN_SIDE);
    }

   private:
    /*
     denotes the file of the rook that we castle to
     1248 1248 1248 1248
     0000 0000 0000 0000
     bq   bk   wq   wk
     3    2    1    0
     */
    BitField16 castling_rights;
};

struct State {
    Square en_passant{};
    CastlingRights castling{};
    uint8_t half_move{};
    Piece captured_piece = None;
    State(Square enpassant_copy = {}, CastlingRights castling_rights_copy = {},
          uint8_t half_move_copy = {}, Piece captured_piece_copy = None)
        : en_passant(enpassant_copy),
          castling(castling_rights_copy),
          half_move(half_move_copy),
          captured_piece(captured_piece_copy) {}
};

class Board {
   public:
    bool chess960 = false;

    Color side_to_move;

    // NO_SQ when enpassant is not possible
    Square en_passant_square;

    CastlingRights castling_rights;

    // halfmoves start at 0
    uint8_t half_move_clock;

    // full moves start at 1
    uint16_t full_move_number;

    // keeps track of previous hashes, used for
    // repetition detection
    std::vector<U64> hash_history;

    // current hashkey
    U64 hash_key;

    std::vector<State> state_history;

    U64 pieces_bb[12] = {};
    std::array<Piece, MAX_SQ> board;

    /// @brief constructor for the board, loads startpos
    Board();

    std::string getCastleString() const;

    /// @brief reload the entire nnue
    void refresh();

    /// @brief Finds what piece is on the square using the board (more performant)
    /// @param sq
    /// @return found piece otherwise None
    Piece pieceAtB(Square sq) const;

    /// @brief applys a new Fen to the board and also reload the entire nnue
    /// @param fen
    /// @param updateAcc
    void applyFen(const std::string &fen, bool updateAcc = true);

    /// @brief returns a Fen string of the current board
    /// @return fen string
    std::string getFen() const;

    /// @brief detects if the position is a repetition by default 1, fide would be 3
    /// @param draw
    /// @return true for repetition otherwise false
    bool isRepetition(int draw = 1) const;

    Result isDrawn(bool inCheck);

    /// @brief only pawns + king = true else false
    /// @param c
    /// @return
    bool nonPawnMat(Color c) const;

    Square kingSQ(Color c) const;

    U64 enemy(Color c) const;

    U64 Us(Color c) const;
    template <Color c>
    U64 us() const {
        return pieces_bb[PAWN + c * 6] | pieces_bb[KNIGHT + c * 6] | pieces_bb[BISHOP + c * 6] |
               pieces_bb[ROOK + c * 6] | pieces_bb[QUEEN + c * 6] | pieces_bb[KING + c * 6];
    }

    U64 all() const;

    // Gets individual piece bitboards

    template <Piece p>
    constexpr U64 pieces() const {
        return pieces_bb[p];
    }

    template <PieceType p, Color c>
    constexpr U64 pieces() const {
        return pieces_bb[p + c * 6];
    }

    constexpr U64 pieces(PieceType p, Color c) const { return pieces_bb[p + c * 6]; }

    /// @brief returns the color of a piece at a square
    /// @param loc
    /// @return
    Color colorOf(Square loc) const;

    /// @brief
    /// @param c
    /// @param sq
    /// @param occ
    /// @return
    bool isSquareAttacked(Color c, Square sq, U64 occ) const;

    // attackers used for SEE
    U64 allAttackers(Square sq, U64 occupiedBB);
    U64 attackersForSide(Color attackerColor, Square sq, U64 occupiedBB);

    void updateHash(Move move);

    /// @brief plays the move on the internal board
    /// @tparam updateNNUE update true = update nnue
    /// @param move
    template <bool updateNNUE>
    void makeMove(Move move);

    /// @brief unmake a move played on the internal board
    /// @tparam updateNNUE update true = update nnue
    /// @param move
    template <bool updateNNUE>
    void unmakeMove(Move move);

    /// @brief make a nullmove
    void makeNullMove();

    /// @brief unmake a nullmove
    void unmakeNullMove();

    const NNUE::accumulator &getAccumulator() const;

    // update the internal board representation

    /// @brief Remove a Piece from the board
    /// @tparam update true = update nnue
    /// @param piece
    /// @param sq
    template <bool updateNNUE>
    void removePiece(Piece piece, Square sq, Square kSQ_White = SQ_A1, Square kSQ_Black = SQ_A1);

    /// @brief Place a Piece on the board
    /// @tparam update
    /// @param piece
    /// @param sq
    template <bool updateNNUE>
    void placePiece(Piece piece, Square sq, Square kSQ_White = SQ_A1, Square kSQ_Black = SQ_A1);

    /// @brief Move a piece on the board
    /// @tparam updateNNUE
    /// @param piece
    /// @param fromSq
    /// @param toSq
    template <bool updateNNUE>
    void movePiece(Piece piece, Square fromSq, Square toSq, Square kSQ_White = SQ_A1,
                   Square kSQ_Black = SQ_A1);

    U64 attacksByPiece(PieceType pt, Square sq, Color c, U64 occupied);

    /********************
     * Static Exchange Evaluation, logical based on Weiss (https://github.com/TerjeKir/weiss)
     *licensed under GPL-3.0
     *******************/
    bool see(Move move, int threshold);

    void clearStacks();

    friend std::ostream &operator<<(std::ostream &os, const Board &b);

    /// @brief calculate the current zobrist hash from scratch
    /// @return
    U64 zobristHash() const;

   private:
    /// @brief current accumulator
    NNUE::accumulator accumulator;

    /// @brief previous accumulators
    std::vector<NNUE::accumulator> accumulatorStack;

    // update the hash

    U64 updateKeyPiece(Piece piece, Square sq) const;
    U64 updateKeyCastling() const;
    U64 updateKeyEnPassant(Square sq) const;
    U64 updateKeySideToMove() const;
};

template <bool updateNNUE>
void Board::removePiece(Piece piece, Square sq, Square kSQ_White, Square kSQ_Black) {
    pieces_bb[piece] &= ~(1ULL << sq);
    board[sq] = None;

    if constexpr (updateNNUE) {
        NNUE::deactivate(accumulator, sq, piece, kSQ_White, kSQ_Black);
    }
}

template <bool updateNNUE>
void Board::placePiece(Piece piece, Square sq, Square kSQ_White, Square kSQ_Black) {
    pieces_bb[piece] |= (1ULL << sq);
    board[sq] = piece;

    if constexpr (updateNNUE) {
        NNUE::activate(accumulator, sq, piece, kSQ_White, kSQ_Black);
    }
}

template <bool updateNNUE>
void Board::movePiece(Piece piece, Square fromSq, Square toSq, Square kSQ_White, Square kSQ_Black) {
    pieces_bb[piece] &= ~(1ULL << fromSq);
    pieces_bb[piece] |= (1ULL << toSq);
    board[fromSq] = None;
    board[toSq] = piece;

    if constexpr (updateNNUE) {
        if (type_of_piece(piece) == KING && NNUE::KING_BUCKET[fromSq] != NNUE::KING_BUCKET[toSq]) {
            refresh();
        } else {
            NNUE::move(accumulator, fromSq, toSq, piece, kSQ_White, kSQ_Black);
        }
    }
}

inline void Board::updateHash(Move move) {
    PieceType pt = type_of_piece(pieceAtB(from(move)));
    Piece p = makePiece(pt, side_to_move);
    Square from_sq = from(move);
    Square to_sq = to(move);
    Piece capture = board[to_sq];
    const auto rank = square_rank(to_sq);

    hash_history.emplace_back(hash_key);

    if (en_passant_square != NO_SQ) hash_key ^= updateKeyEnPassant(en_passant_square);

    hash_key ^= updateKeyCastling();

    en_passant_square = NO_SQ;

    if (pt == KING) {
        castling_rights.clearCastlingRight(side_to_move);

        if (typeOf(move) == CASTLING) {
            const Piece rook = side_to_move == White ? WhiteRook : BlackRook;
            const Square rookSQ =
                file_rank_square(to_sq > from_sq ? FILE_F : FILE_D, square_rank(from_sq));
            const Square kingToSq =
                file_rank_square(to_sq > from_sq ? FILE_G : FILE_C, square_rank(from_sq));

            assert(type_of_piece(pieceAtB(to_sq)) == ROOK);

            hash_key ^= updateKeyPiece(rook, to_sq);
            hash_key ^= updateKeyPiece(rook, rookSQ);
            hash_key ^= updateKeyPiece(p, from_sq);
            hash_key ^= updateKeyPiece(p, kingToSq);

            hash_key ^= updateKeySideToMove();
            hash_key ^= updateKeyCastling();

            return;
        }
    } else if (pt == ROOK && ((square_rank(from_sq) == Rank::RANK_8 && side_to_move == Black) ||
                              (square_rank(from_sq) == Rank::RANK_1 && side_to_move == White))) {
        const auto king_sq = builtin::lsb(pieces(KING, side_to_move));

        castling_rights.clearCastlingRight(
            side_to_move, from_sq > king_sq ? CastleSide::KING_SIDE : CastleSide::QUEEN_SIDE);
    } else if (pt == PAWN) {
        half_move_clock = 0;
        if (typeOf(move) == ENPASSANT) {
            hash_key ^= updateKeyPiece(makePiece(PAWN, ~side_to_move), Square(to_sq ^ 8));
        } else if (std::abs(from_sq - to_sq) == 16) {
            U64 epMask = Attacks::Pawn(Square(to_sq ^ 8), side_to_move);
            if (epMask & pieces(PAWN, ~side_to_move)) {
                en_passant_square = Square(to_sq ^ 8);
                hash_key ^= updateKeyEnPassant(en_passant_square);

                assert(pieceAtB(en_passant_square) == None);
            }
        }
    }

    if (capture != None) {
        half_move_clock = 0;
        hash_key ^= updateKeyPiece(capture, to_sq);
        if (type_of_piece(capture) == ROOK && ((rank == Rank::RANK_1 && side_to_move == Black) ||
                                               (rank == Rank::RANK_8 && side_to_move == White))) {
            const auto king_sq = builtin::lsb(pieces(KING, ~side_to_move));

            castling_rights.clearCastlingRight(
                ~side_to_move, to_sq > king_sq ? CastleSide::KING_SIDE : CastleSide::QUEEN_SIDE);
        }
    }

    if (typeOf(move) == PROMOTION) {
        half_move_clock = 0;

        hash_key ^= updateKeyPiece(makePiece(PAWN, side_to_move), from_sq);
        hash_key ^= updateKeyPiece(makePiece(promotionType(move), side_to_move), to_sq);
    } else {
        hash_key ^= updateKeyPiece(p, from_sq);
        hash_key ^= updateKeyPiece(p, to_sq);
    }

    hash_key ^= updateKeySideToMove();
    hash_key ^= updateKeyCastling();
}

/// @brief
/// @tparam updateNNUE
/// @param move
template <bool updateNNUE>
void Board::makeMove(Move move) {
    PieceType pt = type_of_piece(pieceAtB(from(move)));
    Piece p = pieceAtB(from(move));
    Square from_sq = from(move);
    Square to_sq = to(move);
    Piece capture = board[to_sq];

    assert(from_sq >= 0 && from_sq < 64);
    assert(to_sq >= 0 && to_sq < 64);
    assert(type_of_piece(capture) != KING);
    assert(pt != NONETYPE);
    assert(p != None);
    assert((typeOf(move) == PROMOTION &&
            (promotionType(move) != PAWN && promotionType(move) != KING)) ||
           typeOf(move) != PROMOTION);

    // *****************************
    // STORE STATE HISTORY
    // *****************************

    state_history.emplace_back(en_passant_square, castling_rights, half_move_clock, capture);

    if constexpr (updateNNUE) accumulatorStack.emplace_back(accumulator);

    half_move_clock++;
    full_move_number++;

    const bool ep = to_sq == en_passant_square;

    // Castling is encoded as king captures rook

    // *****************************
    // UPDATE HASH
    // *****************************

    updateHash(move);

    TTable.prefetch(hash_key);

    const Square kSQ_White = builtin::lsb(pieces<KING, White>());
    const Square kSQ_Black = builtin::lsb(pieces<KING, Black>());

    // *****************************
    // UPDATE PIECES AND NNUE
    // *****************************

    if (typeOf(move) == CASTLING) {
        const Piece rook = side_to_move == White ? WhiteRook : BlackRook;
        Square rookToSq = file_rank_square(to_sq > from_sq ? FILE_F : FILE_D, square_rank(from_sq));
        Square kingToSq = file_rank_square(to_sq > from_sq ? FILE_G : FILE_C, square_rank(from_sq));

        if (updateNNUE && NNUE::KING_BUCKET[from_sq] != NNUE::KING_BUCKET[kingToSq]) {
            removePiece<false>(p, from_sq, kSQ_White, kSQ_Black);
            removePiece<false>(rook, to_sq, kSQ_White, kSQ_Black);

            placePiece<false>(p, kingToSq, kSQ_White, kSQ_Black);
            placePiece<false>(rook, rookToSq, kSQ_White, kSQ_Black);

            refresh();
        } else {
            removePiece<updateNNUE>(p, from_sq, kSQ_White, kSQ_Black);
            removePiece<updateNNUE>(rook, to_sq, kSQ_White, kSQ_Black);

            placePiece<updateNNUE>(p, kingToSq, kSQ_White, kSQ_Black);
            placePiece<updateNNUE>(rook, rookToSq, kSQ_White, kSQ_Black);
        }

        side_to_move = ~side_to_move;

        return;
    } else if (pt == PAWN && ep) {
        assert(pieceAtB(Square(to_sq ^ 8)) != None);

        removePiece<updateNNUE>(makePiece(PAWN, ~side_to_move), Square(to_sq ^ 8), kSQ_White,
                                kSQ_Black);
    } else if (capture != None) {
        assert(pieceAtB(to_sq) != None);

        removePiece<updateNNUE>(capture, to_sq, kSQ_White, kSQ_Black);
    }

    // The move is differently encoded for promotions to it requires some special care.
    if (typeOf(move) == PROMOTION) {
        assert(pieceAtB(to_sq) == None);

        removePiece<updateNNUE>(makePiece(PAWN, side_to_move), from_sq, kSQ_White, kSQ_Black);
        placePiece<updateNNUE>(makePiece(promotionType(move), side_to_move), to_sq, kSQ_White,
                               kSQ_Black);
    } else {
        assert(pieceAtB(to_sq) == None);

        movePiece<updateNNUE>(p, from_sq, to_sq, kSQ_White, kSQ_Black);
    }

    side_to_move = ~side_to_move;
}

template <bool updateNNUE>
void Board::unmakeMove(Move move) {
    const State restore = state_history.back();
    state_history.pop_back();

    if (accumulatorStack.size()) {
        accumulator = accumulatorStack.back();
        accumulatorStack.pop_back();
    }

    hash_key = hash_history.back();
    hash_history.pop_back();

    en_passant_square = restore.en_passant;
    castling_rights = restore.castling;
    half_move_clock = restore.half_move;
    Piece capture = restore.captured_piece;

    full_move_number--;

    Square from_sq = from(move);
    Square to_sq = to(move);
    bool promotion = typeOf(move) == PROMOTION;

    side_to_move = ~side_to_move;
    PieceType pt = type_of_piece(pieceAtB(to_sq));
    Piece p = makePiece(pt, side_to_move);

    if (typeOf(move) == CASTLING) {
        Square rookToSq = to_sq;
        Piece rook = side_to_move == White ? WhiteRook : BlackRook;
        Square rookFromSq =
            file_rank_square(to_sq > from_sq ? FILE_F : FILE_D, square_rank(from_sq));
        to_sq = file_rank_square(to_sq > from_sq ? FILE_G : FILE_C, square_rank(from_sq));

        p = makePiece(KING, side_to_move);
        // We need to remove both pieces first and then place them back.
        removePiece<updateNNUE>(rook, rookFromSq);
        removePiece<updateNNUE>(p, to_sq);

        placePiece<updateNNUE>(p, from_sq);
        placePiece<updateNNUE>(rook, rookToSq);

        return;
    } else if (promotion) {
        removePiece<updateNNUE>(makePiece(promotionType(move), side_to_move), to_sq);
        placePiece<updateNNUE>(makePiece(PAWN, side_to_move), from_sq);
        if (capture != None) placePiece<updateNNUE>(capture, to_sq);

        return;
    } else {
        movePiece<updateNNUE>(p, to_sq, from_sq);
    }

    if (to_sq == en_passant_square && pt == PAWN) {
        placePiece<updateNNUE>(makePiece(PAWN, ~side_to_move), Square(en_passant_square ^ 8));
    } else if (capture != None) {
        placePiece<updateNNUE>(capture, to_sq);
    }
}

/// @brief get uci representation of a move
/// @param board
/// @param move
/// @return
std::string uciMove(Move move, bool chess960);

Square extractSquare(std::string_view squareStr);

/// @brief convert console input to move
/// @param board
/// @param input
/// @return
Move convertUciToMove(const Board &board, const std::string &fen);