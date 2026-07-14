#include "../dlx.h"
#include <print>

auto main() -> int
{
    auto entries = std::vector<std::pair<size_t, size_t>> { { 1, 1 } };
    auto solver = DLX::Solver(1, 1, entries);

    std::println("{}", solver.matrix());

    for (auto&& result : solver.solve()) {
        std::println("Solution: {}", result);
    }
}