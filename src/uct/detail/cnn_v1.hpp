//
// Created by lz on 12/25/16.
//

#ifndef LIBUCT_CNN_V1_HPP
#define LIBUCT_CNN_V1_HPP

#include "message.pb.h"
#include <boost/asio.hpp>
#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace uct
{
    namespace detail
    {
        using namespace boost::asio;
        class CNNServiceBase
        {
        protected:
            io_service service;
            ip::tcp::endpoint ep;
        public:
            CNNServiceBase(const std::string &addr, unsigned short port):
                    ep(ip::address::from_string(addr), port)
            {}

            static void config_socket(ip::tcp::socket &sock)
            {
                static ip::tcp::socket::reuse_address ra(true);
                static ip::tcp::no_delay nd(true);
                sock.set_option(ra);
                sock.set_option(nd);
            }

            std::string sync_call(const std::string &message)
            {
                ip::tcp::socket sock(service);
                sock.connect(ep);
                config_socket(sock);

                std::int64_t len = message.size();
                sock.write_some(buffer(&len, 8));
                sock.write_some(buffer(message));

                std::int64_t resp_len = 0;
                sock.read_some(buffer(&resp_len, 8));
                std::vector<char> result(resp_len);
                sock.read_some(buffer(result, resp_len));
                sock.close();
                std::string result_s;
                std::copy(result.cbegin(), result.cend(), std::back_inserter(result_s));
                return result_s;
            }
        };
    }
}

#endif //LIBUCT_CNN_V1_HPP
