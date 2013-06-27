/***************************************************************************
 *   Copyright (C) 2007-2013 by Walter Brisken and Adam Deller             *
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
// $Id: nativemk5.cpp 5029 2012-12-05 18:07:39Z WalterBrisken $
// $HeadURL: https://svn.atnf.csiro.au/difx/mpifxcorr/trunk/src/nativemk5.cpp $
// $LastChangedRevision: 5029 $
// $Author: WalterBrisken $
// $LastChangedDate: 2012-12-05 11:07:39 -0700 (Wed, 05 Dec 2012) $
//
//============================================================================

#include <cstring>
#include <cstdlib>
#include <ctype.h>
#include <cmath>
#include <sys/time.h>
#include <mpi.h>
#include <unistd.h>
#include <vdifio.h>
#include "config.h"
#include "vdifmark5.h"
#include "watchdog.h"
#include "alert.h"

#if HAVE_MARK5IPC
#include <mark5ipc.h>
#endif

static int dirCallback(int scan, int nscan, int status, void *data)
{
	const int MessageLength=200;
	char message[MessageLength];
	int v;
	DifxMessageMk5Status *mk5status;

	mk5status = reinterpret_cast<DifxMessageMk5Status *>(data);
	mk5status->scanNumber = scan + 1;
	mk5status->position = nscan;
	v = snprintf(mk5status->scanName, MODULE_SCAN_NAME_LENGTH, "%s", Mark5DirDescription[status]);
	if(v >= MessageLength)
	{
		fprintf(stderr, "Warning: dirCallback: scanName: v=%d >= %d\n", v, MODULE_SCAN_NAME_LENGTH);
	}

	difxMessageSendMark5Status(mk5status);

	if(status == MARK5_DIR_READ_ERROR)
	{
		v = snprintf(message, MessageLength, "XLR read error in decoding of scan %d\n", scan+1);
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
	}
	else if(status == MARK5_DIR_DECODE_ERROR)
	{
		v = snprintf(message, MessageLength, "cannot decode scan %d\n", scan+1);
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
	}

	if(v >= MessageLength)
	{
		fprintf(stderr, "Warning: dirCallback: message: v=%d, >= %d\n", v, MessageLength);
	}

	return 0;
}

int VDIFMark5DataStream::resetDriveStats()
{
	S_DRIVESTATS driveStats[XLR_MAXBINS];
	const int defaultStatsRange[] = { 75000, 150000, 300000, 600000, 1200000, 2400000, 4800000, -1 };

	WATCHDOGTEST( XLRSetOption(xlrDevice, SS_OPT_DRVSTATS) );
	for(int b = 0; b < XLR_MAXBINS; ++b)
	{
		driveStats[b].range = defaultStatsRange[b];
		driveStats[b].count = 0;
	}
	WATCHDOGTEST( XLRSetDriveStats(xlrDevice, driveStats) );

	return 0;
}

int VDIFMark5DataStream::reportDriveStats()
{
	XLR_RETURN_CODE xlrRC;
	S_DRIVESTATS driveStats[XLR_MAXBINS];
	DifxMessageDriveStats driveStatsMessage;

	snprintf(driveStatsMessage.moduleVSN, DIFX_MESSAGE_MARK5_VSN_LENGTH+1, "%s",  datafilenames[0][0].c_str());
	driveStatsMessage.type = DRIVE_STATS_TYPE_READ;

	/* FIXME: for now don't include complete information on drives */
	strcpy(driveStatsMessage.serialNumber, "");
	strcpy(driveStatsMessage.modelNumber, "");
	driveStatsMessage.diskSize = 0;
	driveStatsMessage.startByte = 0;

	for(int d = 0; d < 8; ++d)
	{
		for(int i = 0; i < DIFX_MESSAGE_N_DRIVE_STATS_BINS; ++i)
		{
			driveStatsMessage.bin[i] = -1;
		}
		driveStatsMessage.moduleSlot = d;
		WATCHDOG( xlrRC = XLRGetDriveStats(xlrDevice, d/2, d%2, driveStats) );
		if(xlrRC == XLR_SUCCESS)
		{
			for(int i = 0; i < XLR_MAXBINS; ++i)
			{
				if(i < DIFX_MESSAGE_N_DRIVE_STATS_BINS)
				{
					driveStatsMessage.bin[i] = driveStats[i].count;
				}
			}
		}
		difxMessageSendDriveStats(&driveStatsMessage);
	}

	resetDriveStats();

	return 0;
}

