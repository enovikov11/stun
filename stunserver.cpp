/**
 * @file stunserver.cpp
 * @brief Simple high performance RFC 8489 compliant UDP IPv4 STUN server that just handles binding requests.
 * @author Evgenii Novikov
 * @email enovikov11@yandex.com
 * @license CC0 (Public Domain) https://creativecommons.org/public-domain/cc0/
 * @source https://github.com/enovikov11/stun
 */

#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <iostream>
#include <array>
#include <memory>

using boost::asio::ip::udp;

const std::array<unsigned char, 32> response_template = {
    0x01, 0x01,             // STUN message type: Binding Response
    0x00, 0x0C,             // Message length: 32 bytes - 20 bytes header
    0x21, 0x12, 0xA4, 0x42, // Magic cookie
    // 8-19: Transaction ID (variable part, to be copied from the request)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,                   // Reserved
    0x20,                   // XOR-MAPPED-ADDRESS type
    0x00, 0x08,             // Length
    0x00,                   // Reserved
    0x01,                   // Family (IPv4)
    // 26-31: Port and Address (variable part magic cookie, to be XORed)
    0x21, 0x12, 0x21, 0x12, 0xA4, 0x42
};

class StunServer : public std::enable_shared_from_this<StunServer>
{
public:
    StunServer(boost::asio::io_context &io_context, unsigned short port)
        : socket_(io_context)
    {
        boost::asio::socket_base::reuse_address option(true);
        socket_.open(udp::v4());
        socket_.set_option(option);
        socket_.bind(udp::endpoint(udp::v4(), port));
        start_receive();
    }

private:
    void start_receive()
    {
        auto recv_buffer = std::make_shared<std::array<unsigned char, 32>>();
        auto remote_endpoint = std::make_shared<udp::endpoint>();
        
        socket_.async_receive_from(
            boost::asio::buffer(*recv_buffer), *remote_endpoint,
            [this, remote_endpoint, recv_buffer](boost::system::error_code ec, std::size_t bytes_recvd)
            {
                if (!ec && bytes_recvd > 0)
                {
                    process_request(bytes_recvd, *remote_endpoint, *recv_buffer);
                }
                start_receive();
            });
    }

    void process_request(std::size_t length, udp::endpoint remote_endpoint, const std::array<unsigned char, 32>& recv_buffer)
    {
        // Exit if it isn't a valid STUN Binding Request
        if (length < 20 ||
            recv_buffer[0] != 0x00 || recv_buffer[1] != 0x01 ||
            recv_buffer[4] != 0x21 || recv_buffer[5] != 0x12 ||
            recv_buffer[6] != 0xA4 || recv_buffer[7] != 0x42)
        {
            return;
        }

        std::array<unsigned char, 32> response = response_template;

        // Copy Transaction ID from the request
        std::copy(recv_buffer.begin() + 8, recv_buffer.begin() + 20, response.begin() + 8);

        // Calculate and fill in the Port XOR'd with magic cookie
        response[26] ^= remote_endpoint.port() >> 8;
        response[27] ^= remote_endpoint.port() & 0xFF;

        // Calculate and fill in the Address XOR'd with magic cookie
        auto address = remote_endpoint.address().to_v4().to_bytes();
        response[28] ^= address[0];
        response[29] ^= address[1];
        response[30] ^= address[2];
        response[31] ^= address[3];

        // Send the response
        socket_.async_send_to(
            boost::asio::buffer(response), remote_endpoint,
            [](boost::system::error_code /*ec*/, std::size_t /*bytes_sent*/) {});   
    }

    udp::socket socket_;
};


int main()
{
    const unsigned short port = 3478;
    const std::size_t num_threads = std::thread::hardware_concurrency();

    try
    {
        std::vector<boost::asio::io_context> io_contexts(num_threads);
        std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_work;
        std::vector<std::thread> io_threads;
        std::vector<std::shared_ptr<StunServer>> servers;

        for (auto &io_context : io_contexts)
        {
            io_work.emplace_back(boost::asio::make_work_guard(io_context));
        }

        for (auto &io_context : io_contexts)
        {
            servers.emplace_back(std::make_shared<StunServer>(io_context, port));
        }

        for (auto &io_context : io_contexts)
        {
            io_threads.emplace_back([&io_context]()
                                    { io_context.run(); });
        }

        for (auto &t : io_threads)
        {
            t.join();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
