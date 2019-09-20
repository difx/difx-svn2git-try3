/***************************************************************************
 *   Copyright (C) 2006-2019 by Adam Deller and Walter Brisken             *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id$
// $HeadURL: https://svn.atnf.csiro.au/difx/mpifxcorr/trunk/src/mk5.cpp $
// $LastChangedRevision$
// $Author$
// $LastChangedDate$
//
//============================================================================
#include <cmath>
#include <cstring>
#include <mpi.h>
#include <iomanip>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "config.h"
#include "alert.h"
#include "vdiffile.h"
#include "vdiffilereader_istream.h"
#include "mode.h"

#include <unistd.h>


/* TODO: 
   - make use of activesec and activescan
 */

/// VDIFDataStream -------------------------------------------------------

template <typename istreamT>
VDIFDataStream<istreamT>::VDIFDataStream(const Configuration * conf, int snum, int id, int ncores, int * cids, int bufferfactor, int numsegments)
 : DataStream<istreamT>(conf, snum, id, ncores, cids, bufferfactor, numsegments)
{
	//each data buffer segment contains an integer number of frames, because thats the way config determines max bytes
	lastconfig = -1;

	// switched power output assigned a name based on the datastream number (MPIID-1)
	int spf_Hz = conf->getDSwitchedPowerFrequency(id-1);
	if(spf_Hz > 0)
	{
		this->switchedpower = new SwitchedPower(conf, id);
		this->switchedpower->frequency = spf_Hz;
	}
	else
	{
		this->switchedpower = 0;
	}

	// Set some VDIF muxer parameters
	nSort = 32;	// allow data to be this many frames out of order without any loss at read boundaries
	nGap = 1000;	// a gap of this many frames will trigger an interruption of muxing
	startOutputFrameNumber = -1;
	memset(&vm, 0, sizeof(vm));

	// Make read buffer a bit bigger than a data segment size so extra bytes can be filtered out 
	// The excess should be limited to avoid large memory moves of extra data
	// But the amount of excess should be large enough to encompass all reasonable amounts of interloper data
	// Here we give 20% overhead plus 8 MB, just to be on the safe side...

	readbuffersize = (bufferfactor/numsegments)*conf->getMaxDataBytes(this->streamnum)*5LL/4LL+8000000LL;
	readbuffersize -= (readbuffersize % 8); // make it a multiple of 8 bytes
	readbufferleftover = 0;
	this->readbuffer = 0;	// to be allocated via initialize();

	this->estimatedbytes += readbuffersize;

	// Don't bother to do another read iteration just to salvage this many bytes at the end of a file/scan
	minleftoverdata = 20000;

	resetvdifmuxstatistics(&vstats);
	nthreads = 0; // no threads identified yet
	threads = 0;  // null pointer indicating not yet initialized
	invalidtime = 0;

	samplingtype = Configuration::REAL;
	filecheck = Configuration::getFileCheckLevel();
	if(filecheck == Configuration::FILECHECKUNKNOWN)
	{
		cwarn << startl << "env var DIFX_FILE_CHECK_LEVEL was set to " << getenv("DIFX_FILE_CHECK_LEVEL") << " which is not a legal value.  Assuming NONE." << endl;
		filecheck = Configuration::FILECHECKNONE;
	}

	this->setHints();
}

template <typename istreamT>
VDIFDataStream<istreamT>::~VDIFDataStream()
{
	cinfo << startl << "VDIF multiplexing statistics: nValidFrame=" << vstats.nValidFrame << " nInvalidFrame=" << vstats.nInvalidFrame << " nDiscardedFrame=" << vstats.nDiscardedFrame << " nWrongThread=" << vstats.nWrongThread << " nSkippedByte=" << vstats.nSkippedByte << " nFillByte=" << vstats.nFillByte << " nDuplicateFrame=" << vstats.nDuplicateFrame << " bytesProcessed=" << vstats.bytesProcessed << " nGoodFrame=" << vstats.nGoodFrame << " nCall=" << vstats.nCall << endl;
	if(vstats.nWrongThread > 0)
	{
		cerror << startl << "One or more wrong-threads were identified.  This may indicate a correlator configuration error." << endl;
	}
	if(vstats.nDuplicateFrame > 0)
	{
		cerror << startl << "One or more duplicate data frames (same time and thread) were found.  This may indicate serious problems with the digital back end configuration." << endl;
	}
	if(vstats.nFillByte > 3*vstats.bytesProcessed/4)
	{
		cerror << startl << "More than three quarters of the data from this station was unrecoverable (fill pattern)." << endl;
	}
	if(vstats.nFillByte > vstats.bytesProcessed/2)
	{
		cwarn << startl << "More than half of the data from this station was unrecoverable (fill pattern)." << endl;
	}
	if(vstats.nSkippedByte > vstats.bytesProcessed/20)
	{
		cwarn << startl << "More than 5 percent of data from this antenna were unwanted packets.  This could indicate a problem in the routing of data from the digital back end to the recorder." << endl;
	}

	//printvdifmuxstatistics(&vstats);
	if(this->switchedpower)
	{
		delete this->switchedpower;
	}
	if(this->readbuffer)
	{
		delete [] this->readbuffer;
	}
}

