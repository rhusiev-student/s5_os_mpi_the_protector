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
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <utility>
#include <vector>

using boost::asio::ip::tcp;

class MPITheProtector {
  public:
    int rank;
    int total;
    std::filesystem::path conf_name;
    bool shared_mem;

    std::vector<int> tcp_sockets;

    std::string shmname;
    int shm_fd;
    char *shm_addr;

    MPITheProtector(int &argc, char **(&argv));
    void establish_connections();

    template <typename T> void get_data(int connection, T &obj) {
        // for (int i = 0; i < total; i++) {
        //     std::cout << "TCP socket " << i << ": " << tcp_sockets[i]
        //               << std::endl;
        // }
        int buffer_size = sizeof(T);
        int buffer;

        int epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            std::cerr << "Failed to create epoll: " << strerror(errno)
                      << std::endl;
            exit(1);
        }

        epoll_event event[1];
        event[0].events = EPOLLIN;
        event[0].data.fd = tcp_sockets[connection];
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sockets[connection], &event[0]);

        int ret = epoll_wait(epoll_fd, event, 1, -1);
        if (ret < 0) {
            std::cerr << "Failed to wait for event: " << strerror(errno)
                      << std::endl;
            exit(1);
        }

        recv(tcp_sockets[connection], &buffer, buffer_size, 0);

        // std::cout << "Received " << buffer << std::endl;
        obj = buffer;
    }

    template <typename T> void send_data(int connection, T &&obj) {
        // for (int i = 0; i < total; i++) {
        //     std::cout << "TCP socket " << i << ": " << tcp_sockets[i]
        //               << std::endl;
        // }
        int buffer_size = sizeof(T);
        auto result = send(tcp_sockets[connection], &obj, buffer_size, 0);
        // std::cout << "fd: " << tcp_sockets[connection] << std::endl;
        if (result < 0) {
            std::cerr << "Failed to send data: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        // std::cout << "Sent " << obj << std::endl;
    }
};

#endif // INCLUDE_MPI_THE_PROTECTOR_HPP_
