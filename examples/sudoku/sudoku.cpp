#include "sudoku.h"

#include <cstring>
#include <initializer_list>
#include <iostream>
#include <print>

auto main() -> int
{
    // IMPORTANT: The Sudoku class uses 0-indexing, whereas the DLX solver uses
    //            1-indexing.
    auto board = Sudoku();

    // 1. Define the starting board
    auto entries = std::initializer_list<std::tuple<size_t, size_t, size_t>> {
        // Example Sudoku board with 17 clues (minimum clues for unique sol'n).
        // clang-format off
        { 0, 4, 4 }, { 0, 7, 5 }, { 0, 8, 8 },  // . . . | . 4 . | . 5 8
        { 1, 1, 2 }, { 1, 5, 7 },               // . 2 . | . . 7 | . . .
                                                // . . . | . . . | . . .
        { 3, 0, 5 }, { 3, 2, 4 }, { 3, 7, 6 },  // 5 . 4 | . . . | . 6 .
        { 4, 2, 8 }, { 4, 3, 2 },               // . . 8 | 2 . . | . . .
        { 5, 5, 3 }, { 5, 6, 7 },               // . . . | . . 3 | 7 . .
        { 6, 1, 3 }, { 6, 6, 2 },               // . 3 . | . . . | 2 . .
        { 7, 0, 4 }, { 7, 4, 5 },               // 4 . . | . 5 . | . . .
        { 8, 3, 1 },                            // . . . | 1 . . | . . .
        // clang-format on
    };

    // 2. Fill in the clues on the board
    for (auto&& [r, c, val] : entries) {
        board[r, c] = val;
    }

    std::println("Input:");
    std::println("{}", board);

    if (auto solution = board.solve()) {
        using std::views::enumerate;

        // clang-format off
        auto expected = {
            7, 1, 3, 6, 4, 2, 9, 5, 8,          // 7 1 3 | 6 4 2 | 9 5 8
            8, 2, 5, 9, 1, 7, 4, 3, 6,          // 8 2 5 | 9 1 7 | 4 3 6
            6, 4, 9, 8, 3, 5, 1, 2, 7,          // 6 4 9 | 8 3 5 | 1 2 7
            5, 9, 4, 7, 8, 1, 3, 6, 2,          // 5 9 4 | 7 8 1 | 3 6 2
            3, 7, 8, 2, 6, 4, 5, 1, 9,          // 3 7 8 | 2 6 4 | 5 1 9
            2, 6, 1, 5, 9, 3, 7, 8, 4,          // 2 6 1 | 5 9 3 | 7 8 4
            1, 3, 6, 4, 7, 8, 2, 9, 5,          // 1 3 6 | 4 7 8 | 2 9 5
            4, 8, 2, 3, 5, 9, 6, 7, 1,          // 4 8 2 | 3 5 9 | 6 7 1
            9, 5, 7, 1, 2, 6, 8, 4, 3,          // 9 5 7 | 1 2 6 | 8 4 3
        };
        // clang-format on

        for (auto&& [r, c, val] : *solution) {
            board[r, c] = val;
        }

        for (auto&& [i, val] : enumerate(expected)) {
            if (val != board[i]) {
                std::println(std::cerr, "Invalid solution.");
                return -1;
            }
        }

        std::println("Solution:");
        std::println("{}", board);
    } else {
        std::println("No solution found for input board.");
    }
}