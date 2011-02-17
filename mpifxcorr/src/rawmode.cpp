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

// TODO: first must have a true generic file DataStream base class, not a LBA-specific DataStream "base" class

RawMode::RawMode(Configuration * conf, int confindex, int dsindex, int recordedbandchan, int chanstoavg, int bpersend, int gsamples, int nrecordedfreqs, double recordedbw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int nrecordedbands, int nzoombands, int nbits, Configuration::datasampling sampling, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs, int framebytes, int framesamples, Configuration::dataformat format)
 : Mode(conf, confindex, dsindex, recordedbandchan, chanstoavg, bpersend, gsamples, nrecordedfreqs, recordedbw, recordedfreqclkoffs, recordedfreqlooffs, nrecordedbands, nzoombands, nbits, sampling, recordedbandchan*1 /*=unpacksamp*/, fbank, fringerotorder, arraystridelen, cacorrs, recordedbw*2)
{
  if (!RawMode::handles(nbits, recordedbandchan)) 
  {
      cfatal << startl << "Format error: " << nbits << " bits x " << recordedbandchan << " channels not supported by RawMode" << endl;
      initok = false;
  }

  int factor = (!usecomplex) ? 1 : 2;
  unpacksamples = factor * recordedbandchan;
  _samplestounpack = factor * recordedbandchan;
  return;
}

RawMode::~RawMode()
{
  return;
}

float RawMode::unpack(int sampleoffset)
{
  size_t off = size_t(sampleoffset);
  for (int i=0; i<_samplestounpack && off<datalengthbytes; i++, off++) 
  {
     unpackedarrays[0][i] = float((signed char)data[off]);
  }
 
  // always 100% of unpacked samples are valid
  return 1.0f;
}
