#include "../dlx.h"
#include <print>
#include <vector>

auto main() -> int
{
    auto entries = std::vector<std::pair<size_t, size_t>> { };

    for (auto i = 1; i <= 512; i++) {
        entries.emplace_back(i, i);
    }

    auto solver = DLX::Solver::from_entries(entries);

    for (auto&& result : solver.solve()) {
        std::println("Solution: {}", result);
    }
}