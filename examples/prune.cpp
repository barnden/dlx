#include "../dlx.h"
#include <print>

auto main() -> int
{
    auto entries = std::vector<std::pair<size_t, size_t>> {
        { 1, 1 },
        { 2, 2 },
        { 3, 1 },
        { 4, 2 },
    };
    auto solver = DLX::Solver::from_entries(entries);

    std::println("{}", solver.matrix());

    DLX::Solver::Predicate predicate = [&](auto& O) {
        for (auto&& node : O) {
            if (node->index.value_or(0) == 1) {
                return true;
            }
        }

        return false;
    };

    for (auto solution : solver.solve(predicate)) {
        if (!solution.empty())
            std::println("{}", solution);
    }
}