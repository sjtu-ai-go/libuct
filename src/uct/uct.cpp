#include "detail/tree.hpp"
#include "detail/uct_algo.hpp"

namespace uct
{
    template class Tree<detail::UCTTreePolicy<5, 5>>;
    template class Tree<detail::UCTTreePolicy<9, 9>>;
    template class Tree<detail::UCTTreePolicy<19, 19>>;
}
