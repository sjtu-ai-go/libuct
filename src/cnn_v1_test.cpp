//
// Created by lz on 12/25/16.
//

#include "uct/detail/cnn_v1.hpp"
#include <board.hpp>
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <string>
#include <cstdint>
#include <thread>
#include <chrono>
#include <string>
#include <algorithm>

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

TEST(CNNBaseTest, TestCNNBaseConstruct)
{
    uct::detail::CNNServiceBase cnnbase("127.0.0.1", 7589);
    Stub stub(7589, "helloworld");
    std::thread t {&Stub::run, &stub} ;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string answer = cnnbase.sync_call("whoareu");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ("helloworld", answer);
    EXPECT_EQ("whoareu", stub.echo);
    if (t.joinable())
        t.join();
}

TEST(ReqV1Test, TestReqV1Connect)
{
    uct::detail::RequestV1Service reqV1Service("127.0.0.1", 7593);
    gocnn::ResponseV1 resp;
    resp.set_board_size(361);
    for (int i=0; i<resp.board_size(); ++i)
        resp.add_possibility(0.5);

    gocnn::RequestV1 reqV1;
    reqV1.set_board_size(361);

    V1Stub stub(7593, resp);

    std::thread t {&V1Stub::run, &stub};
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    gocnn::ResponseV1 gotResp = reqV1Service.sync_call(reqV1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_EQ(361, gotResp.board_size());
    EXPECT_EQ(361, gotResp.possibility_size());
    EXPECT_EQ(0.5, gotResp.possibility(150));

    EXPECT_EQ(361, stub.echo.board_size());
    EXPECT_EQ(0, stub.echo.our_group_lib1_size());
    if (t.joinable())
        t.join();

}

TEST(ReqV2Test, TestReqV2Connect)
{
    uct::detail::RequestV2Service reqV2Service("127.0.0.1", 7592);
    gocnn::ResponseV2 resp;
    resp.set_board_size(361);
    for (int i=0; i<resp.board_size(); ++i)
        resp.add_possibility(0.5);

    gocnn::RequestV2 reqV2;
    reqV2.set_board_size(361);

    V2Stub stub(7592, resp);

    std::thread t {&V2Stub::run, &stub};
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    gocnn::ResponseV2 gotResp = reqV2Service.sync_call(reqV2);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_EQ(361, gotResp.board_size());
    EXPECT_EQ(361, gotResp.possibility_size());
    EXPECT_EQ(0.5, gotResp.possibility(150));

    EXPECT_EQ(361, stub.echo.board_size());
    EXPECT_EQ(0, stub.echo.capture_size_five_size());
    if (t.joinable())
        t.join();

}

TEST(CNNBaseTest, DISABLED_TestReqV1Remote)
{
    uct::detail::RequestV1Service reqV1Service("127.0.0.1", 7591);
    board::Board<19, 19> b;
    auto reqV1 = b.generateRequestV1(board::Player::B);

    spdlog::set_level(spdlog::level::trace);
    gocnn::ResponseV1 respV1 = reqV1Service.sync_call(reqV1);
    std::cout << respV1.board_size() << std::endl;
    for (float i : respV1.possibility())
        std::cout << i << " " << std::endl;
    // todo
    EXPECT_TRUE(true);
}

TEST(CNNBaseTest, DISABLED_TestReqV1RemoteConcurent)
{
    std::vector<std::thread> ts;
    spdlog::set_level(spdlog::level::trace);
    for (int i=0; i<32; ++i)
        ts.emplace_back([&]() {

            board::Board<19, 19> b;
            uct::detail::RequestV1Service reqV1Service("127.0.0.1", 7591);
            auto reqV1 = b.generateRequestV1(board::Player::W);

            spdlog::set_level(spdlog::level::trace);
            gocnn::ResponseV1 respV1 = reqV1Service.sync_call(reqV1);
        });
    // todo
    std::for_each(ts.begin(), ts.end(), [](std::thread &t) {
        if (t.joinable())
            t.join();
    });
    EXPECT_TRUE(true);
}
