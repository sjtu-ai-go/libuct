//
// Created by lz on 12/13/16.
//

#ifndef LIBUCT_UCT_ALGO_HPP
#define LIBUCT_UCT_ALGO_HPP

#include "tree.hpp"
#include <board.hpp>
#include "logger.hpp"
#include <cstddef>
#include <memory>
#include <mutex>

namespace uct
{
    namespace detail
    {
        template<std::size_t W, std::size_t H>
        struct UCTTreeNodeBlock
        {
            std::atomic<int> visit_cnt {0};
            std::atomic<int> x; // X_j in UCT
            board::GridPoint<W, H> action;
            std::mutex expand_mutex;
            std::atomic_bool default_policy_done {false};
            UCTTreeNodeBlock(const UCTTreeNodeBlock& other):
                    visit_cnt(other.visit_cnt.load()), default_policy_done(other.default_policy_done.load()),
                    x(other.x.load()),
                    action(other.action)
            {}

            UCTTreeNodeBlock() = default;
        };

        template<std::size_t W, std::size_t H>
        struct UCTTreePolicyResult
        {
            board::Board<W, H> board;
        };


        template<std::size_t W, std::size_t H>
        struct UCTTreePolicy: public TreePolicy<UCTTreeNodeBlock<W, H>, W * H / 8, UCTTreePolicyResult<W, H>>
        {
            using BaseT = TreePolicy<UCTTreeNodeBlock<W, H>, W * H / 8, UCTTreePolicyResult<W, H>>;
            using TreeNodeType = typename BaseT::TreeNodeType;
            using TreeState = typename BaseT::TreeState;
            using TreePolicyResult = typename BaseT::TreePolicyResult;
            static const std::size_t CH_BUF_SIZE = BaseT::CH_BUF_SIZE;
            std::atomic<int> global_visit_cnt {0};
            std::shared_ptr<spdlog::logger> logger = getGlobalLogger();

            board::Board<W, H> init_board;
            board::Player init_player;

            UCTTreePolicy(const board::Board<W, H> &b, board::Player player):
                    init_board(b), init_player(player)
            {}

            virtual TreePolicyResult tree_policy(TreeNodeType *root) override
            {
                TreeNodeType *cur_node = root;
                board::Board<W, H> cur_board(init_board);
                board::Player cur_player = init_player;

                for (;;)
                {
                    global_visit_cnt.fetch_add(1);
                    if (!cur_node)
                        return std::make_pair(nullptr, TreeState {cur_board});

                    TreeNodeType *expand_node = nullptr;
                    if (!cur_node->block.default_policy_done)
                    {
                        return std::make_pair(nullptr, TreeState {cur_board});
                    }

                    if (cur_node->ch.size() < CH_BUF_SIZE)
                    {
                        // double checking
                        std::lock_guard<std::mutex> lock(cur_node->block.expand_mutex);
                        if (cur_node->ch.size() < CH_BUF_SIZE)
                        {
                            // TODO: expand_node
                            cur_node->ch.emplace_back(cur_node);
                            expand_node = &*cur_node->ch.rbegin();
                        }
                    }

                    if (expand_node)
                        return std::make_pair(expand_node, TreeState {cur_board});
                    else
                    {
                        TreeNodeType *selected_ch = nullptr;
                        // TODO: SELECT child according to UCT()
                        cur_node = selected_ch;
                        cur_board.place(selected_ch->block.action, cur_player);
                        cur_player = board::getOpponentPlayer(cur_player);
                    }
                }
            }
            virtual void default_policy(const TreePolicyResult &result) override
            {
                // TODO
            }
            virtual std::size_t getFinalResultIndex(TreeNodeType *root) override
            {
                // TODO
            }

            virtual TreeNodeType getRoot() override
            {
                return TreeNodeType {nullptr};
            }
        };
    }
}
#endif //LIBUCT_UCT_ALGO_HPP
