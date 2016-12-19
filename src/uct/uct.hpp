#ifndef LIBUCT_HEADER
#define LIBUCT_HEADER

#include "detail/tree.hpp"
#include "detail/uct_algo.hpp"
namespace uct
{
    extern template class Tree<detail::UCTTreePolicy<5, 5>>;
    extern template class Tree<detail::UCTTreePolicy<9, 9>>;
    extern template class Tree<detail::UCTTreePolicy<19, 19>>;
}

#endif