//
// Created by lz on 12/11/16.
//

#ifndef LIBUCT_TREE_HPP_HPP
#define LIBUCT_TREE_HPP_HPP

#include <logger.hpp>

#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <cstddef>
#include <thread>
#include <atomic>
#include <cassert>
#include <boost/pool/pool_alloc.hpp>

namespace uct
{
    namespace detail
    {
        template<typename T, std::size_t ch_buf_size>
        struct TreeNodeWithBlock
        {
        public:
            TreeNodeWithBlock *parent;
            std::vector<TreeNodeWithBlock> ch;
            T block;


            template<typename ... Us>
            TreeNodeWithBlock(TreeNodeWithBlock *parentP, Us&& ...us):
                    parent(parentP), block(std::forward<Us>(us)...)
            {
                ch.reserve(ch_buf_size);
            }

            TreeNodeWithBlock():
                    TreeNodeWithBlock(nullptr)
            {
                ch.reserve(ch_buf_size);
            }
        };


    }

    struct EmptyTreeBlock {};

    template<typename PolicyT>
    class Tree;

    template<typename AdditionalBlockT, std::size_t ch_buf_size, typename TreeBlockT = EmptyTreeBlock>
    struct TreePolicy
    {
        using TreeType = Tree<TreePolicy>;
        using BlockType = AdditionalBlockT;
        using TreeBlockType = TreeBlockT;
        using TreeNodeType = detail::TreeNodeWithBlock<AdditionalBlockT, ch_buf_size>;
        // From root, return expanded new leaf on success(then default_policy will be called),
        // otherwise return nullptr(tree_policy will be called again)
        virtual TreeNodeType *tree_policy(TreeNodeType *root, TreeBlockType& tree) = 0;
        // From newly expanded leaf, compute value and propagate back to root
        virtual void default_policy(TreeNodeType *new_expanded_leaf, TreeBlockType &tree) = 0;
        // after running, use getFinalResultIndex to suggest which child of root will you choose
        virtual std::size_t getFinalResultIndex(TreeNodeType *root, TreeBlockType &tree) = 0;
        virtual ~TreePolicy() {}

        static const std::size_t CH_BUF_SIZE = ch_buf_size;
    };

    template<typename PolicyT>
    class Tree
    {
    public:
        using BlockType = typename PolicyT::BlockType;
        static const std::size_t CH_BUF_SIZE = PolicyT::CH_BUF_SIZE;
        using PolicyType = PolicyT;
        using TreeNodeType = typename PolicyType::TreeNodeType;
        using TreeBlockType = typename PolicyT::TreeBlockType;
        static_assert(std::is_base_of<TreePolicy<BlockType, CH_BUF_SIZE>, PolicyT>::value, "PolicyType must derive from uct::TreePolicy");
    protected:
        std::unique_ptr<TreeNodeType> root_;
        std::shared_ptr<spdlog::logger> plogger_;
        TreeBlockType block;
    public:

        template<typename ... Us>
        Tree(Us&& ...us):
                root_(new TreeNodeType(std::forward<Us>(us)...)), plogger_(getGlobalLogger())
        {
            PolicyType policy;
            policy.default_policy(root_.get(), block);
            plogger_->trace("Tree established with root at {} ", (void*)this);
        }

        void run(std::size_t thread_num, std::chrono::milliseconds time_limit_ms);

        std::size_t getResultIndex()
        {
            PolicyType policy;
            return policy.getResultIndex(root_.get(), block);
        }

    protected:
        void single_thread_runner(std::chrono::milliseconds time_limit_ms, TreeNodeType *root_node);
    };

    template<typename PolicyT>
    void Tree<PolicyT>::single_thread_runner(std::chrono::milliseconds time_limit_ms, TreeNodeType *root_node)
    {
        static constexpr std::size_t TIME_CHECK_CNT_INTEVAL = 100;
        std::size_t cnt = 0;
        auto start_time = std::chrono::steady_clock::now();
        plogger_->debug("[tid={}] New Tree thread created with CHECK_INTEVAL={}, root={}",
                        std::this_thread::get_id(), TIME_CHECK_CNT_INTEVAL, (void*)root_node);

        std::size_t cnt_since_last_check = 0;
        PolicyType policy;

        for (;;)
        {
            ++cnt; ++cnt_since_last_check;
            if (cnt_since_last_check >= TIME_CHECK_CNT_INTEVAL)
            {
                auto cur_time = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - start_time) > time_limit_ms)
                    break;
                cnt_since_last_check = 0;
            }

            TreeNodeType *new_expanded_leaf = policy.tree_policy(root_.get(), block);
            if (new_expanded_leaf)
            {
                policy.default_policy(new_expanded_leaf, block);
            }
        }
        auto cur_time = std::chrono::steady_clock::now();
        plogger_->debug("[tid={}] Tree thread finished! cnt={} time_eclipsed: {}ms",
            std::this_thread::get_id(), cnt, std::chrono::duration_cast<std::chrono::milliseconds>(
                        cur_time - start_time
                ).count());
    }

    template<typename PolicyT>
    void Tree<PolicyT>::run(std::size_t thread_num, std::chrono::milliseconds time_limit_ms)
    {
        plogger_->debug("Start to run Tree & default policy with time limit {}ms and {} threads",
                        time_limit_ms.count(), thread_num);
        std::vector<std::thread> threads;
        for (std::size_t i=0; i<thread_num; ++i)
            threads.emplace_back(&Tree::single_thread_runner, this, time_limit_ms, root_.get());
        std::for_each(threads.begin(), threads.end(), [](std::thread &t) {
            if (t.joinable())
                t.join();
        });
        plogger_->debug("Tree & default_policy finished");
    }
}
#endif //LIBUCT_TREE_HPP_HPP
