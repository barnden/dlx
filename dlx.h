#include <cassert>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <stack>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <generator>
static_assert(__cpp_lib_generator >= 202207L,
              "missing std::generator from <generator>");

namespace DLX {

template <typename T> using limits = std::numeric_limits<T>;

class Matrix;
}
namespace std {
template <> struct formatter<DLX::Matrix> : formatter<string> {
    auto format(auto const& instance, auto& context) const
    {
        return formatter<string>::format(instance.to_string(), context);
    }
};
}

namespace DLX {

template <typename C, typename T>
concept Container = std::ranges::input_range<C>
                    && std::same_as<std::ranges::range_value_t<C>, T>;

class Solver;
class Matrix {
public:
    struct Node {
        /**
        - The (0, 1)-matrix A representing an exact cover problem is stored as a
          sparse matrix. Each `1` in the matrix is represented by an entry.
        - Entries in the matrix are stored as nodes containing the fields
          - left
          - right
          - up
          - down
          - column
          - (optional) size
          - (optional) index
        - The left and right fields define a doubly linked list corresponding
          to a row in the matrix.
        - The up and down fields define a doubly linked list corresponding to
          a column in the matrix.
        - Each node has a `column` field which gives O(1) access to the column
          header; or the start of the column linked list.

        - We make no type distinction between regular nodes and header nodes.
          - The distinction can be inferred by a column with a nullptr value.
        - Unlike the Dancing Links paper [Knuth00]:
          - We do not provide a `name` field. Tracking the semantics of each
            row/column is the user's responsibility.
          - Our implementation additionally keeps row headers in addition to
            column headers.
            - Row headers are not accessible from root via up/down fields.
              (TODO: Maybe they should?)
            - The `size` field of row headers do not keep track of the number
              of entries in the row (TODO: Maybe it should?).
              - Currently, its value is used for internal bookkeeping; and may
                change in the future.
          - The `index` field keeps track of the row index for regular nodes
            and for column header nodes the column index.
        */
        Node* left = nullptr;
        Node* right = nullptr;

        Node* up = nullptr;
        Node* down = nullptr;

        Node* column = nullptr;

        std::optional<size_t> size = std::nullopt;
        std::optional<size_t> index = std::nullopt; // Not in [Knuth00]
    };

private:
    friend Solver;

    template <class C>
        requires Container<C, Node*>
    friend auto transform_rows(C&&);

    size_t m_rows;
    size_t m_cols;

    std::vector<Node*> m_headers;
    std::vector<Node*> m_row_headers;

    auto create_headers(size_t Matrix::* extent,
                        std::vector<Node*> Matrix::* vector, Node* Node::* prev,
                        Node* Node::* next) noexcept -> void
    {
        auto root = new Node();
        this->*vector = std::vector<Node*> { root };

        for (auto i = 0uz; i < this->*extent; i++) {
            auto header = new Node();
            header->size = 0;
            header->index = i + 1;

            // When we create these headers, we're only creating a linked list
            // in one direction.
            // The search algorithm assumes that both directions are never null.
            header->up = header;
            header->down = header;
            header->left = header;
            header->right = header;

            (this->*vector).back()->*next = header;
            header->*prev = (this->*vector).back();
            header->*next = root;

            (this->*vector).push_back(std::move(header));
        }

        root->*prev = (this->*vector).back();
    }

public:
    Matrix(size_t rows, size_t cols)
        : m_rows(rows)
        , m_cols(cols)
    {
        assert(rows > 0);
        assert(cols > 0);

        create_headers(&Matrix::m_cols, &Matrix::m_headers, &Node::left,
                       &Node::right);

        create_headers(&Matrix::m_rows, &Matrix::m_row_headers, &Node::up,
                       &Node::down);
    }

    ~Matrix()
    {
        for (auto* header : m_row_headers) {
            // Incorrect ordering of cover-uncover or forgetting to uncover
            // might make some entries unreachable by using column headers only.
            // Instead, we use the row headers to reach every matrix entry.
            if (header->right) {
                for (auto* node = header->right; node != header;) {
                    // Delete every entry belonging to this row header
                    auto next = node->right;
                    delete node;
                    node = next;
                }
            }

            // Delete row header
            delete header;
        }

        for (auto* header : m_headers | std::views::drop(1)) {
            // Delete column header
            delete header;
        }

        // Delete root
        delete m_headers[0];
    }

