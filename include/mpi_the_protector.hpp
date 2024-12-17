#ifndef INCLUDE_MPI_THE_PROTECTOR_HPP_
#define INCLUDE_MPI_THE_PROTECTOR_HPP_

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/streambuf.hpp>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using boost::asio::ip::tcp;

class MPITheProtector {
  public:
    int rank;
    int total;
    std::filesystem::path conf_name;
    bool shared_mem;

    boost::asio::io_context io_context_send;
    boost::asio::io_context io_context_accept;
    tcp::resolver resolver;
    std::vector<tcp::socket> tcp_sockets;
    std::vector<std::pair<tcp::acceptor, tcp::socket>> accept;

    std::string shmname;
    int shm_fd;
    char *shm_addr;

    MPITheProtector(int &argc, char **(&argv));
    void establish_connections();

    // template <typename T>
    void get_data(int connection, int &obj) {
        size_t buffer_size = sizeof(int);
        // std::array<int, 1> *buffer = new std::array<int, 1>();
        int *buffer = new int[1];
        accept[connection].second.async_read_some(
            boost::asio::buffer(buffer, sizeof(int)),
            [buffer, &buffer_size, &obj](boost::system::error_code ec,
                                 size_t bytes_transferred) {
                if (ec) {
                    std::cerr << "Error getting data: " << ec.message()
                              << std::endl;
                    exit(1);
                }
                if (bytes_transferred != buffer_size) {
                    std::cerr << "Error getting data: wrong size" << std::endl;
                    exit(1);
                }
                std::cout << "Received " << buffer[0] << std::endl;
                memcpy(&obj, buffer, buffer_size);
                delete[] buffer;
            });
        io_context_accept.run_one();
    }

    template <typename T> void send_data(int connection, T &&obj) {
        size_t buffer_size = sizeof(T);
        std::array<T, 1> buffer = {obj};
        // memcpy(buffer.data(), &obj, buffer_size);
        tcp_sockets[connection].async_write_some(
            boost::asio::buffer(buffer),
            [&buffer_size](boost::system::error_code ec,
                           size_t bytes_transferred) {
                if (ec) {
                    std::cerr << "Error sending data: " << ec.message()
                              << std::endl;
                    exit(1);
                }
                if (bytes_transferred != buffer_size) {
                    std::cerr << "Error sending data: wrong size" << std::endl;
                    exit(1);
                }
            });
        io_context_send.run_one();
    }
};

#endif // INCLUDE_MPI_THE_PROTECTOR_HPP_
