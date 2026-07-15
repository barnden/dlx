#include "../dlx.h"
#include <print>

auto main() -> int
{
    auto entries = std::vector<std::pair<size_t, size_t>> {
        { 1, 1 }, { 2, 2 }, { 3, 1 }, { 4, 2 }, { 5, 1 }, { 5, 2 }
    };
    auto solver = DLX::Solver::from_entries(entries);

    std::println("{}", solver.matrix());

    for (auto solution : solver.solve()) {
        std::println("{}", solution);
    }
}