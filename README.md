# coin-mining
The program requires openssl for SHA256 calculation needed in coin mining proof-of-work computation, and openmpi for distribution of work across the cluster. The number of threads per mpi process can be adjusted using the nThreads variable. When run, it'll dynamically disribute and parallelize the work on all the nodes in myHosts.txt file.

To compile: `mpic++ MiningMPI.cpp integer.cpp -lcrypto -lssl`

To run: `mpirun --hostfile myHosts.txt a.out`
