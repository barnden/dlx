#include <cassert>
#include <cstddef>
#include <deque>
#include <format>
#include <initializer_list>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace DLX {
class Matrix;
}
namespace std {
template <>
struct formatter<DLX::Matrix> : formatter<string> {
    auto format(auto const& instance, auto& context) const
    {
        return formatter<string>::format(instance.to_string(), context);
    }
};
}

namespace DLX {

template <typename C, typename T>
concept Container = std::ranges::input_range<C> && std::same_as<std::ranges::range_value_t<C>, T>;

class Solver;
class Matrix {
    friend Solver;

    size_t m_rows;
    size_t m_cols;

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
            // Free using row-headers as covering a column does not potentially
            // remove a node from the row, only from the column.
            if (header->right) {
                for (auto* node = header->right; node != header;) {
                    // Delete every row belonging to this column header
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

    [[gnu::flatten]] auto insert(auto&& criterion, Node* root, Node* node,
                                 Node* Node::* next,
                                 Node* Node::* prev) noexcept -> void
    {
        node->*next = root;
        node->*prev = root;

        if ((root->*next == nullptr) || (root->*prev == nullptr)
            || (root->*next == root) || (root->*prev == root)) {
            root->size = 1;

            root->*prev = node;
            root->*next = node;

            return;
        }

        for (auto cur = root->*next; cur != root; cur = cur->*next) {
            if (!criterion(cur, node))
                continue;

            cur->*prev->*next = node;
            node->*prev = cur->*prev;
            cur->*prev = node;
            node->*next = cur;

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
                [&header, &row](Node* cur, auto) {
                    auto current_row = cur->index.value_or(0);
                    auto next_row = cur->down->index.value_or(0);

                    if (current_row == row)
                        return false;

                    if ((cur->up != header) && (row < current_row))
                        return false;

                    if ((cur->down != header) && (row < next_row))
                        return false;

                    return true;
                },
                header, node, &Node::up, &Node::down);
        }

        {
            auto const header = m_row_headers[row];
            insert(
                [&header, &col](Node* cur, auto) {
                    auto current_col = cur->column->index.value_or(0);
                    auto next_col = (cur->right->column)
                                        ? cur->right->column->index.value_or(0)
                                        : 0;

                    if (current_col == col)
                        return false;

                    if ((cur->left != header) && (col < current_col))
                        return false;

                    if ((cur->right != header) && (col < next_col))
                        return false;

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
                    j->size = std::numeric_limits<size_t>::max();
                    continue;
                }

                remove(j, &Node::up, &Node::down);
                j->column->size
                    = std::max(j->column->size.value_or(0), 1uz) - 1;
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
                j->column->size = j->column->size.value_or(0) + 1;
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
            if (m_row_headers[i]->size.value_or(0)
                == std::numeric_limits<size_t>::max()) {
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

class Solver {
    Matrix m_matrix;

    auto search(std::deque<Matrix::Node*>& O, size_t k = 0) const noexcept
        -> bool
    {
        if (m_matrix.empty()) {
            return true;
        }

        auto* root = m_matrix.root();

        auto min_size = std::numeric_limits<size_t>::max();
        Matrix::Node* c = nullptr;
        for (auto col = root->right; col != root; col = col->right) {
            if (col->size.value_or(std::numeric_limits<size_t>::max()) >= min_size)
                continue;

            min_size = *col->size;
            c = col;
        }

        m_matrix.cover(c);

        for (auto r = c->down; r != c; r = r->down) {
            O.push_back(r);

            for (auto j = r->right; j != r; j = j->right) {
                if (j->column == nullptr)
                    continue;

                m_matrix.cover(j->column);
            }

            if (search(O, k + 1)) {
                return true;
            }

            for (auto j = r->left; j != r; j = j->left) {
                if (j->column == nullptr)
                    continue;

                m_matrix.uncover(j->column);
            }

            O.pop_back();
        }

        m_matrix.uncover(c);
        return false;
    }

public:
    template <class C>
        requires Container<C, std::pair<size_t, size_t>>
    [[nodiscard]] static auto
    from_entries(C&& entries) noexcept
        -> Solver
    {
        auto rows = 0uz;
        auto cols = 0uz;

        for (auto&& [r, c] : entries) {
            rows = std::max(rows, r);
            cols = std::max(cols, c);
        }

        return Solver(rows, cols, entries);
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

    [[nodiscard]] auto solve() const noexcept -> std::optional<std::set<size_t>>
    {
        auto O = std::deque<Matrix::Node*> { };
        auto solution = std::set<size_t> { };

        if (!search(O)) {
            return std::nullopt;
        }

        for (auto* node : O) {
            m_matrix.uncover(node->column);
            solution.insert(node->index.value_or(0));
        }

        return solution;
    }

    auto matrix() const noexcept -> Matrix const& { return m_matrix; }
};

}