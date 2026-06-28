#include "../dlx.h"

auto main() -> int
{
    auto solver = DLX::Solver(1, 1, { { 1, 1 } });

    std::println("{}", solver.matrix());

    if (auto result = solver.solve()) {
        std::println("Solution: {}", *result);
    } else {
        std::println("No solution.");
    }
}