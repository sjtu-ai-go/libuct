//
// Created by lz on 12/25/16.
//

#include "uct/detail/cnn_v1.hpp"
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

    int run()
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
        return 0;
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