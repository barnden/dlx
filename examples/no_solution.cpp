#include "../dlx.h"
#include <print>

auto main() -> int
{
    auto entries = std::vector<std::pair<size_t, size_t>> { { 1, 1 },
                                                            { 2, 1 },
                                                            { 3, 1 } };
    auto solver = DLX::Solver(3, 2, entries);

    std::println("{}", solver.matrix());

    auto solutions = solver.solve();
    auto it = solutions.begin();

    if (it == std::default_sentinel) {
        std::println("No solution.");
    } else {
        std::println("Solution: {}", *it);
    }
}