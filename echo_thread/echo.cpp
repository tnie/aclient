//
// blocking_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2020 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <spdlog\spdlog.h>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include "asio.hpp"

using asio::ip::tcp;

const int max_length = 1024;

std::atomic<long> g_count = 0;

void session(tcp::socket sock)
{
    try
    {
        for (;;)
        {
            char data[max_length];

            asio::error_code error;
            size_t length = sock.read_some(asio::buffer(data), error);
            if (error == asio::error::eof) {
                spdlog::info("Connection closed cleanly by peer. {}", --g_count);
                break; // Connection closed cleanly by peer.
            }
            else if (error)
                throw asio::system_error(error); // Some other error.

            asio::write(sock, asio::buffer(data, length));
        }
    }
    catch (std::exception& e)
    {
        spdlog::info("Exception in thread: {}. {}", e.what(), --g_count);
    }
}

void server(asio::io_context& io_context, unsigned short port)
{
    tcp::acceptor a(io_context, tcp::endpoint(tcp::v4(), port));
    for (;;)
    {
        spdlog::info("waiting for someone connect {}...", ++g_count);
        std::thread(session, a.accept()).detach();
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: blocking_tcp_echo_server <port>\n";
            return 1;
        }

        asio::io_context io_context;

        server(io_context, std::atoi(argv[1]));
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}