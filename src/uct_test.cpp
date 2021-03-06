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
#include <boost/asio.hpp>

struct TreeNodeBlock1
{
    std::atomic<int> visit_cnt {0};
    std::mutex expand_mutex;
    std::atomic_bool default_policy_done {false};

    TreeNodeBlock1() = default;
    TreeNodeBlock1(const TreeNodeBlock1& other):
            visit_cnt(other.visit_cnt.load()), default_policy_done(false)
    {}
};

struct TreePolicy1: public uct::TreePolicy<TreeNodeBlock1, 32>
{
    using BaseT = uct::TreePolicy<TreeNodeBlock1, 32>;
    using TreeNodeType = typename BaseT::TreeNodeType;
    using TreeState = typename BaseT::TreeState;
    using TreePolicyResult = typename BaseT::TreePolicyResult;
    static const std::size_t CH_BUF_SIZE = BaseT::CH_BUF_SIZE;
    virtual TreePolicyResult tree_policy(TreeNodeType *root) override
    {
        TreeNodeType *cur_node = root;
        auto logger = getGlobalLogger();
        logger->trace("[tid={}] tree policy start with root={}", std::this_thread::get_id(), (void*)root);
        for (;;)
        {
            logger->trace("[tid={}] current node={}", std::this_thread::get_id(), (void*)cur_node);
            if (!cur_node)
                return std::make_pair(nullptr, TreeState {});
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
                return std::make_pair(expanded_ch, TreeState {});
            } else
            {
                cur_node = &*(cur_node->ch.begin() + (std::rand() % cur_node->ch.size()));
            }
        }
    }

    virtual void default_policy(const TreePolicyResult &result) override
    {
        TreeNodeType *new_expanded_leaf = result.first;
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

    virtual std::size_t getFinalResultIndex(TreeNodeType *root) override
    {
        return 0;
    }

    virtual TreeNodeType getRoot() override
    {
        return TreeNodeType {nullptr};
    }
};

TEST(TreeTest, TestTree1)
{
    auto logger = getGlobalLogger();
    logger->set_level(spdlog::level::debug);
    uct::Tree<TreePolicy1> tree;
    tree.run(4, std::chrono::seconds(2));
}

TEST(UCTTest, DISABLED_TestUCT9x9) // Disabled due to lack of 9x9 CNN Server
{
    auto logger = getGlobalLogger();
    logger->set_level(spdlog::level::debug);
    board::Board<9, 9> b;
    uct::Tree<uct::detail::UCTTreePolicy<9, 9>> tree(b, board::Player::B, 6.5, "127.0.0.1", 7591);
    tree.run(4, std::chrono::seconds(1));
    tree.dumpToDotFile("uct_test1.dot");
}

using namespace boost::asio;
class Stub
{
    io_service service;
    ip::tcp::acceptor acceptor;

    const std::string reply;
public:
    std::string echo;
    Stub(unsigned short port, std::string reply):
            acceptor(service, ip::tcp::endpoint(ip::address::from_string("127.0.0.1"), port)),
            reply(reply)
    {
    }

    void run()
    {
        ip::tcp::socket sock(service);
        acceptor.accept(sock);
        std::int64_t len;

        sock.read_some(buffer(&len, 8));
        std::vector<char> buf(len);
        sock.read_some(buffer(buf));
        echo.clear();
        std::copy(buf.cbegin(), buf.cend(), std::back_inserter(echo));
        std::int64_t reply_size = reply.size();
        sock.write_some(buffer(&reply_size, 8));
        sock.write_some(buffer(reply, reply_size));
        sock.close();
    }
};

class V1Stub: protected Stub
{
public:
    gocnn::RequestV1 echo;
    V1Stub(unsigned short port, gocnn::ResponseV1 resp):
            Stub(port, resp.SerializeAsString())
    {}

    void run()
    {
        Stub::run();
        echo.ParseFromString(Stub::echo);
    }
};

class V2Stub: protected Stub
{
public:
    gocnn::RequestV2 echo;
    V2Stub(unsigned short port, gocnn::ResponseV2 resp):
            Stub(port, resp.SerializeAsString())
    {}

    void run()
    {
        Stub::run();
        echo.ParseFromString(Stub::echo);
    }
};

// disabled due to our new compact format
TEST(UCTTest, DISABLED_TestUCT2)
{
    gocnn::ResponseV2 resp;
    resp.set_board_size(361);
    resp.mutable_possibility()->Resize(361, 1.0 / 361);
    V2Stub stub(7814, resp);
    std::thread stub_thread([&]() {
        for (;;)
            stub.run();
    });

    stub_thread.detach();

    auto logger = getGlobalLogger();
    logger->set_level(spdlog::level::debug);
    board::Board<19, 19> b;
    uct::Tree<uct::detail::UCTTreePolicy<19, 19>> tree(b, board::Player::B, 6.5, "127.0.0.1", 7814);
    using TreeT = decltype(tree);
    tree.run(2, std::chrono::seconds(5));
    typename TreeT::TreeNodeType *selected_node = tree.getResultNode();
    tree.dumpToDotFile("uct_test2.dot");
}

TEST(UCTTest, DISABLED_TestRemoteUCT)
{
    board::Board<19, 19> b;
    uct::Tree<uct::detail::UCTTreePolicy<19, 19>> tree(b, board::Player::B, 6.5, "127.0.0.1", 7591);
    using TreeT = decltype(tree);
    tree.run(4, std::chrono::seconds(10));
    typename TreeT::TreeNodeType *selected_node = tree.getResultNode();
}
