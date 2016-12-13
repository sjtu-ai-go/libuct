#include <gtest/gtest.h>
#include <logger.hpp>
#include "uct/uct.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <cstddef>
#include <cstdlib>
#include <chrono>
#include <boost/pool/pool.hpp>

struct TreeNodeBlock1
{
    std::atomic<int> visit_cnt {0};
    std::mutex expand_mutex;
    std::atomic_bool default_policy_done {false};

    TreeNodeBlock1() = default;
    TreeNodeBlock1(const TreeNodeBlock1& other):
            visit_cnt(visit_cnt.load()), default_policy_done(false)
    {}
};

struct TreePolicy1: public uct::TreePolicy<TreeNodeBlock1, 32>
{
    virtual TreeNodeType *tree_policy(TreeNodeType *root, TreeBlockType &tree) override
    {
        TreeNodeType *cur_node = root;
        auto logger = getGlobalLogger();
        logger->trace("[tid={}] tree policy start with root={}", std::this_thread::get_id(), (void*)root);
        for (;;)
        {
            logger->trace("[tid={}] current node={}", std::this_thread::get_id(), (void*)cur_node);
            if (!cur_node)
                return nullptr;
            TreeNodeType *expanded_ch = nullptr;
            if (!cur_node->block.default_policy_done.load())
            {
                cur_node = cur_node->parent;
                continue;
            }

            if (cur_node->ch.size() < 32) {
                std::lock_guard<std::mutex> lock(cur_node->block.expand_mutex);
                if (cur_node->ch.size() < 32)
                {
                    cur_node->ch.emplace_back(cur_node);
                    expanded_ch = &*cur_node->ch.rbegin();
                }
            }
            if (expanded_ch)
            {
                return expanded_ch;
            } else
            {
                cur_node = &*(cur_node->ch.begin() + (std::rand() % cur_node->ch.size()));
            }
        }
    }

    virtual void default_policy(TreeNodeType *new_expanded_leaf, TreeBlockType &tree) override
    {
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!new_expanded_leaf->block.default_policy_done.exchange(true))
        {
            while (new_expanded_leaf)
            {
                new_expanded_leaf->block.visit_cnt.fetch_add(1);
                new_expanded_leaf = new_expanded_leaf->parent;
            }
        }
    }

    virtual std::size_t getFinalResultIndex(TreeNodeType *root, TreeBlockType &) override
    {
        return 0;
    }
};

TEST(TreeTest, TestTree1)
{
    auto logger = getGlobalLogger();
    logger->set_level(spdlog::level::debug);
    uct::Tree<TreePolicy1> tree;
    tree.run(4, std::chrono::seconds(2));
}