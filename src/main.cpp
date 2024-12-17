#include "./mpi_the_protector.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    MPITheProtector mpi = MPITheProtector(argc, argv);
    
    if (mpi.rank == 0) {
        std::cout << "Process 0 preparing to send..." << std::endl;
        mpi.send_data(1, 1337);
        std::cout << "Process 0 sent 1337" << std::endl;
    }

    if (mpi.rank == 1) {
        std::cout << "Process 1 preparing to receive..." << std::endl;
        int data = 0;
        mpi.get_data(0, data);
        std::cout << "Process 1 received " << data << std::endl;
    }
    return 0;
}
