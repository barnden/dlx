/**
Sudoku Solver via DLX.
For an explanation of the exact cover matrix, see:
  https://www.stolaf.edu/people/hansonr/sudoku/exactcovermatrix.htm
*/

#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <mdspan>
#include <unordered_set>
#include <vector>

#include "../../dlx.h"

template <typename T>
concept Numeric = std::is_integral_v<T>;

class Sudoku;
namespace std {

template <> struct formatter<Sudoku> : formatter<string> {
    auto format(auto const& instance, auto& context) const
    {
        return formatter<string>::format(instance.to_string(), context);
    }
};

}

class Sudoku {
protected:
    char m_data[81] = { };
    using Cell = std::tuple<size_t, size_t, int>;

    std::mdspan<char, std::extents<size_t, 9, 9>> m_board;

    template <class M> class Wrapper {
        std::mdspan<char, typename M::extents_type, typename M::layout_type>
            span;

    public:
        Wrapper(char* data, M map)
            : span(data, map) { };

        template <typename F> auto for_each(F&& f) const noexcept
        {
            for (auto i = 0uz; i < span.extent(0); i++) {
                for (auto j = 0uz; j < span.extent(1); j++) {
                    f(span[i, j]);
                }
            }
        }
    };

    static auto constexpr matrix_row_to_cell(size_t r) noexcept -> Cell
    {
        auto row = (r - 1) / 81;
        auto col = ((r - 1) / 9) % 9;
        auto val = ((r - 1) % 9) + 1;

        return { row, col, val };
    }

    static auto constexpr cell_to_matrix_row(size_t r, size_t c,
                                             int val) noexcept -> size_t
    {
        return r * 81 + 9 * c + val;
    }

    virtual auto generate_entries() const noexcept
        -> std::pair<std::unordered_set<size_t>,
                     std::vector<std::pair<size_t, size_t>>>
    {
        auto entries = std::vector<std::pair<size_t, size_t>> { };
        auto mask = std::unordered_set<size_t> { };

        for (auto r = 1; r <= 729; r++) {
            // Each row [1-729] corresponds to a row/column/value triplet.
            // The entries in each row corresponds to the imposed constraints.
            auto const [row, col, val] = matrix_row_to_cell(r);

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
                auto offset = 9 * (((r - 1) / 27) % 3) + 27 * ((r - 1) / 243);

                entries.emplace_back(r, 243 + offset + diagonal);
            }
        }

        return { mask, entries };
    }

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

    virtual auto solve() const noexcept -> std::optional<std::vector<Cell>>
    {
        auto const [mask, entries] = generate_entries();
        auto const solver = DLX::Solver::from_entries(entries);

        auto solutions = solver.solve();
        auto it = solutions.begin();

        if (it == std::default_sentinel) {
            return std::nullopt;
        }

        auto result = *it;
        std::erase_if(result, [&](auto val) { return mask.contains(val); });

        auto solution = std::vector<Cell>(result.size());
        std::transform(result.begin(), result.end(), solution.begin(),
                       [&](auto r) { return matrix_row_to_cell(r); });

        return solution;
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

    friend auto& operator<<(std::ostream& ostream, Sudoku const& instance)
    {
        ostream << instance.to_string();

        return ostream;
    }
};
