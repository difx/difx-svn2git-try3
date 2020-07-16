#include <difxmessage.h>
#include <mpi.h>
#include <iostream>

#include "configuration.h"

int main(int argc, char** argv)
{
  MPI_Comm mpicomm = MPI_COMM_WORLD; // or MPI_COMM_SELF
  const char* difxMessageID = "test";
  int mpiid;

  const char* inputfilename = "nonexistent.input";
  if (argc == 2)
  {
    inputfilename = argv[1];
  }

  // Initialize MPI and difxmessage libary
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(mpicomm, &mpiid);
  difxMessageInit(mpiid, difxMessageID);
  difxMessageSetInputFilename(inputfilename);

  // With file load on all nodes
  Configuration* config = new Configuration(inputfilename, mpiid, 0.0);
  std::cout << "node#" << mpiid << ": creating local Configuration() from file " << inputfilename << "\n";

  // Done
  MPI_Finalize();

  //delete config; // causes segfault during dealloc, need to fix Configuration class

  return 0;
}
