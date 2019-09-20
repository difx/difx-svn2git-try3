/***************************************************************************
 *   Copyright (C) 2006-2016 by Adam Deller                                *
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
// $Id: datastream.h 7249 2016-02-24 10:43:39Z AdamDeller $
// $HeadURL: https://svn.atnf.csiro.au/difx/mpifxcorr/trunk/src/datastream.h $
// $LastChangedRevision: 7249 $
// $Author: AdamDeller $
// $LastChangedDate: 2016-02-24 11:43:39 +0100 (Wed, 24 Feb 2016) $
//
//============================================================================

#ifndef IDATASTREAM_H
#define IDATASTREAM_H

#define __STDC_LIMIT_MACROS

#include <mpi.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include "architecture.h"
#include "configuration.h"
#include "datamuxer.h"
#include "switchedpower.h"

using namespace std;

/**
@class IDataStream 
@brief Interface class for DataStream class, without templating.

Interface funcs load data into memory from a disk or network connection, calculate geometric delays and send data to Cores.

This class manages a stream of data from a disk or memory, coarsely aligning it with the geocentre and sending segments of 
data to Core nodes for processing as directed by the FxManager.  Defaults are for LBA-style file and frame headers - the
appropriate methods are virtual so Datastream can be subclassed to give altered functionality for different data formats

@author Adam Deller
*/
class IDataStream {
public:
 /**
  * Constructor: Copies the information passed to it - does not do other initialisation as it can be subclassed and different functionality is needed
  * @param conf The configuration object, containing all information about the duration and setup of this correlation
  * @param snum This Datastream's index (numbered from 0)
  * @param id This Datastream's MPI id
  * @param ncores The number of Cores in this correlation
  * @param cids Array containing the MPI ids of each Core
  * @param bufferfactor The size of the buffer, in terms of number of "max send sizes" - the biggest "blocks per send*numchannels" from the possible configurations
  * @param numsegments The number of separate segments this buffer will be divided into
  */
  IDataStream() { }

  virtual ~IDataStream() { }

 /**
  * Creates all arrays, initialises the reading thread and loads delays from the precomputed delay file
  */
  virtual void initialise() = 0;

 /**
  * While the correlation continues, keep accepting control information from the FxManager and sending data to the appropriate
  * Cores, while maintaining fresh data in the buffer
  */
  virtual void execute() = 0;

 /**
  * Returns the estimated number of bytes used by the Datastream
  * @return Estimated memory size of the Datastream (bytes)
  */
  virtual long long getEstimatedBytes() const = 0;

protected:
 /** 
  * Updates all the parameters (numchannels, sendbytes etc) for the specified segment of the databuffer
  * @param segmentindex The index of the segment to be updated
  */
  virtual void updateConfig(int segmentindex) = 0;

 /** 
  * Reads in the header information from an (LBA-style) file and sets the current segment time information accordingly
  * Should take into account intclockseconds when setting the file time and readseconds
  * @param configindex The config index at the current time
  * @param fileindex The number of the file to be opened
  */
  virtual void initialiseFile(int configindex, int fileindex) = 0;

  virtual void initialiseFake(int configindex) = 0;

 /** 
  * Reads in the header information from a buffer read in from the network, and sets the current segment time information accordingly
  * Should take into account intclockseconds when setting the file time and readseconds
  * Analogous to initialiseFile
  * @param frameheader Buffer containing the frame header
  * @return 0 on success, -1 on failure
  */
  virtual int initialiseFrame(char * frameheader) = 0;

 /** 
  * Calculates the correct offset from the start of the databuffer for a given time in the correlation, 
  * and calculates valid bits for each FFT block as control information to pass to the Cores
  * @param scan The scan to calculate for
  * @param offsetsec The offset in seconds from the start of the scan
  * @param offsetns The offset in nanoseconds from the given second
  * @return The offset in bytes from the start of the databuffer that this block should start from - must be between 0 and bufferlength
  */
  virtual int calculateControlParams(int scan, int offsetsec, int offsetns) = 0;
  
 /** 
  * Attempts to open the specified file for reading
  * @param configindex The config index at the current time
  * @param fileindex The number of the file to be opened
  */
  virtual void openfile(int configindex, int fileindex) = 0;

 /** 
  * Attempts to open the specified file and peeks at what scan it belongs to
  * @param configindex The config index at the current time
  * @param fileindex The number of the file to be opened
  * @return The scan that the start of the file belongs to
  */
  virtual int peekfile(int configindex, int fileindex) = 0;

 /** 
  * Attempts to open the next frame by reading data from the open network socket
  * @return 0 on failure, otherwise the framesize in bytes
  */
  virtual uint64_t openframe() = 0;

 /** 
  * While the correlation continues and there are more files to be read, continues reading data into the databuffer as fast as possible
  */
  virtual void loopfileread() = 0;

 /** 
  * While the correlation continues keep generating fake data
  */
  virtual void loopfakeread() = 0;

 /** 
  * While the correlation continues and the network socket remains open, continues reading data into the databuffer as fast as possible
  */
  virtual void loopnetworkread() = 0;

 /** 
  * Reads one segment's worth of data from the currently open file into the specified segment
  * @param buffersegment The segment of the databuffer that this read will be stored in
  */
  virtual void diskToMemory(int buffersegment) = 0;

 /** 
  * Generates one segment's worth of data and places into the specified segment
  * @param buffersegment The segment of the databuffer that this read will be stored in
  */
  virtual void fakeToMemory(int buffersegment) = 0;

 /** 
  * Reads one segment's worth of data from the currently open network socket into the specified segment
  * @param buffersegment The segment of the databuffer that this read will be stored in
  * @param framebytesremaining The number of bytes left in this frame before reading a new frame is necessary
  */
  virtual void networkToMemory(int buffersegment, uint64_t & framebytesremaining) = 0;

 /** 
  * Reads the specified number of bytes from the specified socket into the provided buffer
  * @param sock The network socket being read from
  * @param ptr The buffer to store read data in
  * @param bytestoread The number of bytes to read from the socket
  * @param nread The number of bytes actually read
  */
  virtual int readnetwork(int sock, char* ptr, int bytestoread, unsigned int* nread) = 0;

 /** 
  * Reads the specified number of bytes from the specified raw socket into the provided buffer
  * @param sock The network socket being read from
  * @param ptr The buffer to store read data in
  * @param bytestoread The number of bytes to read from the socket
  * @param nread The number of bytes actually read
  * @param packetsize Reject all packets not this size
  * @param stripbytes Remove this many bytes from the begining of each packet before storing
  */
  virtual int readrawnetwork(int sock, char* ptr, int bytestoread, unsigned int* nread, int packetsize, int stripbytes) = 0;

 /**
  * Tests that sync has not been lost (assuming the format supports this)
  * @param configindex The current configuration index
  * @param buffersegment The segment of the databuffer that the sync will be tested in
  * @return The number of bytes which must be read in (0 unless sync was lost and must be regained)
  */
  virtual int testForSync(int configindex, int buffersegment) = 0;
  
  /** 
   * Checks a data stream is valid 
   * @param buffersegment The segment of the data buffer that will be checked
   */
  virtual int checkData(int buffersegment) = 0;

 /**
  * Reads one chunk of data into the demux object
  * @param isfirst True if this is the very first read, meaning demux object must be initialised
  * @return number of bytes read in (and subsequently deinterlaced)
  */
  virtual int readonedemux(bool isfirst) = 0;
};

#endif
// vim: shiftwidth=2:softtabstop=2:expandtab