    auto empty() const noexcept -> bool { return root()->right == root(); }

    [[gnu::flatten]] auto insert(auto&& should_insert, Node* root, Node* node,
                                 Node* Node::* next,
                                 Node* Node::* prev) noexcept -> void
    {
        node->*next = root;
        node->*prev = root;

        if (__builtin_expect(root->*next == nullptr, 0)) {
            std::unreachable();
        }

        if (__builtin_expect(root->*prev == nullptr, 0)) {
            std::unreachable();
        }

        if ((root->*next == root) || (root->*prev == root)) {
            root->size = 1;

            root->*prev = node;
            root->*next = node;

            return;
        }

        for (auto cur = root->*next; cur != root; cur = cur->*next) {
            if (!should_insert(cur, node))
                continue;

            cur->*next->*prev = node;
            node->*next = cur->*next;
            cur->*next = node;
            node->*prev = cur;

            root->size = root->size.value_or(0) + 1;
            break;
        }
    }

    auto insert(size_t row, size_t col) noexcept -> void
    {
        // Important: Use of 1-indexed indices
        assert(row > 0);
        assert(row <= row);
        assert(col > 0);
        assert(col <= col);

        auto* const node = new Node();
        node->index = row;

        {
            auto const header = m_headers[col];
            node->column = header;

            insert(
                [&row](Node* cur, auto) {
                    auto current_row = cur->index.value_or(0);

                    if (__builtin_expect(row == current_row, 0)) {
                        std::unreachable();
                    }

                    if (row < current_row) {
                        return false;
                    }

                    return true;
                },
                header, node, &Node::up, &Node::down);
        }

        {
            auto const header = m_row_headers[row];
            insert(
                [&col](Node* cur, auto) {
                    auto current_col = cur->column->index.value_or(0);

                    if (__builtin_expect(col == current_col, 0)) {
                        std::unreachable();
                    }

                    if (col < current_col) {
                        return false;
                    }

                    return true;
                },
                header, node, &Node::left, &Node::right);
        }
    }

    auto remove(Node* node, Node* Node::* prev,
                Node* Node::* next) const noexcept -> void
    {
        // x.R.L = x.L
        node->*next->*prev = node->*prev;

        // x.L.R = x.R
        node->*prev->*next = node->*next;
    }

    auto reinsert(Node* node, Node* Node::* prev,
                  Node* Node::* next) const noexcept -> void
    {
        // x.R.L = x
        node->*next->*prev = node;

        // x.L.R = x
        node->*prev->*next = node;
    }

    auto cover(size_t col) const noexcept -> void
    {
        assert(col > 0);
        assert(col <= m_cols);

        cover(m_headers[col]);
    }

    auto cover(Node* col) const noexcept -> void
    {
        // Remove c from header-list
        remove(col, &Node::left, &Node::right);

        // For each row in c, remove it from all other columns
        for (auto i = col->down; i != col; i = i->down) {
            for (auto j = i->right; j != i; j = j->right) {
                if (j->column == nullptr) {
                    j->size = limits<size_t>::max();
                    continue;
                }

                auto& size = j->column->size;
                if (__builtin_expect(!size.has_value(), 0)) {
                    std::unreachable();
                }

                if (__builtin_expect(*size == 0, 0)) {
                    std::unreachable();
                }

                remove(j, &Node::up, &Node::down);
                size = *size - 1;
            }
        }
    }

    auto uncover(size_t col) const noexcept -> void
    {
        assert(col > 0);
        assert(col <= m_cols);

        uncover(m_headers[col]);
    }

    auto uncover(Node* col) const noexcept -> void
    {
        for (auto i = col->up; i != col; i = i->up) {
            for (auto j = i->left; j != i; j = j->left) {
                if (j->column == nullptr) {
                    j->size = 0;
                    continue;
                }

                reinsert(j, &Node::up, &Node::down);

                auto& size = j->column->size;

                if (__builtin_expect(!size.has_value(), 0)) {
                    std::unreachable();
                }

                size = *size + 1;
            }
        }

        reinsert(col, &Node::left, &Node::right);
    }

