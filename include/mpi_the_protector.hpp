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
#include <semaphore.h>
#include <string>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <utility>
#include <vector>

#define PAIR_SIZE sizeof(int) * 128
#define PAIR_SIZE_INTS 128

struct Await {
    int epoll_fd;
    epoll_event event[1];
};

class MPITheProtector {
  public:
    int rank;
    int total;
    std::filesystem::path conf_name;
    bool shared_mem;

    std::vector<int> tcp_sockets;
    int tcp_barrier_socket;

    std::string shmname;
    int shm_fd;
    void *shm_addr;
    std::vector<std::pair<sem_t *, sem_t *>> semaphores_send;
    std::vector<std::pair<sem_t *, sem_t *>> semaphores_recv;

    bool barrier = false;

    MPITheProtector(int &argc, char **(&argv));
    void establish_connections();
    void establish_tcp(std::vector<std::string> &lines);
    void establish_shm();

    void wait_barrier();

    template <typename T> void get_data(int connection, T &obj) {
        if (shared_mem) {
            get_data_shm(connection, obj);
        } else {
            get_data_tcp(connection, obj);
        }
    }

    template <typename T> void send_data(int connection, T &&obj) {
        if (shared_mem) {
            send_data_shm(connection, obj);
        } else {
            send_data_tcp(connection, obj);
        }
    }

    template <typename T> void get_data_tcp(int connection, T &obj) {
        Await *await = aget_data_tcp<T>(connection);
        await_get_tcp(obj, await);
    }

    template <typename T> void send_data_tcp(int connection, T &&obj) {
        Await *await = asend_data_tcp<T>(connection);
        await_send_tcp(obj, await);
    }

    template <typename T> Await *aget_data_tcp(int connection) {
        int buffer_size = sizeof(T);
        Await *await = new Await();
        await->epoll_fd = epoll_create1(0);
        if (await->epoll_fd < 0) {
            std::cerr << "Failed to create epoll: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        await->event[0].events = EPOLLIN;
        await->event[0].data.fd = tcp_sockets[connection];
        epoll_ctl(await->epoll_fd, EPOLL_CTL_ADD, tcp_sockets[connection],
                  &await->event[0]);
        return await;
    }

    template <typename T> void await_get_tcp(T &&obj, Await *await) {
        int ret = epoll_wait(await->epoll_fd, await->event, 1, -1);
        if (ret < 0) {
            std::cerr << "Failed to wait for event: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        recv(await->event[0].data.fd, &obj, sizeof(T), 0);
        delete await;
    }

    template <typename T> Await *asend_data_tcp(int connection) {
        int buffer_size = sizeof(T);
        Await *await = new Await();
        await->epoll_fd = epoll_create1(0);
        if (await->epoll_fd < 0) {
            std::cerr << "Failed to create epoll: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        await->event[0].events = EPOLLOUT;
        await->event[0].data.fd = tcp_sockets[connection];
        epoll_ctl(await->epoll_fd, EPOLL_CTL_ADD, tcp_sockets[connection],
                  &await->event[0]);
        return await;
    }

    template <typename T> void await_send_tcp(T &&obj, Await *await) {
        int ret = epoll_wait(await->epoll_fd, await->event, 1, -1);
        if (ret < 0) {
            std::cerr << "Failed to wait for event: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        send(await->event[0].data.fd, &obj, sizeof(T), 0);
        delete await;
    }

    bool is_ready_tcp(Await *await) {
        int ret = epoll_wait(await->epoll_fd, await->event, 1, 0);
        if (ret < 0) {
            std::cerr << "Failed to wait for event: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        return ret > 0;
    }

    template <typename T> void get_data_shm(int connection, T &obj) {
        if (sizeof(T) > PAIR_SIZE) {
            std::cerr << "Too big object" << std::endl;
            exit(1);
        }
        int ret;
        while ((ret = sem_wait(semaphores_recv[connection].second)) == -1) {
            if (errno != EINTR) {
                std::cerr << "Failed to wait for semaphore: " << strerror(errno)
                          << std::endl;
                exit(1);
            }
        }
        memcpy(&obj,
               &(static_cast<int *>(
                   shm_addr)[(connection * total + rank) * PAIR_SIZE_INTS]),
               sizeof(T));
        sem_post(semaphores_recv[connection].first);
    }

    template <typename T> void send_data_shm(int connection, T &&obj) {
        if (sizeof(T) > PAIR_SIZE) {
            std::cerr << "Too big object" << std::endl;
            exit(1);
        }

        int ret;
        while ((ret = sem_wait(semaphores_send[connection].first)) == -1) {
            if (errno != EINTR) {
                std::cerr << "Failed to wait for semaphore: " << strerror(errno)
                          << std::endl;
                exit(1);
            }
        }
        memcpy(&(static_cast<int *>(
                   shm_addr)[(rank * total + connection) * PAIR_SIZE_INTS]),
               &obj, sizeof(T));
        sem_post(semaphores_send[connection].second);
    }

    ~MPITheProtector() {
        if (shared_mem) {
            shm_unlink(shmname.c_str());
            for (int i = 0; i < total; i++) {
                if (i == rank) {
                    continue;
                }
                sem_close(semaphores_send[i].first);
                sem_close(semaphores_send[i].second);
                sem_close(semaphores_recv[i].first);
                sem_close(semaphores_recv[i].second);
                sem_unlink((shmname + std::to_string(rank * total + i) + "sent")
                               .c_str());
                sem_unlink((shmname + std::to_string(rank * total + i) + "recd")
                               .c_str());
                sem_unlink((shmname + std::to_string(i * total + rank) + "sent")
                               .c_str());
                sem_unlink((shmname + std::to_string(i * total + rank) + "recd")
                               .c_str());
            }
            munmap(shm_addr, total * total * PAIR_SIZE);
            close(shm_fd);
        } else {
            for (int i = 0; i < total; i++) {
                close(tcp_sockets[i]);
            }
        }
    }
};

#endif // INCLUDE_MPI_THE_PROTECTOR_HPP_
