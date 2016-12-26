//
// Created by lz on 12/13/16.
//

#ifndef LIBUCT_UCT_ALGO_HPP
#define LIBUCT_UCT_ALGO_HPP

#include "tree.hpp"
#include <board.hpp>
#include "logger.hpp"
#include "cnn_v1.hpp"
#include <cstddef>
#include <memory>
#include <mutex>
#include <random>
#include <atomic>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <fastrollout/fastrollout.hpp>

namespace uct
{
    namespace detail
    {

        constexpr std::size_t ChBufSize(std::size_t W, std::size_t H)
        {
            return W * H / 8;
        }

        template<std::size_t W, std::size_t H>
        struct UCTTreeNodeBlock
        {
            std::atomic<int> visit_cnt {0};

            static const std::size_t Q_BASE = 4096; // if it's too small, we may lose precision; too large: overflow;
            // 2^32 / 4096 = 2^20, large enough for single traversal
        private:
            std::atomic_int_fast32_t q { 0 }; // Q in UCT, multiplied by Q_BASE
            static const std::int16_t MAGIC_NUM = 0x38e5;
            volatile const std::int16_t magic_num = MAGIC_NUM; // if magic_num is not ok, then we may read dirty data
        public:
            board::GridPoint<W, H> action; // the action from parent to this node
            std::mutex expand_mutex;
            std::atomic_bool default_policy_done {false};
            board::Player player; // Next step is which player's round

            using GoodPositionType =
            decltype(std::declval<board::Board<W, H>>().getAllGoodPosition(std::declval<board::Player>()));

            std::unique_ptr<std::vector<board::GridPoint<W, H>>> pGoodPos;
            // Good positions to place, no duplicates
            // @nullable

            UCTTreeNodeBlock(const UCTTreeNodeBlock& other):
                    visit_cnt(other.visit_cnt.load()), default_policy_done(other.default_policy_done.load()),
                    q(other.q.load()),
                    action(other.action),
                    player(other.player)

            {
                if (other.pGoodPos)
                    pGoodPos.reset(new GoodPositionType (*other.pGoodPos));
            }

            explicit UCTTreeNodeBlock(board::Player player, board::GridPoint<W, H> action):
                    player(player), action(action)
            {}

            UCTTreeNodeBlock& operator= (const UCTTreeNodeBlock &other)
            {
                visit_cnt = other.visit_cnt.load();
                default_policy_done.store(other.default_policy_done.load());
                q = other.q.load();
                action = other.action;
                player = other.player;
            }

            double getQ() const
            {
                return (double) q.load() / Q_BASE;
            }

            void addQ(double newQ)
            {
                int to_add = static_cast<int>(newQ * Q_BASE);
                q.fetch_add(to_add);
            }

            bool isClean() const volatile
            {
                return magic_num == MAGIC_NUM;
            }
        };

        template<std::size_t W, std::size_t H>
        struct UCTTreePolicyResult
        {
            board::Board<W, H> board;
        };


        template<std::size_t W, std::size_t H>
        struct UCTTreePolicy: public TreePolicy<UCTTreeNodeBlock<W, H>, ChBufSize(W, H), UCTTreePolicyResult<W, H>>
        {
            using BaseT = TreePolicy<UCTTreeNodeBlock<W, H>, ChBufSize(W, H), UCTTreePolicyResult<W, H>>;
            using TreeNodeType = typename BaseT::TreeNodeType;
            using TreeState = typename BaseT::TreeState;
            using TreePolicyResult = typename BaseT::TreePolicyResult;
            using PointType = typename board::Board<W, H>::PointType;
            static const std::size_t CH_BUF_SIZE = BaseT::CH_BUF_SIZE;
            std::atomic<int> global_visit_cnt {0};
            std::shared_ptr<spdlog::logger> logger = getGlobalLogger();

            const board::Board<W, H> init_board;
            const board::Player init_player;
            const double komi;

            std::mt19937 gen { std::random_device()() };

            UCTTreePolicy(const board::Board<W, H> &b, board::Player player, double komi, const std::string addr,
            unsigned short port):
                    init_board(b), init_player(player), komi(komi), reqv1Service(addr, port)
            {}

            static double uctVal(const TreeNodeType& node)
            {
                return node.block.default_policy_done ?
                       node.block.getQ() / node.block.visit_cnt.load() +
                       0.707 * std::sqrt(
                               node.parent ?
                               2 * std::log(node.parent->block.visit_cnt.load()) / node.block.visit_cnt.load() :
                               2.0
                       ) :
                       -1.0;
            }

