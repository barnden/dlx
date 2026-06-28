# DLX
A header only dancing links implementation in C++23 to solve exact cover problems.

## Usage
The assumption is that you have a $(0, 1)-$matrix representation of the exact cover problem (see [Sudoku](https://www.stolaf.edu/people/hansonr/sudoku/exactcovermatrix.htm) and [other examples](https://www.stolaf.edu/people/hansonr/sudoku/exactcover.htm)).

The exact cover matrix is sparse, each entry (or one) in the matrix is represented by a `std::pair<size_t, size_t>` whose value represents the 1-indexed matrix position of the entry.

Given an iterable container (e.g. `std::vector`) of these entries, we construct the matrix and the solver with the `DLX::Solver(size_t row, size_t col, Container&& entries)` constructor.
- The solver instance can also be created using the `DLX::Solver::from_entries(Container&& entries)` class method which infers the `row` and `col` values from the entries.

Then calling `solve()` on the solver instance will yield a `std::optional<std::set<size_t>>`.
- Should no solution be found, it returns a `std::nullopt`.
- Otherwise, it yields a `std::set<size_t>` of rows which satisfy the exact cover.

### Example
```c++
using Entry = std::pair<size_t, size_t>;
auto entries = std::vector<Entry> { /* ... */ };

auto solver = DLX::Solver::from_entries(entries);

if (auto solution = solver.solve()) {
    std::println("{}", *solution);
} else {
    std::println("No solution found.");
}
```

## References
Knuth, Donald E. "Dancing links." [arXiv preprint cs/0011047](https://arxiv.org/abs/cs/0011047) (2000).