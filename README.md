create MPITheProtector (see --help)

if the first line in the file is 0 - shm, otherwise - tcp

T data;
then use get_data(num_of_node_to_receive_from, data) to get data into data (and wait)

and use send_data(num_of_node_to_send_to, data) to send data

or you can use await = aget_data(num_of_node_to_receive_from); then await_get(await, data) to get data when it's ready

same with asend_data and await_send

Unfortunately, await does not work with shared memory - only tcp

Unfortunately, barier does not work (at least at the time of writing. mb I did it in the last 5 min and did not fill here)

For a simple example, try launching the main program

to compile, `./compile.sh -d` (for debug) or `-o` for release (optimized).

`./bin/mpi_the_protector` to launch the example

you can modify `./src/main.cpp` to test your use cases of the library
