#include <iostream>
#include <climits>
#include <openssl/sha.h>
#include <mpi.h>
#include "integer.h"
#include <chrono>
#include <thread> 

using namespace std;
int found = 0;
int myRank; 

string SHA256(const string str){
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);

    static const char characters[] = "0123456789abcdef";
  	std::string result (SHA256_DIGEST_LENGTH * 2, ' ');
  	for(int i = 0; i < SHA256_DIGEST_LENGTH; i++){
	    result[2*i] = characters[(unsigned int) hash[i] >> 4];
	    result[2*i+1] = characters[(unsigned int) hash[i] & 0x0F];
  	}
  	return result;
}
	
void powFunc(int threadId, string blockHash, string targetHash, int start, int end) {
  string tmp_hash;
  for (int nonce = start; nonce <= end; nonce++) {
    if(found == 1){
      break;
    }
    tmp_hash = SHA256(SHA256(blockHash + to_string(nonce)));
    if (targetHash.compare(tmp_hash) > 0) {
      MPI_Send(&nonce, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
      MPI_Send(tmp_hash.c_str(), tmp_hash.length(), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
      break;
    }
  }
  return;
}

void rootBcastFunc(int &nonce, char* resultHashBuf){
  MPI_Status status;

  MPI_Recv(&nonce, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
  MPI_Recv(resultHashBuf, 64, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);

  cout << "Nonce found on rank " << status.MPI_SOURCE << ", stopping all work." << endl;

  found = 1;
  MPI_Bcast(&found, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

void nonRootBcastFunc(){
  MPI_Bcast(&found, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

int main(int argc, char *argv[]){

	MPI_Init(&argc, &argv);
    
  int nProc;

  MPI_Comm_size(MPI_COMM_WORLD, &nProc);
	MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

  if(myRank == 0){
        cout << "Coin Mining MPI: Number of tasks = " << nProc << endl;
  }

  string tmpBlockHash, tmpTargetHash;
  int *sendS = new int[nProc];
  int *sendE = new int[nProc];
  int recvS, recvE;
  double totalNonce = 4294967295.0;
  int nodeWorkSize = (int) (totalNonce / nProc), threadWorkSize, nThreads = 1;
  double elapsedTime;
  int nonce;
  // number of blocks to be generated or number of rounds; default to 5
  int numberOfBlocks = 5;
  // average block generation time, default to 30 Secs.
  double avgBlockGenerationTimeInSec = 30.0;

  // init block hash
  string initBlockHash = SHA256("CSCI-654 Foundations of Parallel Computing");
  // init target hash
  string initTargetHash = "0000992a6893b712892a41e8438e3ff2242a68747105de0395826f60b38d88dc";

  if(myRank == 0){
    cout << "Initial Block Hash:  " << initBlockHash << endl;
    cout << "Initial Target Hash: " << initTargetHash << endl << endl;

  // Chunk up the work based on the number of workers available
    int temp = INT_MIN;
    for (int i = 0; i < nProc - 1; i++) {
      sendS[i] = temp;
      sendE[i] = temp + nodeWorkSize;
      temp = temp + nodeWorkSize;
    }

      sendS[nProc - 1] = temp;
      sendE[nProc - 1] = INT_MAX;
  }

  MPI_Scatter(sendS, 1, MPI_INT, &recvS, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Scatter(sendE, 1, MPI_INT, &recvE, 1, MPI_INT, 0, MPI_COMM_WORLD);

  threadWorkSize = (recvE - recvS)/nThreads;

  int currentBlockID = 1;
  tmpBlockHash = initBlockHash;
  tmpTargetHash = initTargetHash;
  chrono::steady_clock::time_point start;
  thread bcastThread;
  thread *powThreads = new thread[nThreads];
  char* resultHashBuf = new char[64];

  while (currentBlockID <= numberOfBlocks) {
    found = 0;

    if (myRank == 0) {
      nonce = 0;
      resultHashBuf = new char[64];      
      start = chrono::steady_clock::now();
      bcastThread = thread(rootBcastFunc, ref(nonce), ref(resultHashBuf));
    }else{
      bcastThread = thread(nonRootBcastFunc);
    }

    for (int i=0; i < nThreads-1; i++){ 
      powThreads[i] = thread(powFunc, i, tmpBlockHash, tmpTargetHash, recvS + threadWorkSize*i, recvS + threadWorkSize*(i+1));
    }
    powThreads[nThreads-1] = thread(powFunc, nThreads-1, tmpBlockHash, tmpTargetHash, recvS + threadWorkSize*(nThreads-1), recvE);
    
    for (int i=0; i < nThreads; i++){
      powThreads[i].join();
    }

    bcastThread.join();

    if (myRank == 0) {
      long nano_seconds = chrono::duration_cast<std::chrono::nanoseconds>(chrono::steady_clock::now() - start).count();
      elapsedTime = (double)nano_seconds / 1000000000;
      cout << "Time difference = " << elapsedTime << endl;
      cout << "Nonce: " << nonce << endl;
      cout << "Resultant Hash: " << string(resultHashBuf) << endl;
    }

    // found a new block
    tmpBlockHash = SHA256(tmpBlockHash + "|" + to_string(nonce));

    MPI_Bcast(&elapsedTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // update the target
    integer newHash(tmpTargetHash, 16);
    if (elapsedTime < avgBlockGenerationTimeInSec) {
      newHash = newHash / 2;
      tmpTargetHash = newHash.str(16, tmpTargetHash.length());
    } else {
      newHash = newHash * 2;
      tmpTargetHash = newHash.str(16, tmpTargetHash.length());
    }

  if(myRank == 0){
    cout << "New Block Hash: " << tmpBlockHash << endl;
    cout << "New Target Hash: " << tmpTargetHash << endl << endl;
  }

   currentBlockID++;
  }

  MPI_Finalize();
  return 0;
}
