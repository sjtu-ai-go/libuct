//
// Created by lz on 12/25/16.
//

#ifndef LIBUCT_CNN_V1_HPP
#define LIBUCT_CNN_V1_HPP

#include "message.pb.h"
#include "logger.hpp"
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
            std::shared_ptr<spdlog::logger> logger = {getGlobalLogger()};
            io_service service;
            ip::tcp::endpoint ep;

            static void config_socket(ip::tcp::socket &sock)
            {
                static ip::tcp::socket::reuse_address ra(true);
                static ip::tcp::no_delay nd(true);
                sock.set_option(ra);
                sock.set_option(nd);
            }
        public:
            CNNServiceBase(const std::string &addr, unsigned short port):
                    ep(ip::address::from_string(addr), port)
            {}

            std::string sync_call(const std::string &message)
            {
                logger->trace("Start a RPC");
                ip::tcp::socket sock(service);
                sock.connect(ep);
                config_socket(sock);

                std::int64_t len = message.size();

                logger->trace("Start writing len");
                sock.write_some(buffer(&len, 8));
                logger->trace("Start writing msg");
                sock.write_some(buffer(message));

                std::int64_t resp_len = 0;
                logger->trace("Start reading resp len");
                sock.read_some(buffer(&resp_len, 8));
                std::vector<char> result(resp_len);
                logger->trace("Start read msg with len {}", resp_len);
                sock.read_some(buffer(result, resp_len));
                sock.close();
                std::string result_s;
                std::copy(result.cbegin(), result.cend(), std::back_inserter(result_s));
                return result_s;
            }
        };

        class RequestV1Service: protected CNNServiceBase
        {
        public:
            RequestV1Service(const std::string &addr, unsigned short port):
                    CNNServiceBase(addr, port)
            {}

            gocnn::ResponseV1 sync_call(const gocnn::RequestV1 &reqV1)
            {
                std::string resp = CNNServiceBase::sync_call(reqV1.SerializeAsString());
                gocnn::ResponseV1 respV1;
                respV1.ParseFromString(resp);
                return respV1;
            }
        };

        class RequestV2Service: protected CNNServiceBase
        {
        public:
            RequestV2Service(const std::string &addr, unsigned short port):
                    CNNServiceBase(addr, port)
            {}

            gocnn::ResponseV2 sync_call(const gocnn::RequestV2 &reqV2)
            {
                std::string resp = CNNServiceBase::sync_call(reqV2.SerializeAsString());
                gocnn::ResponseV2 respV2;
                respV2.ParseFromString(resp);
                return respV2;
            }
        };
    }
}

#endif //LIBUCT_CNN_V1_HPP
