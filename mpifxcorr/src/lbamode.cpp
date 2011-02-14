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
#include <mpi.h>
#include <iomanip>
#include "lbamode.h"
#include "math.h"
#include "architecture.h"
#include "alert.h"

//using namespace std;

const s16 LBAMode::stdunpackvalues[] = {MAX_S16/4, -MAX_S16/4 - 1, 3*MAX_S16/4, -3*MAX_S16/4 - 1};
const s16 LBAMode::vsopunpackvalues[] = {-3*MAX_S16/4 - 1, MAX_S16/4, -MAX_S16/4 - 1, 3*MAX_S16/4};

LBAMode::LBAMode(Configuration * conf, int confindex, int dsindex, int recordedbandchan, int chanstoavg, int bpersend, int gsamples, int nrecordedfreqs, double recordedbw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int nrecordedbands, int nzoombands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs, const s16* unpackvalues)
  : Mode(conf,confindex,dsindex,recordedbandchan,chanstoavg,bpersend,gsamples,nrecordedfreqs,recordedbw,recordedfreqclkoffs,recordedfreqlooffs,nrecordedbands,nzoombands,nbits,Configuration::REAL,recordedbandchan*2,fbank,fringerotorder,arraystridelen,cacorrs,(recordedbw<16.0)?recordedbw*2.0:32.0)
{
  int shift, outputshift;
  int count = 0;
  int numtimeshifts = (sizeof(u16)*bytesperblockdenominator)/bytesperblocknumerator;

  //build the lookup table - NOTE ASSUMPTION THAT THE BYTE ORDER IS **LITTLE-ENDIAN**!!!
  for(u16 i=0;i<MAX_U16;i++)
  {
    shift = 0;
    for(int j=0;j<numtimeshifts;j++)
    {
      for(int k=0;k<numrecordedbands;k++)
      {
        for(int l=0;l<samplesperblock;l++)
        {
          if(samplesperblock > 1 && numrecordedbands > 1) //32 MHz or 64 MHz dual pol
            if(samplesperblock == 4) //64 MHz
              outputshift = 3*(2-l) - 3*k;
            else
              outputshift = -k*samplesperblock + k + l;
          else if (samplesperblock == 4) //64 MHz single pol
	    outputshift = -2*l + 3;
	  else
            outputshift = 0;

          //if(samplesperblock > 1 && numinputbands > 1) //32 MHz or 64 MHz dual pol
          //  outputshift = (2 - (k + l))*(samplesperblock-1);
          //else
          //  outputshift = 0;

          //littleendian means that the shift, starting from 0, will go through the MS byte, then the LS byte, just as we'd like
          lookup[count + outputshift] = unpackvalues[(i >> shift) & 0x03];
          shift += 2;
          count++;
        }
      }
    }
  }

  //get the last values, i = 1111111111111111
  for (int i=0;i<samplesperlookup;i++)
  {
    lookup[count + i] = unpackvalues[3]; //every sample is 11 = 3
  }
}

LBA8BitMode::LBA8BitMode(Configuration * conf, int confindex, int dsindex, int recordedbandchan, int chanstoavg, int bpersend, int gsamples, int nrecordedfreqs, double recordedbw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int nrecordedbands, int nzoombands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs)
  : Mode(conf,confindex,dsindex,recordedbandchan,chanstoavg,bpersend,gsamples,nrecordedfreqs,recordedbw,recordedfreqclkoffs,recordedfreqlooffs,nrecordedbands,nzoombands,nbits,Configuration::REAL,recordedbandchan*2,fbank,fringerotorder,arraystridelen,cacorrs,recordedbw*2.0)
{}

float LBA8BitMode::unpack(int sampleoffset)
{
  unsigned char * packed = (unsigned char *)(&(data[((unpackstartsamples/samplesperblock)*bytesperblocknumerator)/bytesperblockdenominator]));

  for(int i=0;i<unpacksamples;i++)
  {
    for(int j=0;j<numrecordedbands;j++)
    {
      unpackedarrays[j][i] = (float)(*packed) - 128.0;
      packed++;
    }
  }
  return 1.0;
}

LBA16BitMode::LBA16BitMode(Configuration * conf, int confindex, int dsindex, int recordedbandchan, int chanstoavg, int bpersend, int gsamples, int nrecordedfreqs, double recordedbw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int nrecordedbands, int nzoombands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs)
  : Mode(conf,confindex,dsindex,recordedbandchan,chanstoavg,bpersend,gsamples,nrecordedfreqs,recordedbw,recordedfreqclkoffs,recordedfreqlooffs,nrecordedbands,nzoombands,nbits,Configuration::REAL,recordedbandchan*2,fbank,fringerotorder,arraystridelen,cacorrs,recordedbw*2.0)
{}

float LBA16BitMode::unpack(int sampleoffset)
{
  unsigned short * packed = (unsigned short *)(&(data[((unpackstartsamples/samplesperblock)*bytesperblocknumerator)/bytesperblockdenominator]));

  for(int i=0;i<unpacksamples;i++)
  {
    for(int j=0;j<numrecordedbands;j++)
    {
      unpackedarrays[j][i] = (float)(*packed) - 32768.0;
      packed++;
    }
  }
  return 1.0;
}
// vim: shiftwidth=2:softtabstop=2:expandtab
