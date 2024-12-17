#include "./mpi_the_protector.hpp"
#include "./args.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

MPITheProtector::MPITheProtector(int &argc, char **(&argv))
    : resolver(io_context_send) {
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

    if (shared_mem) {
        std::getline(file, line);
        shmname = line;
        return;
    }

    total = 0;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
        total++;
    }
    tcp_sockets.reserve(total);
    accept.reserve(total);
    int i = 0;
    for (const auto &ip_line : lines) {
        tcp::acceptor acceptor(
            io_context_accept,
            tcp::endpoint(tcp::v4(), 12101 + rank * total + i));
        accept.emplace_back(
            std::make_pair(std::move(acceptor), tcp::socket(io_context_send)));
        accept[i].first.async_accept(
            accept[i].second,
            [this, i, ip_line](boost::system::error_code ec) {
                if (ec) {
                    std::cerr << "Can't accept from " << i << " (" << ip_line
                              << ":" << 12101 + rank * total + i
                              << "): " << ec.message() << std::endl;
                    exit(1);
                }
                std::cout << "I can accept from "
                          << i << "(" << ip_line << ":"
                          << 12101 + rank * total + i << ")" << std::endl;
            });

        tcp_sockets.emplace_back(io_context_send);
        i++;
    }

    i = 0;
    for (auto ip_line : lines) {
        if (i == rank) {
            i++;
            continue;
        }
        auto endpoint =
            resolver.resolve(ip_line, std::to_string(12101 + i * total + rank));
        int tries = 0;
        while (tries < 10) {
            try {
                boost::asio::connect(tcp_sockets[i], endpoint);
                break;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                tries++;
            }
        }
        if (tries == 10) {
            std::cerr << "Couldn't connect to " << i << "(" << ip_line << ":"
                      << 12101 + i * total + rank << ")" << std::endl;
            exit(1);
        }
        std::cout << "I can access " << i << "("
                  << ip_line << ":" << 12101 + i * total + rank << ")"
                  << std::endl;
        i++;
    }
    for (int j = 0; j < total - 1; j++) {
        io_context_accept.run_one();
    }
}
