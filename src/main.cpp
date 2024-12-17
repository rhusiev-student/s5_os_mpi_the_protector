#include "./mpi_the_protector.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    MPITheProtector mpi = MPITheProtector(argc, argv);

    if (mpi.rank == 1) {
        std::cout << "Process 1 preparing to send..." << std::endl;
        mpi.send_data(0, 1337);
        // Await *await = mpi.asend_data_tcp<int>(0);
        // std::cout << "Process 1 started sending 1337" << std::endl;
        // mpi.await_send_tcp(1337, await);
        std::cout << "Process 1 sent 1337" << std::endl;
    }

    if (mpi.rank == 0) {
        std::cout << "Process 0 preparing to receive..." << std::endl;
        int data = 0;
        mpi.get_data(1, data);
        // Await *await = mpi.aget_data_tcp<int>(1);
        // std::cout << "Process 0 started receiving" << std::endl;
        // mpi.await_get_tcp(data, await);
        std::cout << "Process 0 received " << data << std::endl;
    }
    return 0;
}
