#include "sudoku.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <print>
#include <ranges>
#include <vector>

auto k_subset_sum(int nums, int sum) -> std::vector<std::vector<int>>
{
    std::vector<std::vector<int>> solutions;
    std::vector<int> path;

    if (nums == 1)
        return { { sum } };

    std::function<void(int, int, int)> dfs = [&](int acc, int k, int target) {
        if (k == 0) {
            if (target == 0) {
                solutions.emplace_back(path);
            }

            return;
        }

        if (acc >= 9)
            return;

        if (9 - acc < k)
            return;

        if (target < (k * (2 * acc + k + 1)) / 2)
            return;

        if (target > (k * (19 - k)) / 2)
            return;

        for (auto i = acc + 1; i <= 9; i++) {
            path.push_back(i);
            dfs(i, k - 1, target - i);
            path.pop_back();
        }
    };

    dfs(0, nums, sum);
    return solutions;
}

class KillerSudoku : public Sudoku {
    class Cage {
        std::vector<std::pair<size_t, size_t>> m_cells { };
        int m_sum { };
        std::vector<std::vector<int>> m_subsets { };

    public:
        Cage(std::vector<std::pair<size_t, size_t>> cells, int sum)
            : m_cells(cells)
            , m_sum(sum)
            , m_subsets(k_subset_sum(cells.size(), sum)) { };

        [[nodiscard]] auto cells() const noexcept { return m_cells; }

        [[nodiscard]] auto sum() const noexcept { return m_sum; }

        [[nodiscard]] auto subsets() const noexcept { return m_subsets; }
    };

    std::vector<Cage> m_cages;

    [[nodiscard]] auto generate_entries() const noexcept
        -> std::pair<std::vector<std::tuple<size_t, size_t, size_t>>,
                     std::vector<std::pair<size_t, size_t>>>
    {
        using std::next_permutation;
        using std::views::enumerate;
        using std::views::zip;

        /**
        The Killer Sudoku (KS) exact cover matrix differs from regular Sudoku.

        We reuse the same 324 constraint columns (cell, row, col, block) from
        regular Sudoku. We then add an additional constraint column for each
        cage in the current Killer Sudoku board.

        Instead of each row corresponding to a value assignment of a cell,
        each row in the Killer Sudoku matrix corresponds to a cage assignment.

        To create this matrix, we assume a cage has k cells and target sum T.
        - For each cage
            - Assign an ordinal C corresponding to a unique constraint column.
            - Find all k-subset sums of [1, ..., 9] that sum to T.
            - For each k-subset, assign an ordinal R corresponding to a
              unique matrix row.
                - Add a matrix entry (R, C); this will indicate that the row
                  will satisfy the cage's constraint column.
                - For each cell in the cage, find the constraints from regular
                  Sudoku that are satisfied (C_k1, ..., C_k4)
                    - For each satisfied constraint, add matrix entry (R, C_ki)
        */
        auto rows = std::vector<std::tuple<size_t, size_t, size_t>> { };
        auto entries = std::vector<std::pair<size_t, size_t>> { };
        auto value_of = [&](auto&& cell) {
            return this->operator[](cell.first, cell.second);
        };

        for (auto&& [i, cage] : enumerate(m_cages)) {
            for (auto&& [j, assignment] : enumerate(cage.subsets())) {
                auto permutation = 0;
                do {
                    permutation++;
                    auto product = zip(cage.cells(), assignment);
                    // clang-format off
                    auto is_invalid_assignment = std::ranges::any_of(
                        product,
                        [&](auto&& product) {
                            auto&& [cell, value] = product;

                            if (auto fixed_value = value_of(cell)) {
                                return fixed_value != value;
                            }

                            return false;
                        });
                    // clang-format on

                    if (is_invalid_assignment) {
                        continue;
                    }

                    auto row = rows.size() + 1;

                    // Add entry in the current cage's constraint column
                    // We start at column 325 because 1-324 are for the regular
                    // Sudoku constraints (cell, row, col, box)
                    entries.emplace_back(row, i + 1);
                    for (auto&& [cell, value] : product) {
                        auto&& [r, c] = cell;

                        for (auto&& col : cell_to_matrix_columns(r, c, value)) {
                            // Add entry to corresponding Sudoku constraint
                            // columns
                            entries.emplace_back(row, col + m_cages.size());
                        }
                    }
                    rows.emplace_back(i, j, permutation);
                } while (
                    next_permutation(assignment.begin(), assignment.end()));
            }
        }

        return { rows, entries };
    }

public:
    template <typename... T> auto add_cage(T&&... args)
    {
        m_cages.emplace_back(std::forward<T>(args)...);
    }

