//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: vdifmark5_stubs.cpp 5453 2013-06-28 02:13:50Z WalterBrisken $
// $HeadURL: $
// $LastChangedRevision: 5453 $
// $Author: WalterBrisken $
// $LastChangedDate: 2013-06-27 20:13:50 -0600 (Thu, 27 Jun 2013) $
//
//============================================================================
#include <mpi.h>
#include "mark5bmark5.h"
#include "alert.h"

Mark5BMark5DataStream::Mark5BMark5DataStream(const Configuration * conf, int snum, int id, int ncores, int * cids, int bufferfactor, int numsegments) : Mark5BDataStream(conf, snum, id, ncores, cids, bufferfactor, numsegments)
{
	cfatal << startl << "Mark5BMark5DataStream::Mark5BMark5DataStream stub called, meaning mpifxcorr was not compiled for native Mark5 support, but it was requested (with MODULE in .input file).  Aborting." << endl;
	MPI_Abort(MPI_COMM_WORLD, 1);
}

Mark5BMark5DataStream::~Mark5BMark5DataStream()
{
}

void Mark5BMark5DataStream::initialiseFile(int configindex, int fileindex)
{
}

void Mark5BMark5DataStream::openfile(int configindex, int fileindex)
{
}

int Mark5BMark5DataStream::calculateControlParams(int scan, int offsetsec, int offsetns)
{
	return -1;
}

int Mark5BMark5DataStream::sendMark5Status(enum Mk5State state, long long position, double dataMJD, float rate)
{
	return -1;
}

int Mark5BMark5DataStream::resetDriveStats()
{
	return -1;
}

int Mark5BMark5DataStream::reportDriveStats()
{
	return -1;
}