    [[nodiscard]] auto root() const noexcept -> Node const*
    {
        return m_headers[0];
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        auto result = std::string { };

        auto uncovered_rows = 0;
        auto uncovered_cols = 0;

        if (root()->right == root()) {
            return result;
        }

        for (auto i = 1uz; i <= m_rows; i++) {
            if (m_row_headers[i]->size.value_or(0) == limits<size_t>::max()) {
                continue;
            }

            uncovered_rows += 1;
            uncovered_cols = 0;

            for (auto header = root()->right; header != root();
                 header = header->right) {
                auto filled = false;
                uncovered_cols += 1;

                if (__builtin_expect(header->down == nullptr, 0)) {
                    // Realistically this branch should never trigger because
                    // the exact cover problem would be unsolvable if it did.

                    result += "0 ";
                    continue;
                }

                for (auto cur = header->down; cur != header; cur = cur->down) {
                    if (cur->index.value_or(0) == i) {
                        filled = true;
                        break;
                    }
                }

                // TODO: Add option to toggle between dot and zero.
                result += std::format("{} ", filled ? '1' : '.');
            }

            result += '\n';
        }

        result = std::format("Matrix {} x {}\n", uncovered_rows, uncovered_cols)
                 + result;
        return result;
    }
};

template <class C>
    requires Container<C, Matrix::Node*>
[[nodiscard]] auto transform_rows(C&& O) noexcept -> std::unordered_set<size_t>
{
    auto solution = std::unordered_set<size_t> { };

    for (auto* node : O) {
        if (auto index = node->index) {
            solution.insert(*index);
        } else {
            std::unreachable();
        }
    }

    return solution;
}

class Solver {
public:
    using Predicate = std::function<bool(std::vector<Matrix::Node*>&)>;

private:
    Matrix m_matrix;

    [[nodiscard]] auto get_smallest_column() const noexcept -> Matrix::Node*
    {
        auto min_size = limits<size_t>::max();
        auto root = m_matrix.root();
        Matrix::Node* c = nullptr;

        for (auto col = root->right; col != root; col = col->right) {
            auto const size = col->size.value_or(limits<size_t>::max());

            if (size >= min_size) {
                continue;
            }

            min_size = *col->size;
            c = col;
        }

        return c;
    }

    auto search(std::vector<Matrix::Node*>& O,
                std::optional<Predicate> predicate
                = std::nullopt) const noexcept -> std::generator<bool>
    {
        if (m_matrix.empty()) {
            co_yield true;
            co_return;
        }

        auto* c = get_smallest_column();

        m_matrix.cover(c);

        for (auto r = c->down; r != c; r = r->down) {
            O.push_back(r);

            for (auto j = r->right; j != r; j = j->right) {
                if (j->column == nullptr)
                    continue;

                m_matrix.cover(j->column);
            }

            if (!predicate || !(*predicate)(O)) {
                for (auto&& solution : search(O, predicate)) {
                    co_yield solution;
                }
            }

            for (auto j = r->left; j != r; j = j->left) {
                if (j->column == nullptr)
                    continue;

                m_matrix.uncover(j->column);
            }

            O.pop_back();
        }

        m_matrix.uncover(c);
    }

public:
    template <class C>
        requires Container<C, std::pair<size_t, size_t>>
    [[nodiscard]] static auto from_entries(C&& entries) noexcept -> Solver
    {
        auto rows = 0uz;
        auto cols = 0uz;

        for (auto&& [r, c] : entries) {
            rows = std::max(rows, r);
            cols = std::max(cols, c);
        }

        return Solver(rows, cols, std::move(entries));
    }

    template <class C>
        requires Container<C, std::pair<size_t, size_t>>
    Solver(size_t rows, size_t cols, C&& entries)
        : m_matrix(rows, cols)
    {
        for (auto&& [r, c] : entries) {
            m_matrix.insert(r, c);
        }
    }

    [[nodiscard]] auto solve(std::optional<Predicate> predicate
                             = std::nullopt) const noexcept
        -> std::generator<std::unordered_set<size_t>>
    {
        auto O = std::vector<Matrix::Node*> { };
        for (auto _ : search(O, predicate)) {
            if (__builtin_expect(O.empty(), 0)) {
                std::unreachable();
            }

            co_yield DLX::transform_rows(O);
        }
    }

    auto matrix() const noexcept -> Matrix const& { return m_matrix; }
};

}