            detail::RequestV1Service reqv1Service;
            auto getCNNGoodPositions(board::Board<W, H> &b, board::Player player) ->
            typename UCTTreeNodeBlock<W, H>::GoodPositionType
            {
                auto requestV1 = b.generateRequestV1(player);
                auto resp = reqv1Service.sync_call(requestV1);
                auto &possibility = *resp.mutable_possibility();
                using PairT = std::pair<PointType, double>;
                std::vector<PairT> vp;
                vp.reserve(W * H);
                for (std::size_t i=0; i<possibility.size(); ++i)
                    vp.emplace_back(PointType(i / H, i % H), possibility.data()[i]);
                std::sort(vp.begin(), vp.end(), [](const PairT &a, const PairT &b) {
                   return a.second > b.second;
                }); // vp: possibility large -> small

                auto goodPosVec = b.getAllGoodPosition(player);
                const double ACCUM_THRES = b.getStep() > 100 ? (b.getStep() > 200 ? 0.95 : 0.9): 0.8;
                double accum = 0.0;
                auto it = vp.begin();
                int cnt = 0;
                for (; it != vp.end() && (accum < ACCUM_THRES || cnt < 2); ++it)
                {
                    if (std::find(goodPosVec.begin(), goodPosVec.end(), it->first) != goodPosVec.end()) {
                        accum += it->second;
                        ++cnt;
                    }
                }
                vp.erase(it, vp.end());

                std::vector<PointType> ans; ans.reserve(W * H);
                std::for_each(vp.rbegin(), vp.rend(), [&](const PairT &p) {
                    if (b.getPosStatus(p.first, player) == board::Board<W, H>::PositionStatus::OK)
                        ans.push_back(p.first);
                }); // ans: small to large
                return ans;
            }

            virtual TreePolicyResult tree_policy(TreeNodeType *root) override
            {
                TreeNodeType *cur_node = root;
                board::Board<W, H> cur_board(init_board);
                board::Player cur_player = init_player;

                for (;;)
                {
                    global_visit_cnt.fetch_add(1);
                    // Node's visit_cnt will be updated in default_policy's propagation
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
                        if (cur_node->ch.size() < CH_BUF_SIZE &&
                            (!cur_node->block.pGoodPos ||
                             cur_node->ch.size() < cur_node->block.pGoodPos->size()
                            )
                                )
                        {
                            // Only calculate goodPos at the first time
                            if (!cur_node->block.pGoodPos)
                                cur_node->block.pGoodPos.reset(new typename decltype(cur_node->block)::GoodPositionType
                                    (getCNNGoodPositions(cur_board, cur_player)));
                            logger->trace("Generate finished");
                            auto &validPosVec = *cur_node->block.pGoodPos;
                            if (validPosVec.empty())
                                return std::make_pair(nullptr, TreeState {cur_board});

                            std::size_t selected_index = validPosVec.size() - 1;
                            PointType action = validPosVec[selected_index];
                            validPosVec.pop_back(); // the last one: best

                            cur_node->ch.emplace_back(cur_node, board::getOpponentPlayer(cur_player), action);
                            expand_node = &*cur_node->ch.rbegin();
                        }
                    }

                    if (expand_node)
                    {
                        cur_board.place(expand_node->block.action, cur_player);
                        cur_player = board::getOpponentPlayer(cur_player);
                        return std::make_pair(expand_node, TreeState {cur_board});
                    }
                    else
                    {
                        TreeNodeType *selected_ch = nullptr;

                        std::vector<double> uctValue;
                        uctValue.reserve(CH_BUF_SIZE);

                        for (std::size_t i=0; i<cur_node->ch.size(); ++i)
                        {
                            TreeNodeType & child = cur_node->ch[i];
                            uctValue.push_back(uctVal(child));
                        }
                        long selected_ch_idx = std::max_element(uctValue.begin(), uctValue.end()) - uctValue.begin();
                        if (uctValue.empty())
                            return std::make_pair(nullptr, TreeState {cur_board});

                        selected_ch = &(cur_node->ch[selected_ch_idx]);

                        cur_node = selected_ch;
                        if (cur_node->block.isClean()) {
                            cur_board.place(selected_ch->block.action, cur_player);
                        } else
                            return std::make_pair(nullptr, TreeState {cur_board});
                        cur_player = board::getOpponentPlayer(cur_player);
                    }
                }
            }

            fastrollout::RandomRolloutPolicy<W, H> rolloutEngine{3};
            virtual void default_policy(const TreePolicyResult &result) override
            {
                TreeNodeType *cur_node = result.first;
                const auto &board = result.second.board;

                double cur_q = rolloutEngine.run(board, komi, cur_node->block.player);
                // cur_q: higher <-> white dominates

                while(cur_node)
                {
                    // Next action is for Black <=> Previous action is for white <=> the higher the better
                    cur_node->block.addQ(cur_node->block.player == board::Player::B ? cur_q : -cur_q);
                    cur_node->block.visit_cnt.fetch_add(1);
                    cur_node = cur_node->parent;
                }

                result.first->block.default_policy_done.store(true);
            }
            virtual std::size_t getFinalResultIndex(TreeNodeType *root) override
            {
                std::stringstream ss;
                ss << "Root chs:";
                std::for_each(root->ch.begin(), root->ch.end(), [&](TreeNodeType &tn) {
                    ss << "[visit_cnt=" << tn.block.visit_cnt.load() << ", Q=" << tn.block.getQ() <<
                       ", uct=" << uctVal(tn) << "], ";
                });
                logger->debug(ss.str());
                return (std::size_t)
                        (std::max_element(root->ch.cbegin(), root->ch.cend(), [](const TreeNodeType&n1, const TreeNodeType &n2) {
                            return uctVal(n1) < uctVal(n2);
                        }) - root->ch.cbegin());
            }

            virtual TreeNodeType getRoot() override
            {
                return TreeNodeType {nullptr, init_player, PointType(0, 0)};
            }
        };
    }

    template <std::size_t W, std::size_t H>
    using UCTTree = Tree< detail::UCTTreePolicy<W, H> >;
}
#endif //LIBUCT_UCT_ALGO_HPP