template <typename istreamT>
void VDIFDataStream<istreamT>::setHints()
{
}

template <>
void VDIFDataStream<VDIFFileReaderIStream>::setHints()
{
	int fps = this->config->getFramesPerSecond(0, this->streamnum) / this->config->getDNumMuxThreads(0, this->streamnum);
	this->input.setHintFramespersec(fps);
}

template <typename istreamT>
void VDIFDataStream<istreamT>::initialise()
{
	this->readbuffer = new unsigned char[readbuffersize];
	DataStream<istreamT>::initialise();
}

template <typename istreamT>
int VDIFDataStream<istreamT>::calculateControlParams(int scan, int offsetsec, int offsetns)
{
	int bufferindex, framesin, vlbaoffset, looksegment, payloadbytes, framespersecond, framebytes;
	float datarate;

	bufferindex = DataStream<istreamT>::calculateControlParams(scan, offsetsec, offsetns);

	if(this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] == Mode::INVALID_SUBINT)
	{
		return 0;
	}

	looksegment = this->atsegment;
	if(this->bufferinfo[this->atsegment].configindex < 0) //will get garbage using this to set framebytes etc
	{
		//look at the following segment - normally has sensible info
		looksegment = (this->atsegment+1)%this->numdatasegments;
		if(this->bufferinfo[this->atsegment].nsinc != this->bufferinfo[looksegment].nsinc)
		{
			cwarn << startl << "Incorrectly set config index at scan boundary! Flagging this subint" << endl;
			this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = Mode::INVALID_SUBINT;
	
			return bufferindex;
		}
	}
	if(this->bufferinfo[looksegment].configindex < 0)
	{
		//Sometimes the next segment is still showing invalid due to the geometric delay.
		//try the following segment - if thats no good, get out
		//this is not entirely safe since the read thread may not have set the configindex yet, but at worst
		//one subint will be affected
		looksegment = (looksegment+1)%this->numdatasegments;
		if(this->bufferinfo[looksegment].configindex < 0)
		{
			cwarn << startl << "Cannot find a valid configindex to set Mk5-related info.  Flagging this subint" << endl;
			this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = Mode::INVALID_SUBINT;

			return bufferindex;
		}
		if(this->bufferinfo[this->atsegment].nsinc != this->bufferinfo[looksegment].nsinc)
		{
			cwarn << startl << "Incorrectly set config index at scan boundary! Flagging this subint" << endl;
			this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = Mode::INVALID_SUBINT;

			return bufferindex;
		}
	}

	//if we got here, we found a configindex we are happy with.  Find out the mk5 details
	payloadbytes = this->config->getFramePayloadBytes(this->bufferinfo[looksegment].configindex, this->streamnum);
	framebytes = this->config->getFrameBytes(this->bufferinfo[looksegment].configindex, this->streamnum);
	framespersecond = this->config->getFramesPerSecond(this->bufferinfo[looksegment].configindex, this->streamnum);
	payloadbytes *= this->config->getDNumMuxThreads(this->bufferinfo[looksegment].configindex, this->streamnum);
	framebytes = (framebytes-VDIF_HEADER_BYTES)*this->config->getDNumMuxThreads(this->bufferinfo[looksegment].configindex, this->streamnum) + VDIF_HEADER_BYTES;
	framespersecond /= this->config->getDNumMuxThreads(this->bufferinfo[looksegment].configindex, this->streamnum);

	samplingtype = this->config->getDSampling(this->bufferinfo[looksegment].configindex, this->streamnum);

	//set the fraction of data to use to determine system temperature based on data rate
	//the values set here work well for the today's computers and clusters...
	datarate = static_cast<float>(framebytes)*static_cast<float>(framespersecond)*8.0/1.0e6;  // in Mbps
	if(datarate < 512)
	{
		this->switchedpowerincrement = 1;
	}
	else
	{
		this->switchedpowerincrement = static_cast<int>(datarate/512 + 0.1);
	}

	//do the necessary correction to start from a frame boundary; work out the offset from the start of this segment
	vlbaoffset = bufferindex - this->atsegment*this->readbytes;

	if(vlbaoffset < 0)
	{
		cfatal << startl << "VDIFDataStream::calculateControlParams: vlbaoffset<0: vlbaoffset=" << vlbaoffset << " bufferindex=" << bufferindex << " this->atsegment=" << this->atsegment << " readbytes=" << this->readbytes << ", framebytes=" << framebytes << ", payloadbytes=" << payloadbytes << endl;
		this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = Mode::INVALID_SUBINT;
		// WFB20120123 MPI_Abort(MPI_COMM_WORLD, 1);
		
		return 0;
	}

	// bufferindex was previously computed assuming no framing overhead
	framesin = vlbaoffset/payloadbytes;

	// here we enforce frame granularity.  We simply back up to the previous frame that is a multiple of the frame granularity.
	if(framesin % vm.frameGranularity != 0)
	{
		framesin -= (framesin % vm.frameGranularity);
	}

	// Note here a time is needed, so we only count payloadbytes
	long long segoffns = this->bufferinfo[this->atsegment].scanns + (long long)((1000000000.0*framesin)/framespersecond);
	this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = this->bufferinfo[this->atsegment].scanseconds + ((int)(segoffns/1000000000));
	this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][2] = ((int)(segoffns%1000000000));

	//go back to nearest frame -- here the total number of bytes matters
	bufferindex = this->atsegment*this->readbytes + framesin*framebytes;

	//if we are right at the end of the last segment, and there is a jump after this segment, bail out
	if(bufferindex == this->bufferbytes)
	{
		if(this->bufferinfo[this->atsegment].scan != this->bufferinfo[(this->atsegment+1)%this->numdatasegments].scan ||
		   ((this->bufferinfo[(this->atsegment+1)%this->numdatasegments].scanseconds - this->bufferinfo[this->atsegment].scanseconds)*1000000000 +
		   this->bufferinfo[(this->atsegment+1)%this->numdatasegments].scanns - this->bufferinfo[this->atsegment].scanns - this->bufferinfo[this->atsegment].nsinc != 0))
		{
			this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = Mode::INVALID_SUBINT;
			
			return 0; //note exit here!!
		}
		else
		{
			cwarn << startl << "Developer error mk5: bufferindex == bufferbytes in a 'normal' situation" << endl;
		}
	}

	if(bufferindex > this->bufferbytes) /* WFB: this was >= */
	{
		cfatal << startl << "VDIFDataStream::calculateControlParams: bufferindex>=bufferbytes: bufferindex=" << bufferindex << " >= bufferbytes=" << this->bufferbytes << " atsegment = " << this->atsegment << endl;
		this->bufferinfo[this->atsegment].controlbuffer[this->bufferinfo[this->atsegment].numsent][1] = Mode::INVALID_SUBINT;
		MPI_Abort(MPI_COMM_WORLD, 1);

		return 0;
	}

	return bufferindex;
}