    auto solve() const noexcept -> std::optional<std::vector<Cell>> override
    {
        using std::views::transform;
        using std::views::zip;

        auto const [rows, entries] = generate_entries();
        auto const solver = DLX::Solver::from_entries(entries);

        auto solutions = solver.solve();
        auto it = solutions.begin();
        if (it == std::default_sentinel) {
            return std::nullopt;
        }

        auto matrix_row_to_indicies = transform(
            [&](auto&& matrix_row) { return rows.at(matrix_row - 1); });
        auto result = *it | matrix_row_to_indicies;

        auto solution = std::vector<Cell>(result.size());
        for (auto&& [cage_idx, subset_idx, nth_permutation] : result) {
            auto&& cage = m_cages[cage_idx];
            auto&& cells = cage.cells();
            auto assignment = cage.subsets()[subset_idx];

            auto permutation = 0uz;
            do {
                if (++permutation == nth_permutation) {
                    break;
                }
            } while (
                std::next_permutation(assignment.begin(), assignment.end()));

            for (auto&& [cell, value] : zip(cells, assignment)) {
                solution.emplace_back(cell.first, cell.second, value);
            }
        }

        return solution;
    }
};

auto main() -> int
{
    auto board = KillerSudoku();
    auto cages
        = std::vector<std::pair<std::vector<std::pair<size_t, size_t>>, int>> {
              { { { 0, 0 }, { 1, 0 } }, 9 },
              { { { 0, 1 }, { 0, 2 }, { 0, 3 } }, 16 },
              { { { 0, 4 }, { 1, 4 } }, 3 },
              { { { 0, 5 }, { 1, 5 } }, 12 },
              { { { 0, 6 }, { 1, 6 } }, 8 },
              { { { 0, 7 }, { 0, 8 }, { 1, 7 } }, 18 },
              { { { 1, 1 }, { 2, 0 }, { 2, 1 } }, 16 },
              { { { 1, 2 }, { 1, 3 } }, 15 },
              { { { 1, 8 }, { 2, 8 } }, 9 },
              { { { 2, 2 }, { 3, 2 }, { 3, 3 }, { 3, 4 } }, 10 },
              { { { 2, 3 }, { 2, 4 } }, 14 },
              { { { 2, 5 }, { 3, 5 } }, 5 },
              { { { 2, 6 }, { 2, 7 } }, 10 },
              { { { 3, 0 }, { 3, 1 } }, 16 },
              { { { 3, 6 }, { 4, 6 }, { 4, 7 } }, 9 },
              { { { 3, 7 }, { 3, 8 } }, 13 },
              { { { 4, 0 }, { 4, 1 }, { 4, 2 } }, 16 },
              { { { 4, 3 }, { 4, 4 }, { 5, 4 } }, 22 },
              { { { 4, 5 }, { 5, 5 }, { 5, 6 } }, 19 },
              { { { 5, 0 }, { 6, 0 }, { 7, 0 }, { 8, 0 } }, 21 },
              { { { 5, 1 }, { 5, 2 }, { 5, 3 } }, 8 },
              { { { 4, 8 }, { 5, 7 }, { 5, 8 } }, 16 },
              { { { 6, 1 }, { 6, 2 }, { 7, 1 } }, 14 },
              { { { 6, 3 }, { 7, 2 }, { 7, 3 } }, 18 },
              { { { 6, 4 }, { 6, 5 }, { 6, 6 } }, 17 },
              { { { 6, 7 }, { 7, 7 } }, 7 },
              { { { 6, 8 }, { 7, 8 } }, 7 },
              { { { 8, 1 }, { 8, 2 } }, 9 },
              { { { 8, 3 }, { 8, 4 } }, 7 },
              { { { 7, 4 }, { 7, 5 } }, 13 },
              { { { 7, 6 }, { 8, 6 }, { 8, 5 } }, 18 },
              { { { 8, 7 }, { 8, 8 } }, 10 },
          };

    for (auto&& [cells, target_sum] : cages) {
        board.add_cage(cells, target_sum);
    }

    std::println("Input:");
    std::println("{}", board.to_string());

    if (auto solution = board.solve()) {
        using std::views::enumerate;

        for (auto&& [r, c, val] : *solution) {
            board[r, c] = val;
        }

        // Check that the subset sum constraint is satisfied
        // NOTE: This is redundant as we also check for an exact answer below.
        for (auto&& [cells, target_sum] : cages) {
            auto partial_sum = 0;
            for (auto&& [r, c] : cells) {
                partial_sum += board[r, c];
            }

            if (partial_sum != target_sum) {
                std::println(
                    std::cerr,
                    "Cage sum not equal to target. Expected {} got {}.",
                    target_sum, partial_sum);

                return -1;
            }
        }

        // clang-format off
        auto expected = {
            1, 3, 9, 4, 2, 7, 5, 6, 8,
            8, 7, 6, 9, 1, 5, 3, 4, 2,
            5, 4, 2, 6, 8, 3, 1, 9, 7,
            7, 9, 4, 1, 3, 2, 6, 8, 5,
            3, 8, 5, 7, 6, 4, 2, 1, 9,
            6, 2, 1, 5, 9, 8, 7, 3, 4,
            4, 5, 3, 8, 7, 1, 9, 2, 6,
            2, 6, 7, 3, 4, 9, 8, 5, 1,
            9, 1, 8, 2, 5, 6, 4, 7, 3,
        };
        // clang-format on

        for (auto&& [i, val] : enumerate(expected)) {
            if (val != board[i]) {
                std::println(std::cerr, "Invalid solution.");
                return -1;
            }
        }

        std::println("Solution:");
        std::println("{}", board.to_string());
    } else {
        std::println("No solution found for input board.");
    }
}