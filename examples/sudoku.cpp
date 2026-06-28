/**
Sudoku Solver via DLX.
For an explanation of the exact cover matrix, see:
  https://www.stolaf.edu/people/hansonr/sudoku/exactcovermatrix.htm
*/

#include <cassert>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <mdspan>
#include <print>
#include <unordered_set>

#include "../dlx.h"

template <typename T>
concept Numeric = std::is_integral_v<T>;

class Sudoku;
namespace std {

template <>
struct formatter<Sudoku> : formatter<string> {
    auto format(auto const& instance, auto& context) const
    {
        return formatter<string>::format(instance.to_string(), context);
    }
};

}

class Sudoku {
    char m_data[81] = { };
    std::mdspan<char, std::extents<size_t, 9, 9>> m_board;

    template <class M>
    class Wrapper {
        std::mdspan<char, typename M::extents_type, typename M::layout_type>
            span;

    public:
        Wrapper(char* data, M map)
            : span(data, map) { };

        template <typename F>
        auto for_each(F&& f) const noexcept
        {
            for (auto i = 0uz; i < span.extent(0); i++) {
                for (auto j = 0uz; j < span.extent(1); j++) {
                    f(span[i, j]);
                }
            }
        }
    };

public:
    Sudoku()
        : m_board(decltype(m_board)(m_data)) { };

    [[nodiscard]] auto block(size_t i, size_t j) noexcept
    {
        assert(i < 3);
        assert(j < 3);

        static constexpr auto map = std::layout_stride::mapping(
            std::extents<size_t, 3, 3> { }, std::array { 9uz, 1uz });

        return Wrapper(m_data + 27 * i + 3 * j, map);
    }

    [[nodiscard]] auto column(size_t i) noexcept
    {
        assert(i < 9);

        static constexpr auto map = std::layout_stride::mapping(
            std::extents<size_t, 9, 1> { }, std::array { 9uz, 1uz });

        return Wrapper(m_data + i, map);
    }

    [[nodiscard]] auto row(size_t i) noexcept
    {
        assert(i < 9);

        static constexpr auto map = std::layout_stride::mapping(
            std::extents<size_t, 1, 9> { }, std::array { 9uz, 1uz });

        return Wrapper(m_data + 9 * i, map);
    }

    [[nodiscard]] auto to_string() const noexcept
    {
        auto result = std::string { };

        for (auto i = 0uz; i < m_board.extent(0); i++) {
            if (i % 3 == 0) {
                result += "+-------+-------+-------+\n";
            }
            for (auto j = 0uz; j < m_board.extent(1); j++) {
                if (j % 3 == 0) {
                    result += "| ";
                }

                if (auto val = m_board[i, j]) {
                    result += std::format("{:<2}", +val);
                } else {
                    result += ". ";
                }
            }

            result += "|\n";
        }
        result += "+-------+-------+-------+\n";

        return result;
    }

    auto solution_to_cell(size_t r) const noexcept
        -> std::tuple<size_t, size_t, int>
    {
        auto row = (r - 1) / 81;
        auto col = ((r - 1) / 9) % 9;
        auto val = ((r - 1) % 9) + 1;

        return { row, col, val };
    }

    auto generate_entries() const noexcept
        -> std::pair<std::unordered_set<size_t>,
                     std::vector<std::pair<size_t, size_t>>>
    {
        auto entries = std::vector<std::pair<size_t, size_t>> { };
        auto mask = std::unordered_set<size_t> { };

        for (auto r = 1; r <= 729; r++) {
            // Each row [1-729] corresponds to a row/column/value triplet.
            // The entries in each row corresponds to the imposed constraints.
            auto const [row, col, val] = solution_to_cell(r);

            if (m_board[row, col] == val) {
                mask.insert(r);
            } else if (m_board[row, col] != 0) {
                continue;
            }

            {
                // Cell Constraints [cols 1-81]
                entries.emplace_back(r, (r - 1) / 9 + 1);
            }

            {
                // Row Constraints [cols 82-162]
                auto offset = 9 * ((r - 1) / 81);
                auto diagonal = ((r - 1) % 9) + 1;

                entries.emplace_back(r, 81 + offset + diagonal);
            }

            {
                // Column Constraints [cols 163-243]
                entries.emplace_back(r, 162 + (r - 1) % 81 + 1);
            }

            {
                // Block Constraints [cols 244-324]
                auto diagonal = (r - 1) % 9 + 1;
                auto offset
                    = 243 + 9 * (((r - 1) / 27) % 3) + 27 * ((r - 1) / 243);

                entries.emplace_back(r, offset + diagonal);
            }
        }

        return { mask, entries };
    }

    auto solve() const noexcept
        -> std::optional<std::vector<std::tuple<size_t, size_t, int>>>
    {
        auto const [mask, entries] = generate_entries();
        auto const solver = DLX::Solver(729, 324, entries);

        if (auto result = solver.solve()) {
            // std::println("Constraints: {}", mask);
            std::erase_if(*result,
                          [&](auto val) { return mask.contains(val); });

            auto solution
                = std::vector<std::tuple<size_t, size_t, int>>(result->size());

            std::transform(result->begin(), result->end(), solution.begin(),
                           [&](auto r) { return solution_to_cell(r); });

            return solution;
        }

        std::println("No solution.");
        return std::nullopt;
    }

    friend auto& operator<<(std::ostream& ostream, Sudoku const& instance)
    {
        ostream << instance.to_string();

        return ostream;
    }

    [[nodiscard]] constexpr auto operator[](size_t i, size_t j) noexcept
        -> char&
    {
        assert(i < 9);
        assert(j < 9);
        return m_board[i, j];
    }

    [[nodiscard]] constexpr auto operator[](size_t i, size_t j) const noexcept
        -> char
    {
        assert(i < 9);
        assert(j < 9);
        return m_board[i, j];
    }

    [[nodiscard]] constexpr auto operator[](size_t i) const noexcept -> char
    {
        assert(i < 81);
        return m_data[i];
    }

    [[nodiscard]] constexpr auto operator[](size_t i) noexcept -> char&
    {
        assert(i < 81);
        return m_data[i];
    }
};

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

    for (auto&& [r, c, val] : entries) {
        board[r, c] = val;
    }

    std::println("{}", board);

    if (auto solution = board.solve()) {
        using std::views::zip, std::views::iota;

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

        for (auto&& [i, val] : zip(iota(0uz), expected)) {
            assert(val == board[i]);
        }

        std::println("Solution:");
        std::println("{}", board);
    } else {
        std::println("No solution found for input board.");
    }
}