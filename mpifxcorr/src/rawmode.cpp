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
// $Id: rawmode.cpp 2177 2010-05-04 11:52:42Z JanWagner $
// $HeadURL: $
// $LastChangedRevision: 2177 $
// $Author: JanWagner $
// $LastChangedDate: 2010-05-04 13:52:42 +0200 (Tue, 04 May 2010) $
//
//============================================================================
#include "rawmode.h"
#include "alert.h"

RawMode::RawMode(Configuration * conf, int confindex, int dsindex, int nchan, int bpersend, int gblocks, int nfreqs, double bw, double * freqclkoffsets, int ninputbands, int noutputbands, int nbits, bool fbank, bool postffringe, bool quaddelayinterp, bool cacorrs, int framebytes, int framesamples, Configuration::dataformat format)
  : Mode(conf, confindex, dsindex, nchan, bpersend, gblocks, nfreqs, bw, freqclkoffsets, ninputbands, noutputbands, nbits, nchan*2+4, fbank, postffringe, quaddelayinterp, cacorrs, bw*2)
{
  samplestounpack = (nbits/8) * nchan;
  if (!RawMode::handles(nbits, nchan)) {
      cfatal << startl << "Format error: " << nbits << " bits x " << nchan << " channels not supported by RawMode" << endl;
      initok = false;
  }
  return;
}

RawMode::~RawMode()
{
  return;
}

float RawMode::unpack(int sampleoffset)
{
  for (int i=0; i<samplestounpack; i++) {
     size_t off = size_t(i) + size_t(sampleoffset);
     unpackedarrays[0][i] = float((signed char)data[off]);
  }
  return 1.0f;
}
