import argparse
import asyncio

# parse the first two arguments: first as int, second as string
parser = argparse.ArgumentParser()
parser.add_argument("conf", type=str)
args = parser.parse_args()

with open(args.conf) as file:
    file.readline()
    rank = int(file.readline())

processes = []


async def main():
    for i in range(rank):
        processes.append(
            await asyncio.create_subprocess_exec(
                "./bin/mpi_the_protector",
                str(i),
                args.conf,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
        )

    for i in range(rank):
        stdout, stderr = await processes[i].communicate()
        print("Process", i)
        print(stdout.decode())
        print(stderr.decode())
        print("===")


asyncio.run(main())
