/**
Example (3) in Dancing Links [Knuth00].
*/

#include "../dlx.h"

auto main() -> int
{
    // clang-format off
        auto entries = std::initializer_list<std::pair<size_t, size_t>> {
                                { 1, 3 },           { 1, 5 }, { 1, 6 },
            { 2, 1 },                     { 2, 4 },                     { 2, 7 },
                    { 3, 2 }, { 3, 3 },                     { 3, 6 },
            { 4, 1 },                     { 4, 4 },
                    { 5, 2 },                                         { 5, 7 },
                                        { 6, 4 }, { 6, 5 },           { 6, 7 }
        };
    // clang-format on

    auto solver = DLX::Solver(6, 7, entries);

    std::println("{}", solver.matrix());

    if (auto result = solver.solve()) {
        std::println("Solution: {}", *result);
    } else {
        std::println("No solution.");
    }
}