template <typename istreamT>
void VDIFDataStream<istreamT>::updateConfig(int segmentindex)
{
	//run the default update config, then add additional information specific to Mk5
	DataStream<istreamT>::updateConfig(segmentindex);
	if(this->bufferinfo[segmentindex].configindex < 0) //If the config < 0 we can skip this scan
	{
		return;
	}

	int oframebytes = (this->config->getFrameBytes(this->bufferinfo[segmentindex].configindex, this->streamnum) - VDIF_HEADER_BYTES)*this->config->getDNumMuxThreads(this->bufferinfo[segmentindex].configindex, this->streamnum) + VDIF_HEADER_BYTES;
	int oframespersecond = this->config->getFramesPerSecond(this->bufferinfo[segmentindex].configindex, this->streamnum) / this->config->getDNumMuxThreads(this->bufferinfo[segmentindex].configindex, this->streamnum);

	//correct the nsinc - should be number of output frames*frame time
	this->bufferinfo[segmentindex].nsinc = int(((this->bufferbytes/this->numdatasegments)/oframebytes)*(1000000000.0/double(oframespersecond)) + 0.5);

	//take care of the case where an integral number of frames is not an integral number of blockspersend - ensure sendbytes is long enough
	//note below, the math should produce a pure integer, but add 0.5 to make sure that the fuzziness of floats doesn't cause an off-by-one error
	this->bufferinfo[segmentindex].sendbytes = int(((((double)this->bufferinfo[segmentindex].sendbytes)* ((double)this->config->getSubintNS(this->bufferinfo[segmentindex].configindex)))/(this->config->getSubintNS(this->bufferinfo[segmentindex].configindex) + this->config->getGuardNS(this->bufferinfo[segmentindex].configindex)) + 0.5));
}