void VDIFMark5DataStream::openStreamstor()
{
	XLR_RETURN_CODE xlrRC;

	sendMark5Status(MARK5_STATE_OPENING, 0, 0.0, 0.0);

	WATCHDOG( xlrRC = XLROpen(1, &xlrDevice) );
  
  	if(xlrRC == XLR_FAIL)
	{
#if HAVE_MARK5IPC
                unlockMark5();
#endif
		WATCHDOG( XLRClose(xlrDevice) );
		cfatal << startl << "Cannot open Streamstor device.  Either this Mark5 unit has crashed, you do not have read/write permission to /dev/windrvr6, or some other process has full control of the Streamstor device." << endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	// FIXME: for non-bank-mode operation, need to look at the modules to determine what to do here.
	WATCHDOG( xlrRC = XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	if(xlrRC != XLR_SUCCESS)
	{
		cerror << startl << "Cannot put Mark5 unit in bank mode" << endl;
	}

	sendMark5Status(MARK5_STATE_OPEN, 0, 0.0, 0.0);
}

void VDIFMark5DataStream::closeStreamstor()
{
	sendMark5Status(MARK5_STATE_CLOSE, 0, 0.0, 0.0);
	WATCHDOG( XLRClose(xlrDevice) );
}

void VDIFMark5DataStream::resetStreamstor()
{
	sendMark5Status(MARK5_STATE_RESETTING, 0, 0.0, 0.0);
	WATCHDOG( XLRReset(xlrDevice) );
}

VDIFMark5DataStream::VDIFMark5DataStream(const Configuration * conf, int snum, int id, int ncores, int * cids, int bufferfactor, int numsegments) :
		VDIFDataStream(conf, snum, id, ncores, cids, bufferfactor, numsegments)
{
	int perr;

	/* each data buffer segment contains an integer number of frames, 
	 * because thats the way config determines max bytes
	 */

	scanNum = -1;
	readpointer = -1;
	scanPointer = 0;
	lastval = 0xFFFFFFFF;
	filltime = 0;
	invalidtime = 0;
	invalidstart = 0;
	newscan = 0;
	lastrate = 0.0;
	noMoreData = false;
	nrate = 0;
	nError = 0;
	nDMAError = 0;
	readDelayMicroseconds = 0;
	noDataOnModule = false;
	nReads = 0;

	readbufferslots = 8;
	readbufferslotsize = (bufferfactor/numsegments)*conf->getMaxDataBytes(streamnum)*21/10;
	readbufferslotsize -= (readbufferslotsize % 8); // make it a multiple of 8 bytes
	readbuffersize = readbufferslots * readbufferslotsize;
	// Note: the read buffer is allocated in vdiffile.cpp by VDIFDataStream::initialse()
	// the above values override defaults for file-based VDIF


mutexstate = new char[readbufferslots+1];

for(int i = 0; i < readbufferslots; ++i)
{
mutexstate[i] = '.';
}
mutexstate[readbufferslots] = 0;

#if HAVE_MARK5IPC
	perr = lockMark5(5);
	{
		if(perr)
		{
			sendMark5Status(MARK5_STATE_ERROR, 0, 0.0, 0.0);
			++nError;
			cfatal << startl << "Cannot obtain lock for Streamstor device." << endl;
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
	}
#endif
	openStreamstor();

        // Start up mark5 watchdog thread
        perr = initWatchdog();
        if(perr != 0)
        {
                cfatal << startl << "Cannot create the nativemk5 watchdog thread!" << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
        }

	resetDriveStats();


	// set up Mark5 module reader thread
	mark5xlrfail = false;
	mark5threadstop = false;
	lockstart = lockend = lastslot = -2;
	endindex = 0;

	perr = pthread_barrier_init(&mark5threadbarrier, 0, 2);
	mark5threadmutex = new pthread_mutex_t[readbufferslots];
	for(unsigned int m = 0; m < readbufferslots; ++m)
	{
		if(perr == 0)
		{
			perr = pthread_mutex_init(mark5threadmutex + m, 0);
		}
	}

	if(perr == 0)
	{
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		perr = pthread_create(&mark5thread, &attr, VDIFMark5DataStream::launchmark5threadfunction, this);
		pthread_attr_destroy(&attr);
	}

	if(perr)
	{
		cfatal << startl << "Cannot create the Mark5 reader thread!" << endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
}

VDIFMark5DataStream::~VDIFMark5DataStream()
{
	mark5threadstop = true;

	pthread_barrier_wait(&mark5threadbarrier);
	cout << "Master barrier passed " << mpiid << endl;

	pthread_join(mark5thread, 0);

	reportDriveStats();
	closeStreamstor();
#if HAVE_MARK5IPC
	unlockMark5();
#endif

	for(unsigned int m = 0; m < readbufferslots; ++m)
	{
		pthread_mutex_destroy(mark5threadmutex + m);
	}
	delete [] mark5threadmutex;
	pthread_barrier_destroy(&mark5threadbarrier);

	if(readDelayMicroseconds > 0)
	{
		cinfo << startl << "To reduce read rate in RT mode, read delay was set to " << readDelayMicroseconds << " microseconds" << endl;
	}

	if(nError > 0)
	{
		cwarn << startl << nError << " errors were encountered reading this module" << endl;
	}

        // stop watchdog thread
        stopWatchdog();
}

void VDIFMark5DataStream::mark5threadfunction()
{
	cout << "Thread started " << mpiid << endl;

	for(;;)
	{
		// No locks shall be set at this point

		readbufferwriteslot = 1;	// always 
		pthread_mutex_lock(mark5threadmutex + readbufferwriteslot);
		mutexstate[readbufferwriteslot] = 'w';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
		cinfo << startl << "vdifmark5datastream " << mpiid << " start of scan reached.  Locked " << readbufferwriteslot << endl;
		pthread_barrier_wait(&mark5threadbarrier);
		cout << "Slave barrier passed " << mpiid << endl;

		while(!mark5threadstop)
		{
			unsigned long a, b;
			int bytes;
			XLR_RETURN_CODE xlrRC;
			S_READDESC      xlrRD;
			XLR_ERROR_CODE  xlrEC;
			char errStr[XLR_ERROR_LENGTH];
			bool endofscan = false;


			// Bytes to read.  In most cases the following is desired, but when the end of the scan nears it changes
			bytes = readbufferslotsize;

			// if we're starting, jump out of the loop.  Really this should not be executed
			if(readpointer >= readend)
			{
				endofscan = true;
				lastslot = readbufferwriteslot;
				endindex = lastslot*readbufferslotsize;
				cwarn << startl << "Developer error: mark5threadfunction: readpointer >= readend" << endl;
cinfo << startl << "lastslot[1]=" << lastslot << " endindex=" << endindex << endl;

				break;
			}

			// if this will be the last read of the scan, shorten if necessary
			if(readpointer + bytes > readend)
			{
				int origbytes = bytes;
				bytes = readend - readpointer;
				bytes -= (bytes % 8);		// enforce 8 byte multiple length
				endofscan = true;

				lastslot = readbufferwriteslot;
cinfo << startl << "lastslot[2]=" << lastslot << " endindex=" << endindex << endl;
				endindex = lastslot*readbufferslotsize + bytes;	// No data in this slow from here to end

				cinfo << startl << "At end of scan: shortening read to only " << bytes << " bytes " << "(was " << origbytes << ")" << endl;
			}

			// remember that all reads of a module must be 64 bit aligned
			a = readpointer >> 32;
			b = readpointer & 0xFFFFFFF8; 	// enforce 8 byte boundary
			bytes -= (bytes % 8);		// enforce 8 byte multiple length

			// set up the XLR info
			xlrRD.AddrHi = a;
			xlrRD.AddrLo = b;
			xlrRD.XferLength = bytes;
			xlrRD.BufferAddr = reinterpret_cast<streamstordatatype *>(readbuffer + readbufferwriteslot*readbufferslotsize);

			cinfo << startl << "XLRRead " << mpiid << " bytes=" << bytes << "/" << readbufferslotsize << " readbufferslotsize=" << readbufferslotsize << " buf offset=" << (readbufferwriteslot*readbufferslotsize) << endl;

			// delay the read if needed
			if(readDelayMicroseconds > 0)
			{
				usleep(readDelayMicroseconds);
			}

			//execute the XLR read
			WATCHDOG( xlrRC = XLRReadData(xlrDevice, xlrRD.BufferAddr, xlrRD.AddrHi, xlrRD.AddrLo, xlrRD.XferLength) );

			if(xlrRC != XLR_SUCCESS)
			{
				xlrEC = XLRGetLastError();
				XLRGetErrorMessage(errStr, xlrEC);

		#warning "FIXME: use error code when known"
				if(strncmp(errStr, "DMA Timeout", 11) == 0)	// A potentially recoverable issue 
				{
					++nDMAError;
					cwarn << startl << "Cannot read data from Mark5 module: position=" << readpointer << ", length=" << bytes << ", XLRErrorCode=" << xlrEC << ", error=" << errStr << endl;
					cwarn << startl << "This is DMA error number " << nDMAError << " on this unit in this job." << endl;
					cwarn << startl << "Resetting streamstor card..." << endl;

					sleep(1);

					// try to reset card
					resetStreamstor();
				}
				else
				{
					cerror << startl << "Cannot read data from Mark5 module: position=" << readpointer << ", length=" << bytes << ", XLRErrorCode=" << xlrEC << ", error=" << errStr << endl;
					dataremaining = false;
					keepreading = false;

					double errorTime = corrstartday + (readseconds + corrstartseconds + readnanoseconds*1.0e-9)/86400.0;
					sendMark5Status(MARK5_STATE_ERROR, readpointer, errorTime, 0.0);
					++nError;
					mark5xlrfail = true;
				}

				return;
			}
			else
			{
				int curslot = readbufferwriteslot;

				readpointer += bytes;
				++nReads;

				++readbufferwriteslot;
				if(readbufferwriteslot >= readbufferslots)
				{
					// Note: we always save slot 0 for wrap-around
					readbufferwriteslot = 1;
				}
				pthread_mutex_lock(mark5threadmutex + readbufferwriteslot);
		mutexstate[readbufferwriteslot] = 'w';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
				pthread_mutex_unlock(mark5threadmutex + curslot);
		mutexstate[curslot] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;

				cinfo << startl << "vdifmark5datastream " << mpiid << " exhanged locks from " << curslot << " to " << readbufferwriteslot << endl;
			}
			if(endofscan)
			{
				break;
			}
		}
		pthread_mutex_unlock(mark5threadmutex + readbufferwriteslot);
		mutexstate[readbufferwriteslot] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
		cinfo << startl << "vdifmark5datastream " << mpiid << " end of scan reached.  Unlocked " << readbufferwriteslot << endl;
		if(mark5threadstop)
		{
			break;
		}

		// No locks shall be set at this point
	} 
}

void *VDIFMark5DataStream::launchmark5threadfunction(void *self)
{
	VDIFMark5DataStream *me = (VDIFMark5DataStream *)self;

	cout << "Launching " << me->mpiid << endl;

	me->mark5threadfunction();

	return 0;
}

int VDIFMark5DataStream::calculateControlParams(int scan, int offsetsec, int offsetns)
{
	static int last_offsetsec = -1;
	int r;
	
	// call parent class equivalent function and store return value
	r = VDIFDataStream::calculateControlParams(scan, offsetsec, offsetns);

	// check to see if we should send a status update
	if(bufferinfo[atsegment].controlbuffer[bufferinfo[atsegment].numsent][1] == Mode::INVALID_SUBINT)
	{
		// Every second (of data time) send a reminder to the operator
		if(last_offsetsec > -1 && offsetsec != last_offsetsec)
		{
			double mjd = corrstartday + (corrstartseconds+offsetsec)/86400.0;
			
			// NO DATA implies that things are still good, but there is no data to be sent.
			sendMark5Status(MARK5_STATE_NODATA, 0, mjd, 0.0);
		}
		last_offsetsec = offsetsec;
	}
	else  // set last value to indicate that this interval contained data.
	{
		last_offsetsec = -1;
	}

	// return parent class equivalent function return value
	return r;
}

/* Here "File" is VSN */
void VDIFMark5DataStream::initialiseFile(int configindex, int fileindex)
{
	int nrecordedbands, fanout;
	Configuration::datasampling sampling;
	Configuration::dataformat format;
	double bw;
	int rv;

	char defaultDirPath[] = ".";
	double startmjd;
	long long n;
	int doUpdate = 0;
	char *mk5dirpath;
	XLR_RETURN_CODE xlrRC;

	format = config->getDataFormat(configindex, streamnum);
	sampling = config->getDSampling(configindex, streamnum);
	nbits = config->getDNumBits(configindex, streamnum);
	nrecordedbands = config->getDNumRecordedBands(configindex, streamnum);
	inputframebytes = config->getFrameBytes(configindex, streamnum);
	framespersecond = config->getFramesPerSecond(configindex, streamnum)/config->getDNumMuxThreads(configindex, streamnum);
        bw = config->getDRecordedBandwidth(configindex, streamnum, 0);

	nGap = framespersecond/4;	// 1/4 second gap of data yields a mux break
	startOutputFrameNumber = -1;

        outputframebytes = (inputframebytes-VDIF_HEADER_BYTES)*config->getDNumMuxThreads(configindex, streamnum) + VDIF_HEADER_BYTES;

	nthreads = config->getDNumMuxThreads(configindex, streamnum);
	threads = config->getDMuxThreadMap(configindex, streamnum);

	fanout = config->genMk5FormatName(format, nrecordedbands, bw, nbits, sampling, outputframebytes, config->getDDecimationFactor(configindex, streamnum), config->getDNumMuxThreads(configindex, streamnum), formatname);
        if(fanout != 1)
        {
		cfatal << startl << "Fanout is " << fanout << ", which is impossible; no choice but to abort!" << endl;
#if HAVE_MARK5IPC
                unlockMark5();
#endif
                MPI_Abort(MPI_COMM_WORLD, 1);
        }

	cinfo << startl << "VDIFMark5DataStream::initialiseFile format=" << formatname << endl;

	mk5dirpath = getenv("MARK5_DIR_PATH");
	if(mk5dirpath == 0)
	{
		mk5dirpath = defaultDirPath;
	}

	startmjd = corrstartday + corrstartseconds/86400.0;

	sendMark5Status(MARK5_STATE_GETDIR, 0, startmjd, 0.0);

	if(module.nScans() == 0)
	{
		doUpdate = 1;
		cinfo << startl << "Getting module " << datafilenames[configindex][fileindex] << " directory info." << endl;
		rv = module.getCachedDirectory(xlrDevice, corrstartday, 
			datafilenames[configindex][fileindex].c_str(), 
			mk5dirpath, &dirCallback, &mk5status, 0, 0, 0, 1, -1, -1);

		if(rv < 0)
		{
                	if(module.error.str().size() > 0)
			{
				cerror << startl << module.error.str() << " (error code=" << rv << ")" << endl;
			}
			else
			{
				cerror << startl << "Directory for module " << datafilenames[configindex][fileindex] << " is not up to date.  Error code is " << rv << " .  You should have received a more detailed error message than this!" << endl;
			}
			sendMark5Status(MARK5_STATE_ERROR, 0, 0.0, 0.0);

			dataremaining = false;
			++nError;
			WATCHDOG( XLRClose(xlrDevice) );
#if HAVE_MARK5IPC
			unlockMark5();
#endif
			MPI_Abort(MPI_COMM_WORLD, 1);
		}

		rv = module.sanityCheck();
		if(rv < 0)
		{
			cerror << startl << "Module " << datafilenames[configindex][fileindex] << " contains undecoded scans" << endl;
			dataremaining = false;

			return;
		}

		if(module.mode == MARK5_READ_MODE_RT)
		{
			WATCHDOG( xlrRC = XLRSetFillData(xlrDevice, MARK5_FILL_PATTERN) );
			if(xlrRC == XLR_SUCCESS)
			{
				WATCHDOG( xlrRC = XLRSetOption(xlrDevice, SS_OPT_SKIPCHECKDIR) );
			}
			if(xlrRC != XLR_SUCCESS)
			{
				cerror << startl << "Cannot set Mark5 data replacement mode / fill pattern" << endl;
			}
			// NOTE: removed WATCHDOG( xlrRC = XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
			cwarn << startl << "Enabled realtime playback mode" << endl;
			readDelayMicroseconds = 10000;	// prime the read delay to speed up convergence to best value
		}
	}

	sendMark5Status(MARK5_STATE_GOTDIR, 0, startmjd, 0.0);

	// find starting position

	if(scanPointer && scanNum >= 0)  /* just continue by reading next valid scan */
	{
		cinfo << startl << "Advancing to next Mark5 module scan" << endl;
		do
		{
			++scanNum;
			if(scanNum >= module.nScans())
			{
				break;
			}
			scanPointer = &module.scans[scanNum];
		} while(scanPointer->duration < 0.1);
		if(scanNum >= module.nScans())
		{
			cwarn << startl << "No more data for this job on this module" << endl;
			scanPointer = 0;
			dataremaining = false;
			keepreading = false;
			noMoreData = true;
			sendMark5Status(MARK5_STATE_NOMOREDATA, 0, 0.0, 0.0);

			return;
		}
		cverbose << startl << "Before scan change: readscan = " << readscan << "  readsec = " << readseconds << "  readns = " << readnanoseconds << endl;
		readpointer = scanPointer->start + scanPointer->frameoffset;
		readseconds = (scanPointer->mjd-corrstartday)*86400 + scanPointer->sec - corrstartseconds + intclockseconds;
		readnanoseconds = scanPointer->nsStart();
		while(readscan < (model->getNumScans()-1) && model->getScanEndSec(readscan, corrstartday, corrstartseconds) < readseconds)
		{
			++readscan;
		}
		while(readscan > 0 && model->getScanStartSec(readscan, corrstartday, corrstartseconds) > readseconds)
		{
			--readscan;
		}
		readseconds = readseconds - model->getScanStartSec(readscan, corrstartday, corrstartseconds);

		cinfo << startl << "After scan change: readscan = " << readscan << " rs = " << readseconds << "  rns = " << readnanoseconds << endl;

		if(readscan == model->getNumScans() - 1 && readseconds >= model->getScanDuration(readscan))
		{
			cwarn << startl << "No more data for project on module [" << mpiid << "]" << endl;
			scanPointer = 0;
			scanNum = -1;
			dataremaining = false;
			keepreading = false;
			noMoreData = true;
			sendMark5Status(MARK5_STATE_NOMOREDATA, 0, 0.0, 0.0);

			return;
		}
	}
	else	/* first time this project */
	{
		n = 0;
		for(scanNum = 0; scanNum < module.nScans(); ++scanNum)
		{
			double scanstart, scanend;
			scanPointer = &module.scans[scanNum];
			scanstart = scanPointer->mjdStart();
			scanend = scanPointer->mjdEnd();

 			if(startmjd < scanstart)  /* obs starts before data */
			{
				cinfo << startl << "NM5 : scan found(1) : " << (scanNum+1) << endl;
				readpointer = scanPointer->start + scanPointer->frameoffset;
				readseconds = (scanPointer->mjd-corrstartday)*86400 + scanPointer->sec - corrstartseconds + intclockseconds;
				readnanoseconds = scanPointer->nsStart();
				while(readscan < (model->getNumScans()-1) && model->getScanEndSec(readscan, corrstartday, corrstartseconds) < readseconds)
				{
					++readscan;
				}
				while(readscan > 0 && model->getScanStartSec(readscan, corrstartday, corrstartseconds) > readseconds)
				{
					--readscan;
				}
				readseconds = readseconds - model->getScanStartSec(readscan, corrstartday, corrstartseconds);
				break;
			}
			else if(startmjd < scanend) /* obs starts within data */
			{
				int fbytes;

				cinfo << startl << "NM5 : scan found(2) : " << (scanNum+1) << endl;
				readpointer = scanPointer->start + scanPointer->frameoffset;
				n = static_cast<long long>((
					( ( (corrstartday - scanPointer->mjd)*86400 
					+ corrstartseconds - scanPointer->sec) - scanPointer->nsStart()*1.e-9)
					*scanPointer->framespersecond) + 0.5);
				fbytes = scanPointer->framebytes*scanPointer->tracks;
				readpointer += n*fbytes;
				readseconds = 0;
				readnanoseconds = 0;
				while(readscan < (model->getNumScans()-1) && model->getScanEndSec(readscan, corrstartday, corrstartseconds) < readseconds)
				{
					++readscan;
				}
				while(readscan > 0 && model->getScanStartSec(readscan, corrstartday, corrstartseconds) > readseconds)
				{
					--readscan;
				}
				readseconds = readseconds - model->getScanStartSec(readscan, corrstartday, corrstartseconds) + intclockseconds;
				break;
			}
		}
		cinfo << startl << "VDIFMark5DataStream " << mpiid << " positioned at byte " << readpointer << " scan = " << readscan << " seconds = " << readseconds << " ns = " << readnanoseconds << " n = " << n << endl;

		if(scanNum >= module.nScans() || scanPointer == 0)
		{
			cwarn << startl << "No valid data found.  Stopping playback!" << endl;

			scanNum = module.nScans()-1;
			scanPointer = &module.scans[scanNum];
			readpointer = scanPointer->start + scanPointer->length - (1<<21);
			if(readpointer < 0)
			{
				readpointer = 0;
			}

			readseconds = readseconds - model->getScanStartSec(readscan, corrstartday, corrstartseconds) + intclockseconds;
			readnanoseconds = 0;

			noDataOnModule = true;
		}

		cinfo << startl << "Scan info. start = " << scanPointer->start << " off = " << scanPointer->frameoffset << " size = " << scanPointer->framebytes << endl;
	}

        if(readpointer == -1)
        {
		cwarn << startl << "No data for this job on this module" << endl;
		scanPointer = 0;
		dataremaining = false;
		keepreading = false;
		noMoreData = true;
		sendMark5Status(MARK5_STATE_NOMOREDATA, 0, 0.0, 0.0);

		return;
        }

	sendMark5Status(MARK5_STATE_GOTDIR, readpointer, scanPointer->mjdStart(), 0.0);

	newscan = 1;

	cinfo << startl << "The frame start day is " << scanPointer->mjd << ", the frame start seconds is " << scanPointer->secStart()
		<< ", readscan is " << readscan << ", readseconds is " << readseconds << ", readnanoseconds is " << readnanoseconds << endl;

	/* update all the configs - to ensure that the nsincs and 
	 * headerbytes are correct
	 */
	if(doUpdate)
	{
		cinfo << startl << "Updating all configs [" << mpiid << "]" << endl;
		for(int i = 0; i < numdatasegments; ++i)
		{
			updateConfig(i);
		}
	}
	else
	{
		cinfo << startl << "NOT updating all configs [" << mpiid << "]" << endl;
	}

	// pointer to first byte after end of current scan
	readend = scanPointer->start + scanPointer->length;

	lockstart = lockend = lastslot = -1;

	// cause Mark5 reading thread to go ahead and start filling buffers
	pthread_barrier_wait(&mark5threadbarrier);
	cout << "initialiseFile barrier passed " << mpiid << endl;

	cinfo << startl << "Scan " << scanNum <<" initialised[" << mpiid << "]" << endl;
}

void VDIFMark5DataStream::openfile(int configindex, int fileindex)
{
	cinfo << startl << "VDIFMark5DataStream " << mpiid << " is about to look at a scan" << endl;

	/* fileindex should never increase for native mark5, but
	 * check just in case. 
	 */
	if(fileindex >= confignumfiles[configindex])
	{
		dataremaining = false;
		keepreading = false;
		cinfo << startl << "VDIFMark5DataStream " << mpiid << " is exiting because fileindex is " << fileindex << ", while confignumconfigfiles is " << confignumfiles[configindex] << endl;

		return;
	}

	dataremaining = true;

	initialiseFile(configindex, fileindex);
}


int VDIFMark5DataStream::moduleRead(int buffersegment)
{
	// Note: here readbytes is actually the length of the buffer segment

	unsigned long *destination = reinterpret_cast<unsigned long *>(&databuffer[buffersegment*(bufferbytes/numdatasegments)]);
	int n1, n2;
	int muxend, bytesvisible;

	if(lockstart < -1)
	{
		csevere << startl << "moduleRead " << mpiid << " lockstart = " << lockstart << endl;

	}

	if(lockstart == -1)
	{
		// first decoding of scan
		muxindex = readbufferslotsize;	// start at beginning of slot 1 (second slot)
		lockstart = lockend = 1;
		pthread_mutex_lock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = 'r';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
		if(mark5xlrfail)
		{
			cwarn << startl << "moduleRead " << mpiid << " detected mark5xlrfail.  [1]  Stopping." << endl;
			dataremaining = false;
			keepreading = false;
			pthread_mutex_unlock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockstart << endl;
			lockstart = lockend = -2;

			return 0;
		}
		cinfo << startl << "moduleRead " << mpiid << " has locked slot " << lockstart << endl;
	}

	n1 = muxindex / readbufferslotsize;
	if(lastslot >= 0 && muxindex + readbytes > endindex)
	{
		n2 = (endindex - 1) / readbufferslotsize;
	}
	else
	{
		n2 = (muxindex + readbytes - 1) / readbufferslotsize;
	}

	// note: it should be impossible for n2 >= readbufferslots because a previous memmove and slot shuffling should have prevented this.
	if(n2 >= readbufferslots)
	{
		csevere << startl << "moduleRead " << mpiid << " n2 >= readbufferslots" << endl;
	}

	if(n2 > n1 && lockend != n2)
	{
		lockend = n2;
		pthread_mutex_lock(mark5threadmutex + lockend);
		mutexstate[lockend] = 'r';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
		if(mark5xlrfail)
		{
			cwarn << startl << "moduleRead " << mpiid << " detected mark5xlrfail.  [2]  Stopping." << endl;
			dataremaining = false;
			keepreading = false;
			pthread_mutex_unlock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockstart << endl;
			pthread_mutex_unlock(mark5threadmutex + lockend);
		mutexstate[lockend] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockend << endl;
			lockstart = lockend = -2;

			return 0;
		}
		cinfo << startl << "moduleRead " << mpiid << " has locked slot " << lockend << endl;

	}
	
	if(lastslot == n2)
	{
		muxend = endindex;
	}
	else
	{
		muxend = (n2+1)*readbufferslotsize;
	}

	bytesvisible = muxend - muxindex;

	// multiplex and corner turn the data
	vdifmux(reinterpret_cast<unsigned char *>(destination), readbytes, readbuffer+muxindex, bytesvisible, inputframebytes, framespersecond, nbits, nthreads, threads, nSort, nGap, startOutputFrameNumber, &vstats);

	cinfo << startl << "vdifmux " << mpiid << " muxindex=" << muxindex << " bytesvisible=" << bytesvisible << " srcUsed=" << vstats.srcUsed << " destUsed=" << vstats.destUsed << " readbytes=" << readbytes << endl;

	bufferinfo[buffersegment].validbytes = vstats.destUsed;
	bufferinfo[buffersegment].readto = true;
	if(bufferinfo[buffersegment].validbytes > 0)
	{
		// In the case of VDIF, we can get the time from the data, so use that just in case there was a jump
		bufferinfo[buffersegment].scanns = (((vstats.startFrameNumber) % framespersecond) * 1000000000LL) / framespersecond;
		// FIXME: warning! here we are assuming no leap seconds since the epoch of the VDIF stream. FIXME
		// FIXME: below assumes each scan is < 86400 seconds long
		bufferinfo[buffersegment].scanseconds = (((vstats.startFrameNumber / framespersecond)) + intclockseconds - corrstartseconds - model->getScanStartSec(readscan, corrstartday, corrstartseconds)) % 86400;
	
		readnanoseconds = bufferinfo[buffersegment].scanns;
		readseconds = bufferinfo[buffersegment].scanseconds;
	}

	muxindex += vstats.srcUsed;

	if(lastslot == n2 && (muxindex+minleftoverdata > endindex || bytesvisible < readbytes / 4) )
	{
		// end of useful data for this scan
		dataremaining = false;
		pthread_mutex_unlock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
		cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockstart << endl;
		if(lockstart != lockend)
		{
			pthread_mutex_unlock(mark5threadmutex + lockend);
		mutexstate[lockend] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockend << endl;
		}
		lockstart = lockend = -2;
	}
	else
	{
		int n3;
		// note:  in all cases n2 = n1 or n1+1, n3 = n1 or n1+1 and n3 = n2 or n2+1
		// i.e., n3 >= n2 >= n1 and n3-n1 <= 1

		n3 = muxindex / readbufferslotsize;
		if(n3 > lockstart)
		{
			pthread_mutex_unlock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockstart << endl;
			lockstart = n3;
		}

		if(lockstart == readbufferslots - 1 && lastslot != readbufferslots - 1)
		{
			// Here it is time to move the data in the last slot to slot 0
			// Geometry: |  slot 0  |  slot 1  |  slot 2  |  slot 3  |  slot 4  |  slot 5  |
			// Before:   |          |dddddddddd|          |          |          |      dddd|  slot 5 locked
			// After:    |      dddd|dddddddddd|          |          |          |          |  slot 0 locked

			if(lockend != lockstart)
			{
				csevere << startl << "moduleRead " << mpiid << " memmove needed but lockend("<<lockend<<") != lockstart("<<lockstart<<")  lastslot="<<lastslot << endl;
			}
			lockstart = 0;
			pthread_mutex_lock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = 'r';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			if(mark5xlrfail)
			{
				cwarn << startl << "moduleRead " << mpiid << " detected mark5xlrfail.  [3]  Stopping." << endl;

				dataremaining = false;
				keepreading = false;
				pthread_mutex_unlock(mark5threadmutex + lockstart);
		mutexstate[lockstart] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
				cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockstart << endl;
				if(lockend != lockstart)
				{
					pthread_mutex_unlock(mark5threadmutex + lockend);
		mutexstate[lockend] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
					cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockend << endl;
				}
				lockstart = lockend = -2;

				return 0;
			}
			int newstart = muxindex % readbufferslotsize;
			memmove(readbuffer + newstart, readbuffer + muxindex, readbuffersize-muxindex);
		cinfo << startl << "Memmove: " << (readbuffersize-muxindex) << " bytes from " << muxindex << 
"/" << readbuffersize << " to " << newstart << endl;
			muxindex = newstart;

			pthread_mutex_unlock(mark5threadmutex + lockend);
		mutexstate[lockend] = '_';
		cinfo << startl << "Mutex state: " << mutexstate << endl;
			cinfo << startl << "moduleRead " << mpiid << " has unlocked slot " << lockend << endl;
			lockend = 0;
		}
	}

	return 0;
}

void VDIFMark5DataStream::diskToMemory(int buffersegment)
{
	unsigned long *buf;
	int obytes;
	double tv_us;
	static double now_us;
	static long long lastpos = 0;
	struct timeval tv;

	buf = reinterpret_cast<unsigned long *>(&databuffer[buffersegment*(bufferbytes/numdatasegments)]);

	waitForBuffer(buffersegment);

	// This function call abstracts away all the details.  The result is multiplexed data populating the 
	// desired buffer segment.
	moduleRead(buffersegment);

	obytes = bufferinfo[buffersegment].validbytes; // this is the number of bytes relevant for downstream processing

	// Update estimated read timing variables
	readnanoseconds += (bufferinfo[buffersegment].nsinc % 1000000000);
	readseconds += (bufferinfo[buffersegment].nsinc / 1000000000);
	readseconds += readnanoseconds/1000000000;
	readnanoseconds %= 1000000000;

	// did we just cross into next scan?
	if(readseconds >= model->getScanDuration(readscan))
	{
		if(readscan < (model->getNumScans()-1))
		{
			++readscan;
			readseconds += model->getScanStartSec(readscan-1, corrstartday, corrstartseconds);
			readseconds -= model->getScanStartSec(readscan, corrstartday, corrstartseconds);
		}
		else
		{
			// here we just crossed over the end of the job
			keepreading = false;
		}
	}

	if(switchedpower && obytes > 0)
	{
		static int nt = 0;

		++nt;

                // feed switched power detector
		if(nt % switchedpowerincrement == 0)
		{
			struct mark5_stream *m5stream = new_mark5_stream_absorb(
				new_mark5_stream_memory(buf, obytes),
				new_mark5_format_generic_from_string(formatname) );
			if(m5stream)
			{
				mark5_stream_fix_mjd(m5stream, config->getStartMJD());
				switchedpower->feed(m5stream);
				delete_mark5_stream(m5stream);
			}
                }
	}

	/* above here, this is basically the same as VDIFDataStream::diskToMemory() */


	/* below here, handle some Mark5 servoing and reporting */

	if(obytes <= 0)
	{
		++invalidtime;
	}
	else
	{
		invalidtime = 0;
	}

	gettimeofday(&tv, 0);
	tv_us = 1.0e6*tv.tv_sec + tv.tv_usec;

	if(tv_us - now_us > 1.5e6 && nReads > 4)
	{
		if(lastpos > 0)
		{
			double rate;
			double fmjd;
			enum Mk5State state;

			fmjd = corrstartday + (corrstartseconds + model->getScanStartSec(readscan, corrstartday, corrstartseconds) + readseconds + static_cast<double>(readnanoseconds)/1000000000.0)/86400.0;
			if(newscan > 0)
			{
				double fmjd2;

				newscan = 0;
				state = MARK5_STATE_START;
				rate = 0.0;
				lastrate = 0.0;
				nrate = 0;
				fmjd2 = scanPointer->mjd + (scanPointer->sec + static_cast<double>(scanPointer->framenuminsecond)/scanPointer->framespersecond)/86400.0;
				if(fmjd2 > fmjd)
				{
					fmjd = fmjd2;
				}
			}
			else if(invalidtime == 0)
			{
				const int HighRealTimeRate = 1440;
				const int LowRealTimeRate = 1300;

				state = MARK5_STATE_PLAY;
				rate = (static_cast<double>(readpointer) - static_cast<double>(lastpos))*8.0/(tv_us - now_us);
//				if(nrate > 1)
//				{
//					rate = (nrate*lastrate + 4*rate)/(nrate+4);
//				}

				// If in real-time mode, servo playback rate through adjustable inter-read delay
				if(module.mode == MARK5_READ_MODE_RT)
				{
					if(rate > HighRealTimeRate && lastrate > HighRealTimeRate && readDelayMicroseconds < 150000)
					{
						if(readDelayMicroseconds == 0)
						{
							readDelayMicroseconds = 10000;
						}
						else
						{
							readDelayMicroseconds = readDelayMicroseconds*3/2;
						}
						usleep(100000);
					}
					if(rate < LowRealTimeRate && lastrate < LowRealTimeRate)
					{
						readDelayMicroseconds = readDelayMicroseconds*9/10;	// reduce delay by 10%
					}
				}
				lastrate = rate;
				nrate++;
			}
			else
			{
				state = MARK5_STATE_PLAYINVALID;
				rate = invalidtime;
				nrate = 0;
			}

			sendMark5Status(state, readpointer, fmjd, rate);
		}
		lastpos = readpointer;
		now_us = tv_us;
	}
}


static bool legalVSN(const char *vsn)
{
	int nSep = 0;

	for(int i = 0; i < 8; ++i)
	{
		if(vsn[i] == '+' || vsn[i] == '-')
		{
			if(nSep > 0 || i == 0 || i == 7)
			{
				return false;
			}
			++nSep;
		}
		else if(isalpha(vsn[i]))
		{
			if(nSep != 0)
			{
				return false;
			}
		}
		else if(isdigit(vsn[i]))
		{
			if(nSep != 1)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	if(nSep != 1)
	{
		return false;
	}

	return true;
}

int VDIFMark5DataStream::sendMark5Status(enum Mk5State state, long long position, double dataMJD, float rate)
{
	int v = 0;
	S_BANKSTATUS A, B;
	XLR_RETURN_CODE xlrRC;

	// If there really is no more data, override a simple NODATA with a more precise response
	if(noMoreData == true && state == MARK5_STATE_NODATA)
	{
		state = MARK5_STATE_NOMOREDATA;
	}

	mk5status.state = state;
	mk5status.status = 0;
	mk5status.activeBank = ' ';
	mk5status.position = position;
	mk5status.rate = rate;
	mk5status.dataMJD = dataMJD;
	mk5status.scanNumber = scanNum+1;
	if(scanPointer && scanNum >= 0)
	{
      		snprintf(mk5status.scanName, DIFX_MESSAGE_MAX_SCANNAME_LEN, "%s", scanPointer->name.c_str());
	}
	else
	{
		strcpy(mk5status.scanName, "none");
	}
	if(state != MARK5_STATE_OPENING && state != MARK5_STATE_ERROR && state != MARK5_STATE_IDLE)
	{
		WATCHDOG( xlrRC = XLRGetBankStatus(xlrDevice, BANK_A, &A) );
		if(xlrRC == XLR_SUCCESS)
		{
			WATCHDOG( xlrRC = XLRGetBankStatus(xlrDevice, BANK_B, &B) );
		}
		if(xlrRC == XLR_SUCCESS)
		{
			strncpy(mk5status.vsnA, A.Label, 8);
			mk5status.vsnA[8] = 0;
			if(strncmp(mk5status.vsnA, "LABEL NO", 8) == 0)
			{
				strcpy(mk5status.vsnA, "none");
			}
			else if(!legalVSN(mk5status.vsnA))
			{
				strcpy(mk5status.vsnA, "badvsn");
			}
			strncpy(mk5status.vsnB, B.Label, 8);
			mk5status.vsnB[8] = 0;
			if(strncmp(mk5status.vsnB, "LABEL NO", 8) == 0)
			{
				strcpy(mk5status.vsnB, "none");
			}
			else if(!legalVSN(mk5status.vsnB))
			{
				strcpy(mk5status.vsnB, "badvsn");
			}
			if(A.Selected)
			{
				mk5status.activeBank = 'A';
				mk5status.status |= 0x100000;
			}
			if(A.State == STATE_READY)
			{
				mk5status.status |= 0x200000;
			}
			if(A.MediaStatus == MEDIASTATUS_FAULTED)
			{
				mk5status.status |= 0x400000;
			}
			if(A.WriteProtected)
			{
				mk5status.status |= 0x800000;
			}
			if(B.Selected)
			{
				mk5status.activeBank = 'B';
				mk5status.status |= 0x1000000;
			}
			if(B.State == STATE_READY)
			{
				mk5status.status |= 0x2000000;
			}
			if(B.MediaStatus == MEDIASTATUS_FAULTED)
			{
				mk5status.status |= 0x4000000;
			}
			if(B.WriteProtected)
			{
				mk5status.status |= 0x8000000;
			}
		}
		if(xlrRC != XLR_SUCCESS)
		{
			mk5status.state = MARK5_STATE_ERROR;
		}
	}
	else
	{
		sprintf(mk5status.vsnA, "???");
		sprintf(mk5status.vsnB, "???");
	}
	switch(mk5status.state)
	{
	case MARK5_STATE_PLAY:
		mk5status.status |= 0x0100;
		break;
	case MARK5_STATE_ERROR:
		mk5status.status |= 0x0002;
		break;
	case MARK5_STATE_IDLE:
		mk5status.status |= 0x0001;
		break;
	default:
		break;
	}

	v = difxMessageSendMark5Status(&mk5status);

	return v;
}
