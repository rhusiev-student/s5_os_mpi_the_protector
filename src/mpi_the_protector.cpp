#include "./mpi_the_protector.hpp"
#include "./args.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <semaphore.h>
#include <string>
#include <sys/mman.h>
#include <utility>
#include <vector>

MPITheProtector::MPITheProtector(int &argc, char **(&argv)) {
    po::options_description opt_descr{"Usage: " + std::string(argv[0]) +
                                      " [-h|--help] <rank> <conf>"};
    opt_descr.add_options()("help,h", "Show help message");
    Args parser{argc, argv, opt_descr};
    if (parser.var_map.count("help")) {
        std::cout << opt_descr << std::endl;
        exit(0);
    }

    std::vector<std::string> unrecognized =
        po::collect_unrecognized(parser.parsed.options, po::include_positional);
    if (unrecognized.size() < 2) {
        std::cerr << "Exactly one rank and one conf file must be specified"
                  << std::endl;
        exit(1);
    }

    rank = std::stoi(unrecognized[0]);
    // std::cout << "Rank " << rank << std::endl;
    conf_name = unrecognized[1];
    establish_connections();

    argv[2] = argv[0];
    argv += 2;
    argc -= 2;
}

void MPITheProtector::establish_connections() {
    std::ifstream file(conf_name);
    std::string line;
    std::getline(file, line);
    shared_mem = line == "0";
    std::getline(file, line);
    total = std::stoi(line);

    if (shared_mem) {
        std::getline(file, line);
        shmname = line;
        establish_shm();
        return;
    }

    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    tcp_sockets.reserve(total);

    establish_tcp(lines);
}