template <typename istreamT>
void VDIFDataStream<istreamT>::initialiseFile(int configindex, int fileindex)
{
	const int MaxSummaryLength = 256;
	int nrecordedbands, fanout;
	Configuration::datasampling sampling;
	Configuration::dataformat format;
	double bw;
	int muxFlags;
	int rv;

	long long dataoffset = 0;
	char fileSummaryString[MaxSummaryLength];
	int jumpseconds, currentdsseconds;

	format = this->config->getDataFormat(configindex, this->streamnum);
	sampling = this->config->getDSampling(configindex, this->streamnum);
	nbits = this->config->getDNumBits(configindex, this->streamnum);	/* Bits per sample.  If complex, bits per component. */
	nrecordedbands = this->config->getDNumRecordedBands(configindex, this->streamnum);
	inputframebytes = this->config->getFrameBytes(configindex, this->streamnum);
	framespersecond = this->config->getFramesPerSecond(configindex, this->streamnum)/this->config->getDNumMuxThreads(configindex, this->streamnum);
	bw = this->config->getDRecordedBandwidth(configindex, this->streamnum, 0);

	nGap = framespersecond/4;	// 1/4 second gap of data yields a mux break
	startOutputFrameNumber = -1;
	minleftoverdata = 4*inputframebytes;	// waste up to 4 input frames at end of read 

	nthreads = this->config->getDNumMuxThreads(configindex, this->streamnum);
	threads = this->config->getDMuxThreadMap(configindex, this->streamnum);

	muxFlags = VDIF_MUX_FLAG_RESPECTGRANULARITY | VDIF_MUX_FLAG_PROPAGATEVALIDITY;
	if(sampling == Configuration::COMPLEX)
	{
		muxFlags |= VDIF_MUX_FLAG_COMPLEX;
	}

	rv = configurevdifmux(&vm, inputframebytes, framespersecond, nbits, nthreads, threads, nSort, nGap, muxFlags);
	if(rv < 0)
	{
		cfatal << startl << "configurevmux failed with return code " << rv << endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
	}


	if(nrecordedbands > nthreads)
	{
		int nBandPerThread = nrecordedbands/nthreads;

		cinfo << startl << "Note: " << nBandPerThread << " recoded channels (bands) reside on each thread.  Support for this is new.  Congratulations for being bold and trying this out!  Warranty void in the 193 UN recognized nations." << endl;
		
		if(nBandPerThread * nthreads != nrecordedbands)
		{
			cerror << startl << "Error: " << nrecordedbands << " recorded channels (bands) were recorded but they are divided unequally across " << nthreads << " threads.  This is not allowed.  Things will probably get very bad soon..." << endl;
		}
		setvdifmuxinputchannels(&vm, nBandPerThread);
		vdiffilesummarysetsamplerate(&fileSummary, static_cast<int64_t>(bw*2000000LL*nBandPerThread));
	}
	else if(nrecordedbands < nthreads)
	{
		/* must be a fanout mode (DBBC3 probably) */
		int nThreadPerBand = nthreads/nrecordedbands;

		cinfo << startl << "Note: " << nThreadPerBand << " threads are used to store each channel (band; e.g., this is a VDIF fanout mode).  Support for this is new.  Congratulations for experimenting with plausible code.  Warranty void on weekdays and select weekends." << endl;

		if(nThreadPerBand * nrecordedbands != nthreads)
		{
			cerror << startl << "Error: " << nthreads << " threads were recorded but they are divided unequally across " << nrecordedbands << " record channels (bands).  This is not allowed.  Things are about to go from bad to worse.  Hold onto your HAT..." << endl;
		}
		setvdifmuxfanoutfactor(&vm, nThreadPerBand);
		vdiffilesummarysetsamplerate(&fileSummary, static_cast<int64_t>(bw*2000000LL/nThreadPerBand));
	}

	/* Note: the following fanout concept is an explicit one and is not relevant to VDIF in any way */
	fanout = this->config->genMk5FormatName(format, nrecordedbands, bw, nbits, sampling, vm.outputFrameSize, this->config->getDDecimationFactor(configindex, this->streamnum), this->config->getDNumMuxThreads(configindex, this->streamnum), formatname);
	if(fanout != 1)
	{
		cfatal << startl << "Classic fanout is " << fanout << ", which is impossible; no choice but to abort!" << endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	cinfo << startl << "VDIFDataStream::initialiseFile format=" << formatname << endl;

	// Here we need to open the file, read the start time, jump if necessary, and if past end of file, dataremaining = false.  Then set readseconds...

	if(filecheck == Configuration::FILECHECKSEEK)
	{
		// First we get a description of the contents of the purported VDIF file and exit if it looks like not VDIF at all
		rv = summarizevdiffile(&fileSummary, this->datafilenames[configindex][fileindex].c_str(), inputframebytes);
		if(rv < 0)
		{
			cwarn << startl << "VDIFDataStream::initialiseFile: summary of file " << this->datafilenames[configindex][fileindex] << " resulted in error code " << rv << ".  This does not look like valid VDIF data." << endl;
			this->dataremaining = false;

			return;
		}

		// Put file information into log stream
		vdiffilesummarysetsamplerate(&fileSummary, static_cast<int64_t>(bw*2000000LL*nrecordedbands/nthreads));
		snprintvdiffilesummary(fileSummaryString, MaxSummaryLength, &fileSummary);
		cinfo << startl << fileSummaryString << endl;

		// If verbose...
		printvdiffilesummary(&fileSummary);

		// Here set readseconds to time since beginning of job
		this->readseconds = 86400*(vdiffilesummarygetstartmjd(&fileSummary)-this->corrstartday) + vdiffilesummarygetstartsecond(&fileSummary)-this->corrstartseconds + this->intclockseconds;
		this->readnanoseconds = vdiffilesummarygetstartns(&fileSummary);
		currentdsseconds = this->activesec + this->model->getScanStartSec(this->activescan, this->config->getStartMJD(), this->config->getStartSeconds());

		if(currentdsseconds > this->readseconds+1)
		{
			jumpseconds = currentdsseconds - this->readseconds;
			if(this->activens < this->readnanoseconds)
			{
				jumpseconds--;
			}

			// set byte offset to the requested time

			int n, d;	// numerator and demoninator of frame/payload size ratio
			n = fileSummary.frameSize;
			d = fileSummary.frameSize - 32;

			dataoffset = static_cast<long long>(jumpseconds*vdiffilesummarygetbytespersecond(&fileSummary)/d*n + 0.5);

			this->readseconds += jumpseconds;
		}

		// Now set readseconds to time since beginning of scan
		this->readseconds = this->readseconds - this->model->getScanStartSec(this->readscan, this->corrstartday, this->corrstartseconds);
		
		// Advance into file if requested
		if(fileSummary.firstFrameOffset + dataoffset > 0)
		{
			cverbose << startl << "About to seek to byte " << fileSummary.firstFrameOffset << " plus jump " << dataoffset << " to get to the first wanted frame" << endl;

			this->input.seekg(fileSummary.firstFrameOffset + dataoffset, ios_base::beg);
			if(this->input.peek() == EOF)
			{
				cinfo << startl << "File " << this->datafilenames[configindex][fileindex] << " ended before the currently desired time" << endl;
				this->dataremaining = false;
				this->input.clear();
			}
		}
	}
	else
	{
		cverbose << startl << "Not doing peek/seek on file due to setting of DIFX_FILE_CHECK_LEVEL env var." << endl;
	}
}

template <typename istreamT>
int VDIFDataStream<istreamT>::testForSync(int configindex, int buffersegment)
{
	// not needed.  vdifmux always leaves perfectly synchonized data behind
	return 0;
}


// This function does the actual file IO, readbuffer management, and VDIF multiplexing.  The result after each
// call is, hopefully, readbytes of multiplexed data being put into buffer segment with potentially some 
// read data left over in the read buffer ready for next time
template <typename istreamT>
int VDIFDataStream<istreamT>::dataRead(int buffersegment)
{
	unsigned char *destination;
	int bytes;
	int muxReturn;
	unsigned int bytesvisible;

	destination = reinterpret_cast<unsigned char *>(&this->databuffer[buffersegment*(this->bufferbytes/this->numdatasegments)]);

	if(this->input.eof())
	{
		bytes = 0;
	}
	else
	{
		// Bytes to read
		bytes = readbuffersize - readbufferleftover;
	}

	// if the file is exhausted, just multiplex any leftover data and return
	if(bytes > 0)
	{
		// execute the file read
		this->input.clear();

		this->input.read(reinterpret_cast<char *>(this->readbuffer) + readbufferleftover, bytes);
		bytes = this->input.gcount();

		bytesvisible = readbufferleftover + bytes;
	}
	else
	{
		bytesvisible = readbufferleftover;
	}

	if(bytesvisible <= 0)
	{
		this->dataremaining = false;
		this->bufferinfo[buffersegment].validbytes = 0;
		readbufferleftover = 0;

		cinfo << startl << "bytesvisible == 0.  Assuming end of file." << endl;

		return 0;
	}

	// multiplex and corner turn the data
	muxReturn = vdifmux(destination, this->readbytes, this->readbuffer, bytesvisible, &vm, startOutputFrameNumber, &vstats);

	if(muxReturn <= 0)
	{
		this->dataremaining = false;
		this->bufferinfo[buffersegment].validbytes = 0;
		readbufferleftover = 0;

		if(muxReturn < 0)
		{
			cerror << startl << "vdifmux() failed with return code " << muxReturn << ", likely input buffer is too small!" << endl;
		}
		else
		{
			cinfo << startl << "vdifmux returned no data.  Assuming end of file." << endl;
		}

		return 0;
	}

	this->consumedbytes += bytes;
	this->bufferinfo[buffersegment].validbytes = vstats.destUsed;
	this->bufferinfo[buffersegment].readto = true;
	if(this->bufferinfo[buffersegment].validbytes > 0)
	{
		// In the case of VDIF, we can get the time from the data, so use that just in case there was a jump
		this->bufferinfo[buffersegment].scanns = (((vstats.startFrameNumber) % framespersecond) * 1000000000LL) / framespersecond;
		// FIXME: warning! here we are assuming no leap seconds since the epoch of the VDIF stream. FIXME
		// FIXME: below assumes each scan is < 86400 seconds long
		this->bufferinfo[buffersegment].scanseconds = ((vstats.startFrameNumber / framespersecond) % 86400) + this->intclockseconds - this->corrstartseconds - this->model->getScanStartSec(this->readscan, this->corrstartday, this->corrstartseconds);
		if(this->bufferinfo[buffersegment].scanseconds > 86400/2)
		{
			this->bufferinfo[buffersegment].scanseconds -= 86400;
		}
		else if(this->bufferinfo[buffersegment].scanseconds < -86400/2)
		{
			this->bufferinfo[buffersegment].scanseconds += 86400;
		}

		this->readnanoseconds = this->bufferinfo[buffersegment].scanns;
		this->readseconds = this->bufferinfo[buffersegment].scanseconds;

		// look at difference in data frames consumed and produced and proceed accordingly
		int deltaDataFrames = vstats.srcUsed/(nthreads*inputframebytes) - vstats.destUsed/(nthreads*(inputframebytes-VDIF_HEADER_BYTES) + VDIF_HEADER_BYTES);

		if(deltaDataFrames == 0)
		{
			// We should be able to preset startOutputFrameNumber.  Warning: early use of this was frought with peril but things seem OK now.
			startOutputFrameNumber = vstats.startFrameNumber + vstats.nOutputFrame;
		}
		else
		{
			if(deltaDataFrames < -10)
			{
				static int nGapWarn = 0;

				++nGapWarn;
				if( (nGapWarn & (nGapWarn - 1)) == 0)
				{
					cwarn << startl << "Data gap of " << (vstats.destUsed-vstats.srcUsed) << " bytes out of " << vstats.destUsed << " bytes found. startOutputFrameNumber=" << startOutputFrameNumber << " bytesvisible=" << bytesvisible << " N=" << nGapWarn << endl;
				}
			}
			else if(deltaDataFrames > 10)
			{
				static int nExcessWarn = 0;

				++nExcessWarn;
				if( (nExcessWarn & (nExcessWarn - 1)) == 0)
				{
					cwarn << startl << "Data excess of " << (vstats.srcUsed-vstats.destUsed) << " bytes out of " << vstats.destUsed << " bytes found. startOutputFrameNumber=" << startOutputFrameNumber << " bytesvisible=" << bytesvisible << " N=" << nExcessWarn << endl;
				}
			}
			startOutputFrameNumber = -1;
		}
	}
	else
	{
		startOutputFrameNumber = -1;
	}

	readbufferleftover += (bytes - vstats.srcUsed);

	if(readbufferleftover > 0)
	{
		memmove(this->readbuffer, readbuffer+vstats.srcUsed, readbufferleftover);
	}
	else if(readbufferleftover < 0)
	{
		cwarn << startl << "readbufferleftover = " << readbufferleftover << "; it should never be negative." << endl;

		readbufferleftover = 0;
	}
	if(readbufferleftover <= minleftoverdata && this->input.eof())
	{
		if(readbufferleftover > 0)
		{
			cwarn << startl << "Stopping decoding with " << readbufferleftover << " bytes remaining to be decoded.  minleftoverdata = " << minleftoverdata << endl;
		}
		readbufferleftover = 0;

		// here we've in one call both read all the remaining data from a file and multiplexed it all without leftovers
		this->dataremaining = false;
	}

	return bytes;
}

template <typename istreamT>
void VDIFDataStream<istreamT>::diskToMemory(int buffersegment)
{
	u32 *buf;

	buf = reinterpret_cast<u32 *>(&this->databuffer[buffersegment*(this->bufferbytes/this->numdatasegments)]);

	//do the buffer housekeeping
	this->waitForBuffer(buffersegment);

	// This function call abstracts away all the details.  The result is multiplexed data populating the 
	// desired buffer segment.
	dataRead(buffersegment);

	// Update estimated read timing variables
	this->readnanoseconds += (this->bufferinfo[buffersegment].nsinc % 1000000000);
	this->readseconds += (this->bufferinfo[buffersegment].nsinc / 1000000000);
	this->readseconds += this->readnanoseconds/1000000000;
	this->readnanoseconds %= 1000000000;

	if(vstats.destUsed == 0)
	{
		++invalidtime;
	}
	else
	{
		invalidtime = 0;
	}

	// did we just come to the end of job execution time?
	if(this->readseconds + this->model->getScanStartSec(this->readscan, this->corrstartday, this->corrstartseconds) >= this->config->getExecuteSeconds())
	{
		this->keepreading = false;
		this->dataremaining = false;
		cinfo << startl << "diskToMemory: end of executeseconds reached.  stopping." << endl;
	}

	// did we just cross into next scan?
	if(this->readseconds >= this->model->getScanDuration(this->readscan))
	{
		cinfo << startl << "diskToMemory: end of schedule scan " << this->readscan << " of " << this->model->getNumScans() << " detected" << endl;

		// find next valid schedule scan
		do
		{
			++this->readscan;
		} while(this->readscan < this->model->getNumScans() && this->config->getScanConfigIndex(this->readscan));

		cinfo << startl << "readscan incremented to " << this->readscan << endl;

		if(this->readscan < this->model->getNumScans())
		{
			//if we need to, change the config
			if(this->config->getScanConfigIndex(this->readscan) != this->bufferinfo[(this->lastvalidsegment + 1)%this->numdatasegments].configindex)
			{
				updateConfig((this->lastvalidsegment + 1)%this->numdatasegments);
			}
		}
		else
		{
			// here we just crossed over the end of the job
			cverbose << startl << "readscan==getNumScans -> keepreading=false" << endl;
			
			this->keepreading = false;

			this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].scan = this->model->getNumScans()-1;
			this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].scanseconds = this->model->getScanDuration(this->model->getNumScans()-1);
			this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].scanns = 0;
		}
		cinfo << startl << "diskToMemory: starting schedule scan " << this->readscan << endl;
	}

	if(this->switchedpower && this->bufferinfo[buffersegment].validbytes > 0)
	{
		static int nt = 0;

		++nt;

                // feed switched power detector
		if(nt % this->switchedpowerincrement == 0)
		{
			struct mark5_stream *m5stream = new_mark5_stream_absorb(
				new_mark5_stream_memory(buf, this->bufferinfo[buffersegment].validbytes),
				new_mark5_format_generic_from_string(formatname) );
			if(m5stream)
			{
				mark5_stream_fix_mjd(m5stream, this->config->getStartMJD());
				this->switchedpower->feed(m5stream);
				delete_mark5_stream(m5stream);
			}
                }
	}
}

