/***************************************************************************
 *   Copyright (C) 2006 by Adam Deller                                     *
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
// $Id$
// $HeadURL$
// $LastChangedRevision$
// $Author$
// $LastChangedDate$
//
//============================================================================
#ifndef LBAMODE_H
#define LBAMODE_H

#include "mode.h"
#include "architecture.h"
#include "configuration.h"
#include <cstdlib>

using namespace std;
class Mode;

/** 
 @class LBAMode 
 @brief A mode for 'standard' LBA 2 bit data

 Assumes data has been compressed if running at 128 Mbps or lower ie no redundancy.  Assumes running on a LITTLE-ENDIAN MACHINE!!!
 @author Adam Deller
 */
class LBAMode : public Mode{
public:
 /**
  * Constructor: calls Mode constructor then creates lookup table
  * @param conf The configuration object, containing all information about the duration and setup of this correlation
  * @param confindex The index of the configuration this Mode is for
  * @param dsindex The index of the datastream this Mode is for
  * @param nchan The number of channels per subband
  * @param chanstoavg The number of channels to average for each subband
  * @param bpersend The number of FFT blocks to be processed in a message
  * @param gblocks The number of additional guard blocks at the end of a message
  * @param nfreqs The number of frequencies for this Mode
  * @param bw The bandwidth of each of these IFs
  * @param recordedfreqclkoffs The time offsets in microseconds to be applied post-F for each of the frequencies
  * @param recordedfreqlooffs The LO offsets in Hz for each recorded frequency
  * @param ninputbands The total number of subbands recorded
  * @param noutputbands The total number of subbands after prefiltering - not currently used (must be = numinputbands)
  * @param nbits The number of bits per sample
  * @param fbank Whether to use a polyphase filterbank to channelise (instead of FFT)
  * @param fringerotorder The interpolation order across an FFT (Oth, 1st or 2nd order; 0th = post-F)
  * @param arraystridelen The number of samples to stride when doing complex multiplies to implement sin/cos operations efficiently
  * @param cacorrs Whether cross-polarisation autocorrelations are to be calculated
  * @param unpackvalues 4 element array containing floating point unpack values for the four possible two bit values
  */
    LBAMode(Configuration * conf, int confindex, int dsindex, int nchan, int chanstoavg, int bpersend, int gblocks, int nfreqs, double bw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int ninputbands, int noutputbands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs, const s16* unpackvalues);

    ///unpack mapping for "standard" recording modes
    static const s16 stdunpackvalues[];
    ///unpack mapping for "vsop-style" recording modes
    static const s16 vsopunpackvalues[];
};

/**
 @class LBA8BitMode
 @brief A mode for 'Bruce' style LBA 8 bit data

 Assumes running on a LITTLE-ENDIAN MACHINE!!!
 Also assumes Nyquist sampled clock
 @author Adam Deller
 */
class LBA8BitMode : public Mode{
public:
  LBA8BitMode(Configuration * conf, int confindex, int dsindex, int nchan, int chanstoavg, int bpersend, int gblocks, int nfreqs, double bw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int ninputbands, int noutputbands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs);

  virtual float unpack(int sampleoffset);
};

/**
 @class LBA16BitMode
 @brief A mode for 'Bruce' style LBA 16 bit data

 Assumes running on a LITTLE-ENDIAN MACHINE and the byte order of the 16 bit samples is little endian!!!
 Also assumes Nyquist sampled clock
 @author Adam Deller
 */
class LBA16BitMode : public Mode{
public:
  LBA16BitMode(Configuration * conf, int confindex, int dsindex, int nchan, int chanstoavg, int bpersend, int gblocks, int nfreqs, double bw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int ninputbands, int noutputbands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs);

  virtual float unpack(int sampleoffset);
};

#endif
// vim: shiftwidth=2:softtabstop=2:expandtab