void MPITheProtector::establish_shm() {
    shm_fd = shm_open(shmname.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Failed to open shared memory: " << strerror(errno)
                  << std::endl;
        exit(1);
    }
    if (ftruncate(shm_fd, total * total * PAIR_SIZE) == -1) {
        std::cerr << "Failed to truncate shared memory: " << strerror(errno)
                  << std::endl;
        exit(1);
    }
    shm_addr = mmap(nullptr, total * total * PAIR_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        std::cerr << "Failed to map shared memory: " << strerror(errno)
                  << std::endl;
        exit(1);
    }
    semaphores_send.reserve(total);
    semaphores_recv.reserve(total);
    for (int i = 0; i < total; i++) {
        if (i == rank) {
            semaphores_send.emplace_back(nullptr, nullptr);
            semaphores_recv.emplace_back(nullptr, nullptr);
            continue;
        }
        sem_t *sem_sent = sem_open(
            (shmname + std::to_string(rank * total + i) + "sent").c_str(),
            O_CREAT, 0666, 1);
        if (sem_sent == SEM_FAILED) {
            std::cerr << "Failed to open semaphore: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        sem_t *sem_recd = sem_open(
            (shmname + std::to_string(rank * total + i) + "recd").c_str(),
            O_CREAT, 0666, 0);
        if (sem_recd == SEM_FAILED) {
            std::cerr << "Failed to open semaphore: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        semaphores_send.emplace_back(sem_sent, sem_recd);
        int values[2];
        sem_getvalue(sem_sent, &values[0]);
        sem_getvalue(sem_recd, &values[1]);
        std::cout << "sem values: " << shmname << rank * total + i << ": "
                  << values[0] << ", " << values[1] << std::endl;
        sem_t *sem_sent1 =
            sem_open((shmname + std::to_string(i * total + rank) + "sent").c_str(),
                     O_CREAT, 0666, 1);
        if (sem_sent1 == SEM_FAILED) {
            std::cerr << "Failed to open semaphore: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        sem_t *sem_recd1 = sem_open(
            (shmname + std::to_string(i * total + rank) + "recd").c_str(),
            O_CREAT, 0666, 0);
        if (sem_recd1 == SEM_FAILED) {
            std::cerr << "Failed to open semaphore: " << strerror(errno)
                      << std::endl;
            exit(1);
        }
        semaphores_recv.emplace_back(sem_sent1, sem_recd1);
        sem_getvalue(sem_sent1, &values[0]);
        sem_getvalue(sem_recd1, &values[1]);
        std::cout << "sem values: " << shmname << i * total + rank << ": "
                  << values[0] << ", " << values[1] << std::endl;
    }
}

void MPITheProtector::establish_tcp(std::vector<std::string> &lines) {
    int i = 0;

    for (const auto &ip_line : lines) {
        if (i >= rank) {
            i++;
            continue;
        }
        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICSERV;

        addrinfo *to_connect;
        if (getaddrinfo(ip_line.c_str(),
                        std::to_string(12101 + rank * total + i).c_str(),
                        &hints, &to_connect) != 0) {
            std::cerr << "Failed to get address info" << std::endl;
            exit(1);
        }

        addrinfo *result_element;
        int socket_descriptor;
        for (result_element = to_connect; result_element != nullptr;
             result_element = result_element->ai_next) {
            socket_descriptor =
                socket(result_element->ai_family, result_element->ai_socktype,
                       result_element->ai_protocol);
            if (socket_descriptor == -1) {
                continue;
            }
            if (connect(socket_descriptor, result_element->ai_addr,
                        result_element->ai_addrlen) != -1) {
                break;
            }
            close(socket_descriptor);
        }
        if (result_element == nullptr) {
            std::cerr << "Failed to connect" << std::endl;
            exit(1);
        }
        // std::cout << "Connected to " << ip_line << ":"
        //           << 12101 + rank * total + i << std::endl;
        // std::cout << "Connected to " << socket_descriptor << std::endl;
        tcp_sockets.push_back(socket_descriptor);
        i++;
    }

    tcp_sockets.push_back(-1);

    i = 0;
    for (auto ip_line : lines) {
        if (i <= rank) {
            i++;
            continue;
        }
        sockaddr_in server;
        int socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_descriptor == -1) {
            std::cerr << "Socket creation error" << std::endl;
            exit(1);
        }
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(12101 + i * total + rank);

        int result =
            bind(socket_descriptor, (sockaddr *)&server, sizeof(server));
        if (result == -1) {
            std::cerr << "Bind error" << strerror(errno) << std::endl;
            exit(1);
        }
        auto res = listen(socket_descriptor, 1);
        if (res == -1) {
            std::cerr << "Listen error" << strerror(errno) << std::endl;
            exit(1);
        }
        // std::cout << "Listened on " << 12101 + i * total + rank << std::endl;
        int client_handler = accept(socket_descriptor, 0, 0);
        if (client_handler == -1) {
            std::cerr << "Accept error" << strerror(errno) << std::endl;
            exit(1);
        }
        close(socket_descriptor);
        tcp_sockets.push_back(client_handler);
        i++;
    }

    // if (rank == total - 1) {
    //
    // }
    // addrinfo hints;
    // memset(&hints, 0, sizeof(hints));
    // hints.ai_family = AF_INET;
    // hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags = AI_NUMERICSERV;
    //
    // addrinfo *to_connect;
    // if (getaddrinfo(lines[rank].c_str(),
    //                 std::to_string(10101 + rank * total + rank + 1).c_str(),
    //                 &hints, &to_connect) != 0) {
    //     std::cerr << "Failed to get address info" << std::endl;
    //     exit(1);
    // }
    //
    // addrinfo *result_element;
    // int socket_descriptor;
    // for (result_element = to_connect; result_element != nullptr;
    //      result_element = result_element->ai_next) {
    //     socket_descriptor =
    //         socket(result_element->ai_family, result_element->ai_socktype,
    //                result_element->ai_protocol);
    //     if (socket_descriptor == -1) {
    //         continue;
    //     }
    //     if (connect(socket_descriptor, result_element->ai_addr,
    //                 result_element->ai_addrlen) != -1) {
    //         break;
    //     }
    //     close(socket_descriptor);
    // }
    // if (result_element == nullptr) {
    //     std::cerr << "Failed to connect" << std::endl;
    //     exit(1);
    // }
    // tcp_barrier_socket = socket_descriptor;
}

void MPITheProtector::wait_barrier() {
    //
}