template <typename istreamT>
void VDIFDataStream<istreamT>::loopfileread()
{
	int perr;
	int numread = 0;

	//lock the outstanding send lock
	perr = pthread_mutex_lock(&this->outstandingsendlock);
	if(perr != 0)
	{
		csevere << startl << "Error in initial readthread lock of outstandingsendlock!" << endl;
	}

	//lock the first section to start reading
	this->dataremaining = false;
	this->keepreading = true;
	while(!this->dataremaining && this->keepreading)
	{
		this->openfile(this->bufferinfo[0].configindex, this->filesread[this->bufferinfo[0].configindex]++);
		if(!this->dataremaining)
		{
			this->input.close();
		}
	}

	if(this->keepreading)
	{
		diskToMemory(numread++);
		diskToMemory(numread++);
		this->lastvalidsegment = numread;
		perr = pthread_mutex_lock(&(this->bufferlock[numread]));
		if(perr != 0)
		{
			csevere << startl << "Error in initial readthread lock of first buffer section!" << endl;
		}
	}
	else
	{
		csevere << startl << "Couldn't find any valid data; will be shutting down gracefully!" << endl;
	}
	this->readthreadstarted = true;
	perr = pthread_cond_signal(&this->initcond);
	if(perr != 0)
	{
		csevere << startl << "Datastream readthread error trying to signal main thread to wake up!" << endl;
	}
	if(this->keepreading)
	{
		diskToMemory(numread++);
	}

	while(this->keepreading && (this->bufferinfo[this->lastvalidsegment].configindex < 0 || this->filesread[this->bufferinfo[this->lastvalidsegment].configindex] <= this->confignumfiles[this->bufferinfo[this->lastvalidsegment].configindex]))
	{
		while(this->dataremaining && this->keepreading)
		{
			this->lastvalidsegment = (this->lastvalidsegment + 1)%this->numdatasegments;

			//lock the next section
			perr = pthread_mutex_lock(&(this->bufferlock[this->lastvalidsegment]));
			if(perr != 0)
			{
				csevere << startl << "Error in readthread lock of buffer section!" << this->lastvalidsegment << endl;
			}

			if(!this->isnewfile) //can unlock previous section immediately
			{
				//unlock the previous section
				perr = pthread_mutex_unlock(&(this->bufferlock[(this->lastvalidsegment-1+this->numdatasegments)% this->numdatasegments]));    
				if(perr != 0)
				{
					csevere << startl << "Error (" << perr << ") in readthread unlock of buffer section!" << (this->lastvalidsegment-1+this->numdatasegments)%this->numdatasegments << endl;
				}
			}

			//do the read
			diskToMemory(this->lastvalidsegment);
			numread++;

			if(this->isnewfile) //had to wait before unlocking file
			{
				//unlock the previous section
				perr = pthread_mutex_unlock(&(this->bufferlock[(this->lastvalidsegment-1+this->numdatasegments)% this->numdatasegments]));
				if(perr != 0)
				{
					csevere << startl << "Error (" << perr << ") in readthread unlock of buffer section!" << (this->lastvalidsegment-1+this->numdatasegments)%this->numdatasegments << endl;
				}
			}
			this->isnewfile = false;
		}
		if(this->keepreading)
		{
cverbose << startl << "keepreading is true" << endl;
			this->input.close();

			//if we need to, change the config
			int nextconfigindex = this->config->getScanConfigIndex(this->readscan);
			while(nextconfigindex < 0 && this->readscan < this->model->getNumScans())
			{
				this->readseconds = 0; 
				nextconfigindex = this->config->getScanConfigIndex(++this->readscan);
			}
			if(this->readscan == this->model->getNumScans())
			{
				this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].scan = this->model->getNumScans()-1;
				this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].scanseconds = this->model->getScanDuration(this->model->getNumScans()-1);
				this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].scanns = 0;
				this->keepreading = false;
			}
			else
			{
				if(this->config->getScanConfigIndex(this->readscan) != this->bufferinfo[(this->lastvalidsegment + 1)%this->numdatasegments].configindex)
				{
					updateConfig((this->lastvalidsegment + 1)%this->numdatasegments);
				}
				//if the datastreams for two or more configs are common, they'll all have the same 
				//files.  Therefore work with the lowest one
				int lowestconfigindex = this->bufferinfo[(this->lastvalidsegment+1)%this->numdatasegments].configindex;
				for(int i=this->config->getNumConfigs()-1;i>=0;i--)
				{
					if(this->config->getDDataFileNames(i, this->streamnum) == this->config->getDDataFileNames(lowestconfigindex, this->streamnum))
					lowestconfigindex = i;
				}
				this->openfile(lowestconfigindex, this->filesread[lowestconfigindex]++);
				bool skipsomefiles = (this->config->getScanConfigIndex(this->readscan) < 0)?true:false;
				while(skipsomefiles)
				{
					int nextscan = this->peekfile(lowestconfigindex, this->filesread[lowestconfigindex]);
					if(nextscan == this->readscan || (nextscan == this->readscan+1 && this->config->getScanConfigIndex(nextscan) < 0))
					{
						this->openfile(lowestconfigindex, this->filesread[lowestconfigindex]++);
					}
					else
					{
						skipsomefiles = false;
					}
				}
			}
		}
	}
	if(this->input.is_open())
	{
		this->input.close();
	}
	if(numread > 0)
	{
		perr = pthread_mutex_unlock(&(this->bufferlock[this->lastvalidsegment]));
		if(perr != 0)
		{
			csevere << startl << "Error (" << perr << ") in readthread unlock of buffer section!" << this->lastvalidsegment << endl;
		}
	}

	//unlock the outstanding send lock
	perr = pthread_mutex_unlock(&this->outstandingsendlock);
	if(perr != 0)
	{
		csevere << startl << "Error (" << perr << ") in readthread unlock of outstandingsendlock!" << endl;
	}

	if(this->lastvalidsegment >= 0)
	{
		cverbose << startl << "Datastream readthread is exiting! Filecount was " << this->filesread[this->bufferinfo[this->lastvalidsegment].configindex] << ", confignumfiles was " << this->confignumfiles[this->bufferinfo[this->lastvalidsegment].configindex] << ", dataremaining was " << this->dataremaining << ", keepreading was " << this->keepreading << endl;
	}
	else
	{
		cverbose << startl << "Datastream readthread is exiting, after not finding any data at all!" << endl;
	}
}

// Explicitly instantiate template, must be at *end* of this cpp file
template class VDIFDataStream<ifstream>;
template class VDIFDataStream<VDIFFileReaderIStream>;

// vim: shiftwidth=2:softtabstop=2:expandtab

