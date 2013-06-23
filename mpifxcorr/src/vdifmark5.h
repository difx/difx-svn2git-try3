/***************************************************************************
 *   Copyright (C) 2007-2012 by Walter Brisken and Adam Deller             *
 *                                                                         *
 *   This program is free for non-commercial use: see the license file     *
 *   at http://astronomy.swin.edu.au:~adeller/software/difx/ for more      *
 *   details.                                                              *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: nativemk5.h 4699 2012-07-08 18:53:57Z WalterBrisken $
// $HeadURL: https://svn.atnf.csiro.au/difx/mpifxcorr/trunk/src/nativemk5.h $
// $LastChangedRevision: 4699 $
// $Author: WalterBrisken $
// $LastChangedDate: 2012-07-08 12:53:57 -0600 (Sun, 08 Jul 2012) $
//
//============================================================================

#ifndef VDIFMARK5_H
#define VDIFMARK5_H

#include <pthread.h>
#include <time.h>
#include "mode.h"
#include "datastream.h"
#include "config.h"

#ifdef HAVE_XLRAPI_H
#include "mark5dir.h"
#endif

#include "vdiffile.h"
#include <difxmessage.h>

class VDIFMark5DataStream : public VDIFDataStream
{
public:
	VDIFMark5DataStream(const Configuration * conf, int snum, int id, int ncores, int * cids, int bufferfactor, int numsegments);
	virtual ~VDIFMark5DataStream();
	virtual void initialiseFile(int configindex, int fileindex);
	virtual void openfile(int configindex, int fileindex);
	virtual int calculateControlParams(int scan, int offsetsec, int offsetns);
	virtual void diskToMemory(int buffersegment);
	int moduleRead(unsigned long * destination, int nbytes, long long start, int buffersegment);
	int sendMark5Status(enum Mk5State state, long long position, double dataMJD, float rate);
	int resetDriveStats();
	int reportDriveStats();

protected:
#ifdef HAVE_XLRAPI_H
	void setDiscModuleState(SSHANDLE xlrDevice, const char *newState);
#endif

private:
#ifdef HAVE_XLRAPI_H
	Mark5Module module;
	int scanNum;
	const Mark5Scan *scanPointer;
	long long readpointer;
	SSHANDLE xlrDevice;
#endif

	DifxMessageMk5Status mk5status;

	int invalidtime;
	int filltime;
	long long invalidstart;
	unsigned long lastval;
	int newscan;
	double lastrate;
	int nrate;
	int nError;
	int nDMAError;
	bool noMoreData;
	bool noDataOnModule;
	int readDelayMicroseconds;
	int nReads;

	void openStreamstor();
	void closeStreamstor();
	void resetStreamstor();
};

#endif
