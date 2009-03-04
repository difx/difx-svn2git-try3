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
#include <string.h>
//#include "mk5.h"
#include "mk5mode.h"
#include "configuration.h"
#include "mode.h"
//#include "datastream.h"
//#include "mk5.h"
//#include "nativemk5.h"
#include "alert.h"

Configuration::Configuration(const char * configfile, int id)
  : mpiid(id), consistencyok(true)
{
  sectionheader currentheader = INPUT_EOF;
  commonread = false;
  datastreamread = false;
  configread = false;
  freqread = false;
  maxnumchannels = 0;
  uvw = NULL;
  
  //open the file
  ifstream * input = new ifstream(configfile);
  if(input->fail() || !input->is_open())
  {
    cfatal << startl << "Error opening file " << configfile << " - aborting!!!" << endl;
    consistencyok = false;
  }
  else
    currentheader = getSectionHeader(input);

  //go through all the sections and tables in the input file
  while(consistencyok && currentheader != INPUT_EOF)
  {
    switch(currentheader)
    {
      case COMMON:
        processCommon(input);
        break;
      case CONFIG:
        if(!commonread)
        {
          cfatal << startl << "Error - input file out of order!  Attempted to read configuration details without knowledge of common settings - aborting!!!" << endl;
          consistencyok = false;
        }
        else
          consistencyok = consistencyok && processConfig(input);
        break;
      case FREQ:
        processFreqTable(input);
        break;
      case TELESCOPE:
        processTelescopeTable(input);
        break;
      case DATASTREAM:
        if(!configread || ! freqread)
        {
          cfatal << startl << "Error - input file out of order!  Attempted to read datastreams without knowledge of one or both of configs/freqs - aborting!!!" << endl;
          consistencyok = false;
        }
        else
          consistencyok = consistencyok && processDatastreamTable(input);
        break;
      case BASELINE:
        consistencyok = consistencyok && processBaselineTable(input);
        break;
      case DATA:
        if(!datastreamread)
        {
          cfatal << startl << "Error - input file out of order!  Attempted to read datastream data files without knowledge of datastreams - aborting!!!" << endl;
          consistencyok = false;
        }
        else
          processDataTable(input);
        break;
      case NETWORK:
        if(!datastreamread)
        {
          cfatal << startl << "Error - input file out of order!  Attempted to read datastream network details without knowledge of datastreams - aborting!!!" << endl;
          consistencyok = false;
        }
        else
          processNetworkTable(input);
        break;
      default:
        break;
    }
    currentheader = getSectionHeader(input);
  }
  if(!configread)
  {
    cfatal << startl << "Error - no config section in input file - aborting!!!" << endl;
    consistencyok = false;
  }
  input->close();
  delete input;
  //work out which frequencies are used in each config
  for(int i=0;i<numconfigs;i++)
  {
    configs[i].frequsedbybaseline = new bool[freqtablelength];
    for(int j=0;j<freqtablelength;j++)
      configs[i].frequsedbybaseline[j] = false;
    for(int j=0;j<numbaselines;j++)
    {
      for(int k=0;k<baselinetable[configs[i].baselineindices[j]].numfreqs;k++)
      {
        //cout << "Setting frequency " << getBFreqIndex(i,j,k) << " used to true, from baseline " << j << ", baseline frequency " << k << endl; 
        configs[i].frequsedbybaseline[getBFreqIndex(i,j,k)] = true;
      }
    }
  }
  //process the pulsar configuration files
  for(int i=0;i<numconfigs;i++)
  {
    if(configs[i].pulsarbin)
    {
      if (consistencyok)
        consistencyok = processPulsarConfig(configs[i].pulsarconfigfilename, i);
      if (consistencyok)
        consistencyok = setPolycoFreqInfo(i);
    }
  }
  if (consistencyok)
    consistencyok = consistencyCheck();
  commandthreadinitialised = false;
  dumpsta = false;
  dumplta = false;
  stadumpchannels = DEFAULT_MONITOR_NUMCHANNELS;
  ltadumpchannels = DEFAULT_MONITOR_NUMCHANNELS;
//  cinfo << startl << "Finished processing input file!!!" << endl;
}


Configuration::~Configuration()
{
  if(configread)
  {
    for(int i=0;i<numconfigs;i++)
    {
      delete [] configs[i].datastreamindices;
      delete [] configs[i].baselineindices;
      delete [] configs[i].ordereddatastreamindices;
    }
    delete [] configs;
  }
  if(datastreamread)
  {
    for(int i=0;i<datastreamtablelength;i++)
    {
      for(int j=0;j<datastreamtable[i].numrecordedfreqs;j++)
      {
        delete [] datastreamtable[i].recordedfreqtableindices;
        delete [] datastreamtable[i].recordedfreqpols;
        delete [] datastreamtable[i].recordedfreqclockoffsets;
      }
      for(int j=0;j<datastreamtable[i].numzoomfreqs;j++)
      {
        delete [] datastreamtable[i].zoomfreqtableindices;
        delete [] datastreamtable[i].zoomfreqpols;
      }
      for(int j=0;j<datastreamtable[i].numrecordedbands;j++)
      {
        delete [] datastreamtable[i].recordedbandpols;
        delete [] datastreamtable[i].recordedbandlocalfreqindices;
      }
      for(int j=0;j<datastreamtable[i].numzoombands;j++)
      {
        delete [] datastreamtable[i].zoombandpols;
        delete [] datastreamtable[i].zoombandlocalfreqindices;
      }
      for(int j=0;j<datastreamtable[i].numdatafiles;j++)
        delete [] datastreamtable[i].datafilenames;
    }
    delete [] datastreamtable;
  }
  if(uvw)
    delete uvw;
  delete [] freqtable;
  delete [] telescopetable;
  for(int i=0;i<baselinetablelength;i++)
  {
    for(int j=0;j<baselinetable[i].numfreqs;j++)
    {
      delete [] baselinetable[i].datastream1bandindex[j];
      delete [] baselinetable[i].datastream2bandindex[j];
    }
    delete [] baselinetable[i].datastream1bandindex;
    delete [] baselinetable[i].datastream2bandindex;
    delete [] baselinetable[i].numpolproducts;
  }
  delete [] baselinetable;
  delete [] numprocessthreads;
}

int Configuration::genMk5FormatName(dataformat format, int nchan, double bw, int nbits, int framebytes, int decimationfactor, char *formatname)
{
  int fanout=1, mbps;

  mbps = int(2*nchan*bw*nbits + 0.5);

  switch(format)
  {
    case MKIV:
      fanout = framebytes*8/(20000*nbits*nchan);
      if(fanout*20000*nbits*nchan != framebytes*8)
      {
        cfatal << "genMk5FormatName : MKIV format : framebytes = " << framebytes << " is not allowed\n";
        return -1;
      }
      if(decimationfactor > 1)	// Note, this conditional is to ensure compatibility with older mark5access versions
        sprintf(formatname, "MKIV1_%d-%d-%d-%d/%d", fanout, mbps, nchan, nbits, decimationfactor);
      else
        sprintf(formatname, "MKIV1_%d-%d-%d-%d", fanout, mbps, nchan, nbits);
      break;
    case VLBA:
      fanout = framebytes*8/(20160*nbits*nchan);
      if(fanout*20160*nbits*nchan != framebytes*8)
      {
        cfatal << "genMk5FormatName : VLBA format : framebytes = " << framebytes << " is not allowed\n";
        return -1;
      }
      if(decimationfactor > 1)
        sprintf(formatname, "VLBA1_%d-%d-%d-%d/%d", fanout, mbps, nchan, nbits, decimationfactor);
      else
        sprintf(formatname, "VLBA1_%d-%d-%d-%d", fanout, mbps, nchan, nbits);
      break;
    case MARK5B:
      if(decimationfactor > 1)
        sprintf(formatname, "Mark5B-%d-%d-%d/%d", mbps, nchan, nbits, decimationfactor);
      else
        sprintf(formatname, "Mark5B-%d-%d-%d", mbps, nchan, nbits);
      break;
    default:
      cfatal << startl << "genMk5FormatName : unsupported format encountered\n" << endl;
      return -1;
  }

  return fanout;
}

int Configuration::getFramePayloadBytes(int configindex, int configdatastreamindex)
{
  int payloadsize;
  int framebytes = getFrameBytes(configindex, configdatastreamindex);
  dataformat format = getDataFormat(configindex, configdatastreamindex);
  
  switch(format)
  {
    case VLBA:
      payloadsize = (framebytes/2520)*2500;
      break;
    case MARK5B:
      payloadsize = framebytes - 16;
      break;
    default:
      payloadsize = framebytes;
  }

  return payloadsize;
}

void Configuration::getFrameInc(int configindex, int configdatastreamindex, int &sec, int &ns)
{
  int nchan, qb, decimationfactor;
  int payloadsize;
  double samplerate; /* in Hz */
  double seconds;

  nchan = getDNumRecordedBands(configindex, configdatastreamindex);
  samplerate = 2.0e6*getDRecordedBandwidth(configindex, configdatastreamindex, 0);
  qb = getDNumBits(configindex, configdatastreamindex);
  decimationfactor = getDDecimationFactor(configindex, configdatastreamindex);
  payloadsize = getFramePayloadBytes(configindex, configdatastreamindex);

  seconds = payloadsize*8/(samplerate*nchan*qb*decimationfactor);
  sec = int(seconds);
  ns = int(1.0e9*(seconds - sec));
}

int Configuration::getFramesPerSecond(int configindex, int configdatastreamindex)
{
  int nchan, qb, decimationfactor;
  int payloadsize;
  double samplerate; /* in Hz */

  nchan = getDNumRecordedBands(configindex, configdatastreamindex);
  samplerate = 2.0e6*getDRecordedBandwidth(configindex, configdatastreamindex, 0);
  qb = getDNumBits(configindex, configdatastreamindex);
  decimationfactor = getDDecimationFactor(configindex, configdatastreamindex);
  payloadsize = getFramePayloadBytes(configindex, configdatastreamindex);

  // This will always work out to be an integer 
  return int(samplerate*nchan*qb*decimationfactor/(8*payloadsize) + 0.5);
}

int Configuration::getMaxResultLength()
{
  int length;
  int maxlength = getResultLength(0);

  for(int i=1;i<numconfigs;i++)
  {
    length = getResultLength(i);
    if(length > maxlength)
      maxlength = length;
  }

  return maxlength;
}

int Configuration::getMaxDataBytes()
{
  int length;
  int maxlength = getDataBytes(0,0);

  for(int i=0;i<numconfigs;i++)
  {
    for(int j=0;j<numdatastreams;j++)
    {
      length = getDataBytes(i,j);
      if(length > maxlength)
        maxlength = length;
    }
  }

  return maxlength;
}

int Configuration::getMaxDataBytes(int datastreamindex)
{
  int length;
  int maxlength = getDataBytes(0,datastreamindex);

  for(int i=1;i<numconfigs;i++)
  {
    length = getDataBytes(i,datastreamindex);
    if(length > maxlength)
      maxlength = length;
  }

  return maxlength;
}

int Configuration::getMaxBlocksPerSend()
{
  int length;
  int maxlength = configs[0].blockspersend;

  for(int i=1;i<numconfigs;i++)
  {
    length = configs[i].blockspersend;
    if(length > maxlength)
      maxlength = length;
  }

  return maxlength;
}

int Configuration::getMaxNumRecordedFreqs()
{
  int currentnumfreqs, maxnumfreqs = 0;
  
  for(int i=0;i<numconfigs;i++)
  {
    currentnumfreqs = getMaxNumRecordedFreqs(i);
    if(currentnumfreqs > maxnumfreqs)
      maxnumfreqs = currentnumfreqs;
  }
  
  return maxnumfreqs;
}

int Configuration::getMaxNumRecordedFreqs(int configindex)
{
  int maxnumfreqs = 0;
  
  for(int i=0;i<numdatastreams;i++)
  {
    if(datastreamtable[configs[configindex].datastreamindices[i]].numrecordedfreqs > maxnumfreqs)
      maxnumfreqs = datastreamtable[configs[configindex].datastreamindices[i]].numrecordedfreqs;
  }
  
  return maxnumfreqs;
}

int Configuration::getMaxNumFreqDatastreamIndex(int configindex)
{
  int maxindex = 0;
  int maxnumfreqs = datastreamtable[configs[configindex].datastreamindices[0]].numrecordedfreqs;
  
  for(int i=1;i<numdatastreams;i++)
  {
    if(datastreamtable[configs[configindex].datastreamindices[i]].numrecordedfreqs > maxnumfreqs)
    {
      maxnumfreqs = datastreamtable[configs[configindex].datastreamindices[i]].numrecordedfreqs;
      maxindex = i;
    }
  }
  
  return maxindex;
}

int Configuration::getResultLength(int configindex)
{
  datastreamdata currentdatastream;
  int len = 0;
  int bandsperautocorr = (configs[configindex].writeautocorrs)?2:1;

  //add up all the bands in the baselines
  for(int i=0;i<numbaselines;i++) {
    for(int j=0;j<getBNumFreqs(configindex, i);j++)
      len += getBNumPolProducts(configindex, i, j)*(getFNumChannels(getBFreqIndex(configindex, i, j))+1);
  }

  //multiply this by number of pulsar bins if necessary
  if(configs[configindex].pulsarbin && !configs[configindex].scrunchoutput)
    len *= configs[configindex].numbins;

  //add all the bands from all the datastreams
  for(int i=0;i<numdatastreams;i++)
  {
    for(int j=0;j<getDNumRecordedBands(configindex, i);j++) {
      if(isFrequencyUsed(configindex, getDRecordedFreqIndex(configindex, i, j)))
        len += (getFNumChannels(getDRecordedFreqIndex(configindex, i, j))+1)*bandsperautocorr;
    }
    for(int j=0;j<getDNumZoomBands(configindex, i);j++) {
      if(isFrequencyUsed(configindex, getDZoomFreqIndex(configindex, i, j)))
        len += (getFNumChannels(getDZoomFreqIndex(configindex, i, j))+1)*bandsperautocorr;
    }
  }

  return len;
}

int Configuration::getDataBytes(int configindex, int datastreamindex)
{
  datastreamdata currentds = datastreamtable[configs[configindex].datastreamindices[datastreamindex]];
  freqdata arecordedfreq = freqtable[currentds.recordedfreqtableindices[0]]; 
  int validlength = (arecordedfreq.decimationfactor*configs[configindex].blockspersend*currentds.numrecordedbands*2*currentds.numbits*arecordedfreq.numchannels)/8;
  if(currentds.format == MKIV || currentds.format == VLBA || currentds.format == MARK5B)
  {
    //must be an integer number of frames, with enough margin for overlap on either side
    validlength += (arecordedfreq.decimationfactor*(int)(configs[configindex].guardns/(1000.0/(freqtable[currentds.recordedfreqtableindices[0]].bandwidth*2.0))+0.5)*currentds.numrecordedbands*2*currentds.numbits*arecordedfreq.numchannels)/8;
    return ((validlength/currentds.framebytes)+2)*currentds.framebytes;
  }
  else
    return validlength;
}

int Configuration::getMaxProducts(int configindex)
{
  baselinedata current;
  int maxproducts = 0;
  for(int i=0;i<numbaselines;i++)
  {
    //check is done in consistencyCheck - no longer necessary
    //if(configs[configindex].baselineindices[i] >= baselinetablelength || configs[configindex].baselineindices[i] < 0)
    //{
    //  cfatal << "Error - baselinetable index of " << configs[configindex].baselineindices[i] << " from config " << configindex << ", baseline " << i << " is outside of table range!!!" << endl;
    //  MPI_Abort(MPI_COMM_WORLD, 1);
    //}
    current = baselinetable[configs[configindex].baselineindices[i]];
    for(int j=0;j<current.numfreqs;j++)
    {
      if(current.numpolproducts[j] > maxproducts)
        maxproducts = current.numpolproducts[j];
    }
  }
  return maxproducts;
}

int Configuration::getMaxProducts()
{
  int maxproducts = 0;

  for(int i=0;i<numconfigs;i++)
  {
    if(getMaxProducts(i) > maxproducts)
      maxproducts = getMaxProducts(i);
  }
  
  return maxproducts;
}

int Configuration::getDMatchingBand(int configindex, int datastreamindex, int bandindex)
{
  datastreamdata ds = datastreamtable[configs[configindex].datastreamindices[datastreamindex]];
  if(bandindex >= ds.numrecordedbands) {
    for(int i=0;i<ds.numzoombands;i++)
    {
      if(ds.zoombandlocalfreqindices[bandindex] == ds.zoombandlocalfreqindices[i] && (i != bandindex))
        return i;
    }
  }
  else {
    for(int i=0;i<ds.numrecordedbands;i++)
    {
      if(ds.recordedbandlocalfreqindices[bandindex] == ds.recordedbandlocalfreqindices[i] && (i != bandindex))
        return i;
    }
  }

  return -1;
}

int Configuration::getCNumProcessThreads(int corenum)
{
  if(corenum < numcoreconfs)
    return numprocessthreads[corenum];
  return 1;
}

bool Configuration::loaduvwinfo(bool sourceonly)
{
  uvw = new Uvw(this, uvwfilename, sourceonly);
  return uvw->openSuccess();
}

bool Configuration::stationUsed(int telescopeindex)
{
  bool toreturn = false;

  for(int i=0;i<numconfigs;i++)
  {
    for(int j=0;j<numdatastreams;j++)
    {
      if(datastreamtable[configs[i].datastreamindices[j]].telescopeindex == telescopeindex)
        toreturn = true;
    }
  }

  return toreturn;
}

int Configuration::getConfigIndex(int offsetseconds)
{
  int currentconfigindex;
  string currentsourcename;

  if(!uvw)
  {
    cfatal << startl << "UVW HAS NOT BEEN CREATED!!!" << endl;
    return -1; //nasty, but this should never happen except in a programmer error.  Caller will probably crash.
  }

  uvw->getSourceName(startmjd, startseconds + offsetseconds, currentsourcename);
  currentconfigindex = defaultconfigindex;
  for(int i=0;i<numconfigs;i++)
  {
    if(configs[i].sourcename == currentsourcename.substr(0, (configs[i].sourcename).length()))
      currentconfigindex = i;
  }

  return currentconfigindex;
}

Mode* Configuration::getMode(int configindex, int datastreamindex)
{
  configdata conf = configs[configindex];
  datastreamdata stream = datastreamtable[conf.datastreamindices[datastreamindex]];
  int framesamples, framebytes;
  int guardsamples = (int)(conf.guardns/(1000.0/(freqtable[stream.recordedfreqtableindices[0]].bandwidth*2.0)) + 0.5);
  int streamrecbandchan = freqtable[stream.recordedfreqtableindices[0]].numchannels;
  int streamdecimationfactor = freqtable[stream.recordedfreqtableindices[0]].decimationfactor;

  switch(stream.format)
  {
    case LBASTD:
      if(stream.numbits != 2)
        cerror << startl << "ERROR! All LBASTD Modes must have 2 bit sampling - overriding input specification!!!" << endl;
      return new LBAMode(this, configindex, datastreamindex, streamrecbandchan, conf.blockspersend, guardsamples, stream.numrecordedfreqs, freqtable[stream.recordedfreqtableindices[0]].bandwidth, stream.recordedfreqclockoffsets, stream.recordedfreqlooffsets, stream.numrecordedbands, stream.numzoombands, 2/*bits*/, stream.filterbank, conf.postffringerot, conf.quadraticdelayinterp, conf.writeautocorrs, LBAMode::stdunpackvalues);
      break;
    case LBAVSOP:
      if(stream.numbits != 2)
        cerror << startl << "ERROR! All LBASTD Modes must have 2 bit sampling - overriding input specification!!!" << endl;
      return new LBAMode(this, configindex, datastreamindex, streamrecbandchan, conf.blockspersend, guardsamples, stream.numrecordedfreqs, freqtable[stream.recordedfreqtableindices[0]].bandwidth, stream.recordedfreqclockoffsets, stream.recordedfreqlooffsets, stream.numrecordedbands, stream.numzoombands, 2/*bits*/, stream.filterbank, conf.postffringerot, conf.quadraticdelayinterp, conf.writeautocorrs, LBAMode::vsopunpackvalues);
      break;
    case MKIV:
    case VLBA:
    case MARK5B:
      framesamples = getFramePayloadBytes(configindex, datastreamindex)*8/(getDNumBits(configindex, datastreamindex)*getDNumRecordedBands(configindex, datastreamindex)*streamdecimationfactor);
      framebytes = getFrameBytes(configindex, datastreamindex);
      return new Mk5Mode(this, configindex, datastreamindex, streamrecbandchan, conf.blockspersend, guardsamples, stream.numrecordedfreqs, freqtable[stream.recordedfreqtableindices[0]].bandwidth, stream.recordedfreqclockoffsets, stream.recordedfreqlooffsets, stream.numrecordedbands, stream.numzoombands, stream.numbits, stream.filterbank, conf.postffringerot, conf.quadraticdelayinterp, conf.writeautocorrs, framebytes, framesamples, stream.format);
      break;
    default:
      cerror << startl << "Error - unknown Mode!!!" << endl;
      return NULL;
  }
}

Configuration::sectionheader Configuration::getSectionHeader(ifstream * input)
{
  string line = "";

  while (line == "" && !input->eof())
    getline(*input, line); //skip the whitespace

  //return the type of section this is
  if(line.substr(0, 17) == "# COMMON SETTINGS")
    return COMMON;
  if(line.substr(0, 16) == "# CONFIGURATIONS")
    return CONFIG;
  if(line.substr(0, 12) == "# FREQ TABLE")
    return FREQ;
  if(line.substr(0, 17) == "# TELESCOPE TABLE")
    return TELESCOPE;
  if(line.substr(0, 18) == "# DATASTREAM TABLE")
    return DATASTREAM;
  if(line.substr(0, 16) == "# BASELINE TABLE")
    return BASELINE;
  if(line.substr(0, 12) == "# DATA TABLE")
    return DATA;
  if(line.substr(0, 15) == "# NETWORK TABLE")
    return NETWORK;

  if (input->eof())
    return INPUT_EOF;

  return UNKNOWN;
}

bool Configuration::processBaselineTable(ifstream * input)
{
  int tempint, dsband;
  int ** tempintptr;
  string line;
  datastreamdata dsdata;

  getinputline(input, &line, "BASELINE ENTRIES");
  baselinetablelength = atoi(line.c_str());
  baselinetable = new baselinedata[baselinetablelength];
  if(baselinetablelength < numbaselines)
  {
    cfatal << startl << "Error - not enough baselines are supplied in the baseline table (" << baselinetablelength << ") compared to the number of baselines (" << numbaselines << ")!!!" << endl;
    return false;
  }

  for(int i=0;i<baselinetablelength;i++)
  {
    //read in the info for this baseline
    baselinetable[i].totalbands = 0;
    getinputline(input, &line, "D/STREAM A INDEX ", i);
    baselinetable[i].datastream1index = atoi(line.c_str());
    getinputline(input, &line, "D/STREAM B INDEX ", i);
    baselinetable[i].datastream2index = atoi(line.c_str());
    getinputline(input, &line, "NUM FREQS ", i);
    baselinetable[i].numfreqs = atoi(line.c_str());
    baselinetable[i].numpolproducts = new int[baselinetable[i].numfreqs];
    baselinetable[i].datastream1bandindex = new int*[baselinetable[i].numfreqs];
    baselinetable[i].datastream2bandindex = new int*[baselinetable[i].numfreqs];
    baselinetable[i].freqtableindices = new int[baselinetable[i].numfreqs];
    baselinetable[i].polpairs = new char**[baselinetable[i].numfreqs];
    for(int j=0;j<baselinetable[i].numfreqs;j++)
    {
      getinputline(input, &line, "POL PRODUCTS ", i);
      baselinetable[i].numpolproducts[j] = atoi(line.c_str());
      baselinetable[i].datastream1bandindex[j] = new int[baselinetable[i].numpolproducts[j]];
      baselinetable[i].datastream2bandindex[j] = new int[baselinetable[i].numpolproducts[j]];
      baselinetable[i].polpairs[j] = new char*[baselinetable[i].numpolproducts[j]];
      for(int k=0;k<baselinetable[i].numpolproducts[j];k++)
      {
        baselinetable[i].totalbands++;
        getinputline(input, &line, "D/STREAM A BAND ", k);
        baselinetable[i].datastream1bandindex[j][k] = atoi(line.c_str());
        getinputline(input, &line, "D/STREAM B BAND ", k);
        baselinetable[i].datastream2bandindex[j][k] = atoi(line.c_str());
        baselinetable[i].polpairs[j][k] = new char[3];
      }
      dsdata = datastreamtable[baselinetable[i].datastream1index];
      dsband = baselinetable[i].datastream1bandindex[j][0];
      if(dsband >= dsdata.numrecordedbands) //it is a zoom band
        baselinetable[i].freqtableindices[j] = dsdata.zoomfreqtableindices[dsdata.zoombandlocalfreqindices[dsband-dsdata.numrecordedbands]];
      else
        baselinetable[i].freqtableindices[j] = dsdata.recordedfreqtableindices[dsdata.recordedbandlocalfreqindices[dsband]];
      for(int k=0;k<baselinetable[i].numpolproducts[j];k++) {
        dsdata = datastreamtable[baselinetable[i].datastream1index];
        dsband = baselinetable[i].datastream1bandindex[j][k];
        if(dsband >= dsdata.numrecordedbands) //it is a zoom band
          baselinetable[i].polpairs[j][k][0] = dsdata.zoombandpols[dsband-dsdata.numrecordedbands];
        else
          baselinetable[i].polpairs[j][k][0] = dsdata.recordedbandpols[dsband];
        dsdata = datastreamtable[baselinetable[i].datastream2index];
        dsband = baselinetable[i].datastream2bandindex[j][k];
        if(dsband >= dsdata.numrecordedbands) //it is a zoom band
          baselinetable[i].polpairs[j][k][1] = dsdata.zoombandpols[dsband-dsdata.numrecordedbands];
        else
          baselinetable[i].polpairs[j][k][1] = dsdata.recordedbandpols[dsband];
      }
    }
    if(datastreamtable[baselinetable[i].datastream1index].telescopeindex > datastreamtable[baselinetable[i].datastream2index].telescopeindex)
    {
      cerror << startl << "Error - first datastream for baseline " << i << " has a higher number than second datastream - reversing!!!" << endl;
      tempint = baselinetable[i].datastream1index;
      baselinetable[i].datastream1index = baselinetable[i].datastream2index;
      baselinetable[i].datastream2index = tempint;
      tempintptr = baselinetable[i].datastream1bandindex;
      baselinetable[i].datastream1bandindex = baselinetable[i].datastream2bandindex;
      baselinetable[i].datastream2bandindex = tempintptr;
    }
  }
  return true;
}

void Configuration::processCommon(ifstream * input)
{
  string line;

  getinputline(input, &delayfilename, "DELAY FILENAME");
  getinputline(input, &uvwfilename, "UVW FILENAME");
  getinputline(input, &coreconffilename, "CORE CONF FILENAME");
  getinputline(input, &line, "EXECUTE TIME (SEC)");
  executeseconds = atoi(line.c_str());
  getinputline(input, &line, "START MJD");
  startmjd = atoi(line.c_str());
  getinputline(input, &line, "START SECONDS");
  startseconds = atoi(line.c_str());
  startns = (int)((atof(line.c_str()) - ((double)startseconds))*1000000000.0 + 0.5);
  getinputline(input, &line, "ACTIVE DATASTREAMS");
  numdatastreams = atoi(line.c_str());
  getinputline(input, &line, "ACTIVE BASELINES");
  numbaselines = atoi(line.c_str());
  getinputline(input, &line, "VIS BUFFER LENGTH");
  visbufferlength = atoi(line.c_str());
  getinputline(input, &line, "OUTPUT FORMAT");
  if(line == "SWIN" || line == "DIFX")
  {
    outformat = DIFX;
  }
  else if(line == "ASCII")
  {
    outformat = ASCII;
  }
  else
  {
    cerror << startl << "Unknown output format " << line << " (case sensitive choices are SWIN, DIFX (same thing) and ASCII), assuming SWIN/DIFX" << endl;
    outformat = DIFX;
  }
  getinputline(input, &outputfilename, "OUTPUT FILENAME");

  commonread = true;
}

bool Configuration::processConfig(ifstream * input)
{
  string line;
  bool found;

  maxnumpulsarbins = 0;
  numindependentchannelconfigs = 0;

  getinputline(input, &line, "NUM CONFIGURATIONS");
  numconfigs = atoi(line.c_str());
  configs = new configdata[numconfigs];
  defaultconfigindex = -1;
  for(int i=0;i<numconfigs;i++)
  {
    found = false;
    configs[i].datastreamindices = new int[numdatastreams];
    configs[i].baselineindices = new int [numbaselines];
    getinputline(input, &(configs[i].sourcename), "CONFIG SOURCE");
    if(configs[i].sourcename == "DEFAULT")
      defaultconfigindex = i;
    getinputline(input, &line, "INT TIME (SEC)");
    configs[i].inttime = atof(line.c_str());
    getinputline(input, &line, "SUBINT NANOSECONDS");
    configs[i].subintns = atoi(line.c_str());
    getinputline(input, &line, "GUARD NANOSECONDS");
    configs[i].guardns = atoi(line.c_str());
    getinputline(input, &line, "POST-F FRINGE ROT");
    configs[i].postffringerot = ((line == "TRUE") || (line == "T") || (line == "true") || (line == "t"))?true:false;
    getinputline(input, &line, "QUAD DELAY INTERP");
    configs[i].quadraticdelayinterp = ((line == "TRUE") || (line == "T") || (line == "true") || (line == "t"))?true:false;
    if(configs[i].postffringerot && configs[i].quadraticdelayinterp)
    {
      cfatal << startl << "ERROR - cannot quad interpolate delays with post-f fringe rotation - aborting!!!" << endl;
      return false;
    }
    getinputline(input, &line, "WRITE AUTOCORRS");
    configs[i].writeautocorrs = ((line == "TRUE") || (line == "T") || (line == "true") || (line == "t"))?true:false;
    getinputline(input, &line, "PULSAR BINNING");
    configs[i].pulsarbin = ((line == "TRUE") || (line == "T") || (line == "true") || (line == "t"))?true:false;
    if(configs[i].pulsarbin)
    {
      getinputline(input, &configs[i].pulsarconfigfilename, "PULSAR CONFIG FILE");
    }
    for(int j=0;j<numdatastreams;j++)
    {
      getinputline(input, &line, "DATASTREAM ", j);
      configs[i].datastreamindices[j] = atoi(line.c_str());
    }
    for(int j=0;j<numbaselines;j++)
    {
      getinputline(input, &line, "BASELINE ", j);
      configs[i].baselineindices[j] = atoi(line.c_str());
    }
  }
  if(defaultconfigindex < 0)
  {
//    cinfo << startl << "Warning - no default config found - sources which were not specified will not be correlated!!!" << endl;
  }

  configread = true;
  return true;
}

bool Configuration::processDatastreamTable(ifstream * input)
{
  bool ok = true;
  string line;

  getinputline(input, &line, "DATASTREAM ENTRIES");
  datastreamtablelength = atoi(line.c_str());
  datastreamtable = new datastreamdata[datastreamtablelength];
  if(datastreamtablelength < numdatastreams)
  {
    cfatal << startl << "Error - not enough datastreams are supplied in the datastream table (" << datastreamtablelength << ") compared to the number of datastreams (" << numdatastreams << "!!!" << endl;
    return false;
  }
  //create the ordereddatastream array
  for(int i=0;i<numconfigs;i++)
    configs[i].ordereddatastreamindices = new int[datastreamtablelength];

  //get the information on the length of the internal buffer for the datastreams
  getinputline(input, &line, "DATA BUFFER FACTOR");
  databufferfactor = atoi(line.c_str());
  getinputline(input, &line, "NUM DATA SEGMENTS");
  numdatasegments = atoi(line.c_str());

  for(int i=0;i<datastreamtablelength;i++)
  {
    int configindex=-1;
    int decimationfactor = 1;

    //get configuration index for this datastream
    for(int c=0; c<numconfigs; c++)
    {
      for(int d=0; d<numdatastreams; d++)
      {
        if(configs[c].datastreamindices[d] == i)
        {
          configindex = c;
          break;
        }
      }
      if(configindex >= 0) break;
    }

    //read all the info for this datastream
    getinputline(input, &line, "TELESCOPE INDEX");
    datastreamtable[i].telescopeindex = atoi(line.c_str());
    getinputline(input, &line, "TSYS");
    datastreamtable[i].tsys = atof(line.c_str());
    getinputline(input, &line, "DATA FORMAT");
    if(line == "LBASTD")
      datastreamtable[i].format = LBASTD;
    else if(line == "LBAVSOP")
      datastreamtable[i].format = LBAVSOP;
    else if(line == "NZ")
      datastreamtable[i].format = NZ;
    else if(line == "K5")
      datastreamtable[i].format = K5;
    else if(line == "MKIV")
      datastreamtable[i].format = MKIV;
    else if(line == "VLBA")
      datastreamtable[i].format = VLBA;
    else if(line == "MARK5B")
      datastreamtable[i].format = MARK5B;
    else
    {
      cfatal << startl << "Unknown data format " << line << " (case sensitive choices are LBASTD, LBAVSOP, NZ, K5, MKIV, VLBA, and MARK5B)" << endl;
      return false;
    }
    getinputline(input, &line, "QUANTISATION BITS");
    datastreamtable[i].numbits = atoi(line.c_str());

    getinputline(input, &line, "DATA FRAME SIZE");
    datastreamtable[i].framebytes = atoi(line.c_str());

    getinputline(input, &line, "DATA SOURCE");
    if(line == "FILE")
      datastreamtable[i].source = UNIXFILE;
    else if(line == "MODULE")
      datastreamtable[i].source = MK5MODULE;
    else if(line == "EVLBI")
      datastreamtable[i].source = EVLBI;
    else
    {
      cfatal << startl << "Unnkown data source " << line << " (case sensitive choices are FILE, MK5MODULE and EVLBI)" << endl;
      return false;
    }

    getinputline(input, &line, "FILTERBANK USED");
    datastreamtable[i].filterbank = ((line == "TRUE") || (line == "T") || (line == "true") || (line == "t"))?true:false;
    if(datastreamtable[i].filterbank)
      cwarn << startl << "Error - filterbank not yet supported!!!" << endl;

    getinputline(input, &line, "RECORDED FREQS");
    datastreamtable[i].numrecordedfreqs = atoi(line.c_str());
    datastreamtable[i].recordedfreqpols = new int[datastreamtable[i].numrecordedfreqs];
    datastreamtable[i].recordedfreqtableindices = new int[datastreamtable[i].numrecordedfreqs];
    datastreamtable[i].recordedfreqclockoffsets = new double[datastreamtable[i].numrecordedfreqs];
    datastreamtable[i].recordedfreqlooffsets = new double[datastreamtable[i].numrecordedfreqs];
    datastreamtable[i].numrecordedbands = 0;
    for(int j=0;j<datastreamtable[i].numrecordedfreqs;j++)
    {
      getinputline(input, &line, "FREQ TABLE INDEX ", j);
      datastreamtable[i].recordedfreqtableindices[j] = atoi(line.c_str());
      getinputline(input, &line, "CLK OFFSET ", j);
      datastreamtable[i].recordedfreqclockoffsets[j] = atof(line.c_str());
      getinputline(input, &line, "FREQ OFFSET ", j); //Freq offset is positive if recorded LO frequency was higher than the frequency in the frequency table
      datastreamtable[i].recordedfreqlooffsets[j] = atof(line.c_str());
      getinputline(input, &line, "NUM POLS ", j);
      datastreamtable[i].recordedfreqpols[j] = atoi(line.c_str());
      datastreamtable[i].numrecordedbands += datastreamtable[i].recordedfreqpols[j];
    }
    decimationfactor = freqtable[datastreamtable[i].recordedfreqtableindices[0]].decimationfactor;
    datastreamtable[i].bytespersamplenum = (datastreamtable[i].numrecordedbands*datastreamtable[i].numbits*decimationfactor)/8;
    if(datastreamtable[i].bytespersamplenum == 0)
    {
      datastreamtable[i].bytespersamplenum = 1;
      datastreamtable[i].bytespersampledenom = 8/(datastreamtable[i].numrecordedbands*datastreamtable[i].numbits*decimationfactor);
    }
    else
      datastreamtable[i].bytespersampledenom = 1;
    datastreamtable[i].recordedbandpols = new char[datastreamtable[i].numrecordedbands];
    datastreamtable[i].recordedbandlocalfreqindices = new int[datastreamtable[i].numrecordedbands];
    for(int j=0;j<datastreamtable[i].numrecordedbands;j++)
    {
      getinputline(input, &line, "INPUT BAND ", j);
      datastreamtable[i].recordedbandpols[j] = *(line.data());
      getinputline(input, &line, "INPUT BAND ", j);
      datastreamtable[i].recordedbandlocalfreqindices[j] = atoi(line.c_str());
      if(datastreamtable[i].recordedbandlocalfreqindices[j] >= datastreamtable[i].numrecordedfreqs)
        cerror << startl << "Error - attempting to refer to freq outside local table!!!" << endl;
    }
    getinputline(input, &line, "ZOOM FREQS");
    datastreamtable[i].numzoomfreqs = atoi(line.c_str());
    datastreamtable[i].zoomfreqtableindices = new int[datastreamtable[i].numzoomfreqs];
    datastreamtable[i].zoomfreqpols = new int[datastreamtable[i].numzoomfreqs];
    datastreamtable[i].zoomfreqparentdfreqindices = new int[datastreamtable[i].numzoomfreqs];
    datastreamtable[i].zoomfreqchanneloffset = new int[datastreamtable[i].numzoomfreqs];
    datastreamtable[i].numzoombands = 0;
    for(int j=0;j<datastreamtable[i].numzoomfreqs;j++)
    {
      getinputline(input, &line, "ZOOM FREQ INDEX ");
      datastreamtable[i].zoomfreqtableindices[j] = atoi(line.c_str());
      getinputline(input, &line, "NUM ZOOM POLS ", j);
      datastreamtable[i].zoomfreqpols[j] = atoi(line.c_str());
      datastreamtable[i].numzoombands += datastreamtable[i].zoomfreqpols[j];
      datastreamtable[i].zoomfreqparentdfreqindices[j] = -1;
      for (int k=0;k<datastreamtable[i].numrecordedfreqs;k++) {
        double parentlowbandedge = freqtable[datastreamtable[i].recordedfreqtableindices[k]].bandedgefreq;
        double parenthighbandedge = freqtable[datastreamtable[i].recordedfreqtableindices[k]].bandedgefreq + freqtable[datastreamtable[i].recordedfreqtableindices[k]].bandwidth;
        if(freqtable[datastreamtable[i].recordedfreqtableindices[k]].lowersideband) {
          parentlowbandedge -= freqtable[datastreamtable[i].recordedfreqtableindices[k]].bandwidth;
          parenthighbandedge -= freqtable[datastreamtable[i].recordedfreqtableindices[k]].bandwidth;
        }
        double lowbandedge = freqtable[datastreamtable[i].zoomfreqtableindices[k]].bandedgefreq;
        double highbandedge = freqtable[datastreamtable[i].zoomfreqtableindices[k]].bandedgefreq + freqtable[datastreamtable[i].zoomfreqtableindices[k]].bandwidth;
        if(freqtable[datastreamtable[i].zoomfreqtableindices[k]].lowersideband) {
          parentlowbandedge -= freqtable[datastreamtable[i].zoomfreqtableindices[k]].bandwidth;
          parenthighbandedge -= freqtable[datastreamtable[i].zoomfreqtableindices[k]].bandwidth;
        }
        if (highbandedge < parenthighbandedge && lowbandedge > parentlowbandedge) {
          datastreamtable[i].zoomfreqparentdfreqindices[j] = k;
          datastreamtable[i].zoomfreqchanneloffset[j] = (int)(((lowbandedge - parentlowbandedge)/freqtable[datastreamtable[i].recordedfreqtableindices[0]].bandwidth)*freqtable[datastreamtable[i].recordedfreqtableindices[0]].numchannels);
          if (freqtable[datastreamtable[i].zoomfreqtableindices[j]].lowersideband)
            datastreamtable[i].zoomfreqchanneloffset[j] += freqtable[datastreamtable[i].zoomfreqtableindices[j]].numchannels;
        }
      }
    }
    datastreamtable[i].zoombandpols = new char[datastreamtable[i].numzoombands];
    datastreamtable[i].zoombandlocalfreqindices = new int[datastreamtable[i].numzoombands];
    for(int j=0;j<datastreamtable[i].numzoombands;j++)
    {
      getinputline(input, &line, "ZOOM BAND ", j);
      datastreamtable[i].zoombandpols[j] = *(line.data());
      getinputline(input, &line, "ZOOM BAND ", j);
      datastreamtable[i].zoombandlocalfreqindices[j] = atoi(line.c_str());
      if(datastreamtable[i].zoombandlocalfreqindices[j] >= datastreamtable[i].numzoomfreqs)
        cerror << startl << "Error - attempting to refer to freq outside local table!!!" << endl;
    }
  }

  for(int i=0;i<numconfigs;i++)
  {
    //work out blockspersend
    freqdata f = freqtable[datastreamtable[configs[i].datastreamindices[0]].recordedfreqtableindices[0]];
    double ffttime = 1000.0*f.numchannels/f.bandwidth;
    double bpersenddouble = configs[i].subintns/ffttime;
    configs[i].blockspersend = int(bpersenddouble + 0.5);
    if (fabs(bpersenddouble - configs[i].blockspersend) > Mode::TINY) {
      ok = false;
      cfatal << startl << "The supplied value of subint nanoseconds (" << configs[i].subintns << ") for config " << i << " does not yield an integer number of FFTs! (FFT time is " << ffttime << "). Aborting!" << endl;
    }
  }
  if(!ok)
    return false;

  //read in the core numthreads info
  ifstream coreinput(coreconffilename.c_str());
  numcoreconfs = 0;
  if(!coreinput.is_open() || coreinput.bad())
  {
    cerror << startl << "Error - could not open " << coreconffilename << " - will set all numthreads to 1!!" << endl;
  }
  else
  {
    getinputline(&coreinput, &line, "NUMBER OF CORES");
    int maxlines = atoi(line.c_str());
    numprocessthreads = new int[maxlines];
    getline(coreinput, line);
    for(int i=0;i<maxlines;i++)
    {
      if(coreinput.eof())
      {
        cerror << startl << "Warning - hit the end of the file! Setting the numthread for Core " << i << " to 1" << endl;
        numprocessthreads[numcoreconfs++] = 1;
      }
      else
      {
        numprocessthreads[numcoreconfs++] = atoi(line.c_str());
        getline(coreinput, line);
      }
    }
  }
  coreinput.close();

  datastreamread = true;
  return true;
}

void Configuration::processDataTable(ifstream * input)
{
  string line;

  for(int i=0;i<datastreamtablelength;i++)
  {
    getinputline(input, &line, "D/STREAM ", i);
    datastreamtable[i].numdatafiles = atoi(line.c_str());
    datastreamtable[i].datafilenames = new string[datastreamtable[i].numdatafiles];
    for(int j=0;j<datastreamtable[i].numdatafiles;j++)
      getinputline(input, &(datastreamtable[i].datafilenames[j]), "FILE ", i);
  }
}

void Configuration::processFreqTable(ifstream * input)
{
  string line;

  getinputline(input, &line, "FREQ ENTRIES");
  freqtablelength = atoi(line.c_str());
  freqtable = new freqdata[freqtablelength];

  for(int i=0;i<freqtablelength;i++)
  {
    getinputline(input, &line, "FREQ (MHZ) ", i);
    freqtable[i].bandedgefreq = atof(line.c_str());
    getinputline(input, &line, "BW (MHZ) ", i);
    freqtable[i].bandwidth = atof(line.c_str());
    getinputline(input, &line, "SIDEBAND ", i);
    freqtable[i].lowersideband = ((line == "L") || (line == "l") || (line == "LOWER") || (line == "lower"))?true:false;
    getinputline(input, &line, "NUM CHANNELS ");
    freqtable[i].numchannels = atoi(line.c_str());
    if(freqtable[i].numchannels > maxnumchannels)
      maxnumchannels = freqtable[i].numchannels;
    getinputline(input, &line, "CHANS TO AVG ");
    freqtable[i].channelstoaverage = atoi(line.c_str());
    getinputline(input, &line, "OVERSAMPLE FAC. ");
    freqtable[i].oversamplefactor = atoi(line.c_str());
    getinputline(input, &line, "DECIMATION FAC. ");
    freqtable[i].decimationfactor = atoi(line.c_str());
    freqtable[i].matchingwiderbandindex = -1;
    freqtable[i].matchingwiderbandoffset = -1;
  }
  //now look for matching wider bands
  for(int i=freqtablelength-1;i>0;i--)
  {
    double f1chanwidth = freqtable[i].bandwidth/freqtable[i].numchannels;
    double f1loweredge = freqtable[i].bandedgefreq;
    if (freqtable[i].lowersideband)
      f1loweredge -= freqtable[i].bandwidth;
    for(int j=i-1;j>=0;j--)
    {
      double f2chanwidth = freqtable[j].bandwidth/freqtable[j].numchannels;
      double f2loweredge = freqtable[j].bandedgefreq;
      if (freqtable[j].lowersideband)
        f2loweredge -= freqtable[j].bandwidth;
      if((i != j) && (f1chanwidth == f2chanwidth) && (f1loweredge < f2loweredge) &&
          (f1loweredge + freqtable[i].bandwidth > f2loweredge + freqtable[j].bandwidth))
      {
        freqtable[j].matchingwiderbandindex = i;
        freqtable[j].matchingwiderbandoffset = int(((f2loweredge-f1loweredge)/freqtable[i].bandwidth)*freqtable[i].numchannels + 0.5);
      }
    }
  }
  freqread = true;
}

void Configuration::processTelescopeTable(ifstream * input)
{
  string line;

  getinputline(input, &line, "TELESCOPE ENTRIES");
  telescopetablelength = atoi(line.c_str());
  telescopetable = new telescopedata[telescopetablelength];

  for(int i=0;i<telescopetablelength;i++)
  {
    getinputline(input, &(telescopetable[i].name), "TELESCOPE NAME ", i);
    getinputline(input, &line, "CLOCK DELAY (us) ", i);
    telescopetable[i].clockdelay = atof(line.c_str());
    getinputline(input, &line, "CLOCK RATE(us/s) ", i);
    telescopetable[i].clockrate = atof(line.c_str());
  }
}

void Configuration::processNetworkTable(ifstream * input)
{
  string line;

  for(int i=0;i<datastreamtablelength;i++)
  {
    getinputline(input, &line, "PORT NUM ", i);
    datastreamtable[i].portnumber = atoi(line.c_str());
    getinputline(input, &line, "TCP WINDOW (KB) ", i);
    datastreamtable[i].tcpwindowsizekb = atoi(line.c_str());
  }
}

bool Configuration::consistencyCheck()
{
  int tindex, count;
  double bandwidth, sampletimens, ffttime, numffts;
  datastreamdata ds;

  //check entries in the datastream table
  for(int i=0;i<datastreamtablelength;i++)
  {
    //check the telescope index is acceptable
    if(datastreamtable[i].telescopeindex < 0 || datastreamtable[i].telescopeindex >= telescopetablelength)
    {
      cerror << startl << "Error!!! Datastream table entry " << i << " has a telescope index (" << datastreamtable[i].telescopeindex << ") that refers outside the telescope table range (table length " << telescopetablelength << ")- aborting!!!" << endl;
      return false;
    }

    //check the recorded bands all refer to valid local freqs
    for(int j=0;j<datastreamtable[i].numrecordedbands;j++)
    {
      if(datastreamtable[i].recordedbandlocalfreqindices[j] < 0 || datastreamtable[i].recordedbandlocalfreqindices[j] >= datastreamtable[i].numrecordedfreqs)
      {
        cerror << startl << "Error!!! Datastream table entry " << i << " has an recorded band local frequency index (band " << j << ") which is equal to " << datastreamtable[i].recordedbandlocalfreqindices[j] << " that refers outside the local frequency table range (" << datastreamtable[i].numrecordedfreqs << ")- aborting!!!" << endl;
        return false;
      }
    }

    //check that the zoom mode bands also refer to valid local freqs
    for(int j=0;j<datastreamtable[i].numzoombands;j++)
    {
      if(datastreamtable[i].zoombandlocalfreqindices[j] < 0 || datastreamtable[i].zoombandlocalfreqindices[j] >= datastreamtable[i].numzoomfreqs)
      {
        cerror << startl << "Error!!! Datastream table entry " << i << " has an zoom band local frequency index (band " << j << ") which is equal to " << datastreamtable[i].zoombandlocalfreqindices[j] << " that refers outside the local frequency table range (" << datastreamtable[i].numzoomfreqs << ")- aborting!!!" << endl;
        return false;
      }
    }

    //check that all zoom freqs come later in the freq table than regular freqs
    for(int j=0;j<datastreamtable[i].numrecordedfreqs;j++)
    {
      int rfreqtableindex = datastreamtable[i].recordedfreqtableindices[j];
      for(int k=0;k<datastreamtable[i].numzoomfreqs;k++)
      {
        if(datastreamtable[i].zoomfreqtableindices[k] < rfreqtableindex)
        {
          cerror << startl << "Error!!! Datastream table entry " << i << " has a zoom band (index " << k << ") which comes earlier in the freq table than a recorded band (index " << j << ") - aborting!!!" << endl;
          return false;
        }
      }
    }

    //check the frequency table indices are ok and all the bandwidths, number of channels, oversampling etc match for the recorded freqs
    bandwidth = freqtable[datastreamtable[i].recordedfreqtableindices[0]].bandwidth;
    int oversamp = freqtable[datastreamtable[i].recordedfreqtableindices[0]].oversamplefactor;
    int decim = freqtable[datastreamtable[i].recordedfreqtableindices[0]].decimationfactor;
    int toaver = freqtable[datastreamtable[i].recordedfreqtableindices[0]].channelstoaverage;
    if(oversamp < decim)
    {
      cerror << startl << "Error - oversamplefactor (" << oversamp << ") is less than decimation factor (" << decim << ") - aborting!!!" << endl;
      return false;
    }
    for(int j=0;j<datastreamtable[i].numrecordedfreqs;j++)
    {
      if(datastreamtable[i].recordedfreqtableindices[j] < 0 || datastreamtable[i].recordedfreqtableindices[j] >= freqtablelength)
      {
        cerror << startl << "Error!!! Datastream table entry " << i << " has a recorded frequency index (freq " << j << ") which is equal to " << datastreamtable[i].recordedfreqtableindices[j] << " that refers outside the frequency table range (" << freqtablelength << ") - aborting!!!" << endl;
        return false;
      }
      if(bandwidth != freqtable[datastreamtable[i].recordedfreqtableindices[j]].bandwidth)
      {
        cerror << startl << "Error - all recorded bandwidths for a given datastream must be equal - Aborting!!!!" << endl;
        return false;
      }
      if(oversamp != freqtable[datastreamtable[i].recordedfreqtableindices[j]].oversamplefactor)
      {
        cerror << startl << "Error - all recorded oversample factors for a given datastream must be equal - Aborting!!!!" << endl;
        return false;
      }
      if(decim != freqtable[datastreamtable[i].recordedfreqtableindices[j]].decimationfactor)
      {
        cerror << startl << "Error - all recorded decimations for a given datastream must be equal - Aborting!!!!" << endl;
        return false;
      }
      if(toaver != freqtable[datastreamtable[i].recordedfreqtableindices[j]].channelstoaverage)
      {
        cerror << startl << "Error - all recorded channels to average for a given datastream must be equal - Aborting!!!!" << endl;
        return false;
      }
    }

    //repeat for the zoom freqs, also check that they fit into a recorded freq and the channel widths match, and the polarisations match
    for(int j=0;j<datastreamtable[i].numzoomfreqs;j++)
    {
      if(datastreamtable[i].zoomfreqtableindices[j] < 0 || datastreamtable[i].zoomfreqtableindices[j] >= freqtablelength)
      {
        cerror << startl << "Error!!! Datastream table entry " << i << " has a zoom frequency index (freq " << j << ") which is equal to " << datastreamtable[i].zoomfreqtableindices[j] << " that refers outside the frequency table range (" << freqtablelength << ")- aborting!!!" << endl;
        return false;
      }
      if(datastreamtable[i].zoomfreqparentdfreqindices[j] < 0) {
        cerror << startl << "Error!!! Datastream table entry " << i << " has a zoom frequency index (freq " << j << ") which does not fit into any of the recorded bands - aborting!!!" << endl;
        return false;
      }
      double zoomfreqchannelwidth = freqtable[datastreamtable[i].zoomfreqtableindices[j]].bandwidth/freqtable[datastreamtable[i].zoomfreqtableindices[j]].numchannels;
      double parentfreqchannelwidth = freqtable[datastreamtable[i].recordedfreqtableindices[datastreamtable[i].zoomfreqparentdfreqindices[j]]].bandwidth/freqtable[datastreamtable[i].recordedfreqtableindices[datastreamtable[i].zoomfreqparentdfreqindices[j]]].numchannels;
      if(fabs(zoomfreqchannelwidth - parentfreqchannelwidth) > Mode::TINY) {
        cerror << startl << "Error!!! Datastream table entry " << i << " has a zoom frequency index (freq " << j << ") whose channel width (" << zoomfreqchannelwidth << ") does not match its parents channel width (" << parentfreqchannelwidth << ") - aborting!!!" << endl;
        return false;
      }
    }

    //check that each zoom band has actually been recorded in the same polarisation
    for(int j=0;j<datastreamtable[i].numzoombands;j++) {
      bool matchingpol = false;
      for(int k=0;k<datastreamtable[i].numrecordedbands;k++) {
        if(datastreamtable[i].zoomfreqparentdfreqindices[datastreamtable[i].zoombandlocalfreqindices[j]] == datastreamtable[i].recordedbandlocalfreqindices[k]) {
          if (datastreamtable[i].zoombandpols[j] == datastreamtable[i].recordedbandpols[k])
            matchingpol = true;
        }
      }
      if(!matchingpol) {
        cerror << startl << "Error!!! Datastream table entry " << i << " has a zoom band (band " << j << ") which does have have a parent band of the same polarisation (" << datastreamtable[i].zoombandpols[j] << ") - aborting!" << endl;
        return false;
      }
    }
  }

  //check that for all configs, the datastreams refer to the same telescope
  for(int i=0;i<numdatastreams;i++)
  {
    tindex = datastreamtable[configs[0].datastreamindices[i]].telescopeindex;
    for(int j=1;j<numconfigs;j++)
    {
      if(tindex != datastreamtable[configs[0].datastreamindices[i]].telescopeindex)
      {
        cerror << startl << "Error - all configs must have the same telescopes!  Config " << j << " datastream " << i << " refers to different telescopes - aborting!!!" << endl;
        return false;
      }
    }
  }

  //check entries in the config table, check that number of channels * sample time yields a whole number of nanoseconds and that the nanosecond increment is not too large for an int, and generate the ordered datastream indices array
  int numpulsarconfigs = 0;
  int numscrunchconfigs = 0;
  for(int i=0;i<numconfigs;i++)
  {
    //work out the ordereddatastreamindices
    count = 0;
    for(int j=0;j<datastreamtablelength;j++)
    {
      configs[i].ordereddatastreamindices[j] = -1;
      for(int k=0;k<numdatastreams;k++)
      {
        if(configs[i].datastreamindices[k] == j)
          configs[i].ordereddatastreamindices[j] = count++;
      }
    }
    if(count != numdatastreams)
    {
      cerror << startl << "Error - not all datastreams accounted for in the datastream table for config " << i << endl;
      return false;
    }

    //check that the subint time results in a whole number of FFTs for each datastream
    //also that the blockspersend is the same for all datastreams
    for(int j=0;j<numdatastreams;j++)
    {
      sampletimens = 1000.0/(2.0*freqtable[datastreamtable[configs[i].datastreamindices[j]].recordedfreqtableindices[0]].bandwidth);
      ffttime = sampletimens*freqtable[datastreamtable[configs[i].datastreamindices[j]].recordedfreqtableindices[0]].numchannels*2;
      numffts = configs[i].subintns/ffttime;
      if(ffttime - (int)(ffttime+0.5) > 0.00000001 || ffttime - (int)(ffttime+0.5) < -0.000000001)
      {
        cerror << startl << "Error - FFT chunk time for config " << i << ", datastream " << j << " is not a whole number of nanoseconds (" << ffttime << ") - aborting!!!" << endl;
        return false;
      }
      if(fabs(numffts - int(numffts+0.5)) > Mode::TINY) {
        cerror << startl << "Error - Send of size " << configs[i].subintns << " does not yield an integer number of FFTs for datastream " << j << " in config " << i << " - ABORTING" << endl;
        return false;
      }
      if(((double)configs[i].subintns)*(databufferfactor/numdatasegments) > ((1 << (sizeof(int)*8 - 1)) - 1))
      {
        cerror << startl << "Error - increment per read in nanoseconds is " << ((double)configs[i].subintns)*(databufferfactor/numdatasegments) << " - too large to fit in an int.  ABORTING" << endl;
        return false;
      }
      for (int k=1;k<getDNumRecordedFreqs(i,j);k++) {
        freqdata f = freqtable[datastreamtable[configs[i].datastreamindices[j]].recordedfreqtableindices[k]];
        if (fabs((1000.0*f.numchannels)/f.bandwidth) - ffttime > Mode::TINY)
          return false;
      }
    }

    //check that all baseline indices refer inside the table, and go in ascending order
    int b, lastt1 = 0, lastt2 = 0;
    for(int j=0;j<numbaselines;j++)
    {
      b = configs[i].baselineindices[j];
      if(b < 0 || b >= baselinetablelength) //bad index
      {
        cerror << startl << "Error - config " << i << " baseline index " << j << " refers to baseline " << b << " which is outside the range of the baseline table - aborting!!!" << endl;
        return false;
      }
      if(datastreamtable[baselinetable[b].datastream2index].telescopeindex < lastt2 && datastreamtable[baselinetable[b].datastream1index].telescopeindex <= lastt1)
      {
        cerror << startl << "Error - config " << i << " baseline index " << j << " refers to baseline " << datastreamtable[baselinetable[b].datastream2index].telescopeindex << "-" << datastreamtable[baselinetable[b].datastream1index].telescopeindex << " which is out of order with the previous baseline " << lastt1 << "-" << lastt2 << " - aborting!!!" << endl;
        return false;
      }
      lastt1 = datastreamtable[baselinetable[b].datastream1index].telescopeindex;
      lastt2 = datastreamtable[baselinetable[b].datastream2index].telescopeindex;
    }

    //check that if pulsar binning is turned on, that scrunch matches for all
    //if(configs[i].pulsarbin)
    //  numpulsarconfigs++;
    //if(configs[i].scrunchoutput)
    //  numscrunchconfigs++;
  }
  //if(numpulsarconfigs != numscrunchconfigs) {
  //  cerror << startl << "Error - there are " << numpulsarconfigs << " configurations with pulsar binning enabled, but only " << numscrunchconfigs << " have scrunching enabled.  If one is scrunching, all must - aborting!" << endl;
  //  return false;
  //}

  //check the baseline table entries
  for(int i=0;i<baselinetablelength;i++)
  {
    //check the datastream indices
    if(baselinetable[i].datastream1index < 0 || baselinetable[i].datastream2index < 0 || baselinetable[i].datastream1index >= datastreamtablelength || baselinetable[i].datastream2index >= datastreamtablelength)
    {
      cerror << startl << "Error - baseline table entry " << i << " has a datastream index outside the datastream table range! Its two indices are " << baselinetable[i].datastream1index << ", " << baselinetable[i].datastream2index << ".  ABORTING" << endl;
      return false;
    }

    //check the band indices
    for(int j=0;j<baselinetable[i].numfreqs;j++)
    {
      for(int k=0;k<baselinetable[i].numpolproducts[j];k++)
      {
        ds = datastreamtable[baselinetable[i].datastream1index];
        if((baselinetable[i].datastream1bandindex[j][k] < 0) || (baselinetable[i].datastream1bandindex[j][k] >= (ds.numrecordedbands + ds.numzoombands)))
        {
          cerror << startl << "Error! Baseline table entry " << i << ", frequency " << j << ", polarisation product " << k << " for datastream 1 refers to a band outside datastream 1's range (" << baselinetable[i].datastream1bandindex[j][k] << ") - aborting!!!" << endl;
          return false;
        }
        ds = datastreamtable[baselinetable[i].datastream2index];
        if((baselinetable[i].datastream2bandindex[j][k] < 0) || (baselinetable[i].datastream2bandindex[j][k] >= (ds.numrecordedbands + ds.numzoombands)))
        {
          cerror << startl << "Error! Baseline table entry " << i << ", frequency " << j << ", polarisation product " << k << " for datastream 2 refers to a band outside datastream 2's range (" << baselinetable[i].datastream2bandindex[j][k] << ") - aborting!!!" << endl;
          return false;
        }
      }
    }
  }

  if(databufferfactor % numdatasegments != 0)
  {
    cerror << startl << "Error - there must be an integer number of sends per datasegment.  Presently databufferfactor is " << databufferfactor << ", and numdatasegments is " << numdatasegments << ".  ABORTING" << endl;
    return false;
  }

  return true;
}

bool Configuration::processPulsarConfig(string filename, int configindex)
{
  int numpolycofiles, ncoefficients, polycocount;
  string line;
  string * polycofilenames;
  double * binphaseends;
  double * binweights;
  int * numsubpolycos;
  char psrline[128];
  ifstream temppsrinput;

  cinfo << startl << "About to process pulsar file " << filename << endl;
  ifstream pulsarinput(filename.c_str(), ios::in);
  if(!pulsarinput.is_open() || pulsarinput.bad())
  {
    cfatal << startl << "Error - could not open pulsar config file " << line << " - aborting!!!" << endl;
    return false;
  }
  getinputline(&pulsarinput, &line, "NUM POLYCO FILES");
  numpolycofiles = atoi(line.c_str());
  polycofilenames = new string[numpolycofiles];
  numsubpolycos = new int[numpolycofiles];
  configs[configindex].numpolycos = 0;
  for(int i=0;i<numpolycofiles;i++)
  {
    getinputline(&pulsarinput, &(polycofilenames[i]), "POLYCO FILE");
    numsubpolycos[i] = 0;
    temppsrinput.open(polycofilenames[i].c_str());
    temppsrinput.getline(psrline, 128);
    temppsrinput.getline(psrline, 128);
    while(!(temppsrinput.eof() || temppsrinput.fail())) {
      psrline[54] = '\0';
      ncoefficients = atoi(&(psrline[49]));
      for(int j=0;j<ncoefficients/3 + 2;j++)
        temppsrinput.getline(psrline, 128);
      numsubpolycos[i]++;
      configs[configindex].numpolycos++;
    }
    temppsrinput.close();
  }
  getinputline(&pulsarinput, &line, "NUM PULSAR BINS");
  configs[configindex].numbins = atoi(line.c_str());
  if(configs[configindex].numbins > maxnumpulsarbins)
    maxnumpulsarbins = configs[configindex].numbins;
  binphaseends = new double[configs[configindex].numbins];
  binweights = new double[configs[configindex].numbins];
  getinputline(&pulsarinput, &line, "SCRUNCH OUTPUT");
  configs[configindex].scrunchoutput = ((line == "TRUE") || (line == "T") || (line == "true") || (line == "t"))?true:false;
  for(int i=0;i<configs[configindex].numbins;i++)
  {
    getinputline(&pulsarinput, &line, "BIN PHASE END");
    binphaseends[i] = atof(line.c_str());
    getinputline(&pulsarinput, &line, "BIN WEIGHT");
    binweights[i] = atof(line.c_str());
  }

  //create the polycos
  configs[configindex].polycos = new Polyco*[configs[configindex].numpolycos];
  polycocount = 0;
  for(int i=0;i<numpolycofiles;i++)
  {
    for(int j=0;j<numsubpolycos[i];j++)
    {
      cinfo << startl << "About to create polyco file " << polycocount << " from filename " << polycofilenames[i] << ", subcount " << j << endl;
      configs[configindex].polycos[polycocount] = new Polyco(polycofilenames[i], j, configindex, configs[configindex].numbins, getMaxNumChannels(), binphaseends, binweights, double(configs[configindex].subintns)/60000000000.0);
      if (!configs[configindex].polycos[polycocount]->initialisedOK())
        return false;
      polycocount++;
    } 
  }
  
  delete [] binphaseends;
  delete [] binweights;
  delete [] polycofilenames;
  delete [] numsubpolycos;
  pulsarinput.close();
  return true;
}

bool Configuration::setPolycoFreqInfo(int configindex)
{
  bool ok = true;
  datastreamdata d = datastreamtable[getMaxNumFreqDatastreamIndex(configindex)];
  double * frequencies = new double[freqtablelength];
  double * bandwidths = new double[freqtablelength];
  int * numchannels = new int[freqtablelength];
  bool * used = new bool[freqtablelength];
  //double bandwidth = freqtable[d.recordedfreqtableindices[0]].bandwidth;
  for(int i=0;i<freqtablelength;i++)
  {
    frequencies[i] = freqtable[i].bandedgefreq;
    if(freqtable[i].lowersideband)
      frequencies[i] -= freqtable[i].bandwidth;
    bandwidths[i] = freqtable[i].bandwidth;
    numchannels[i] = freqtable[i].numchannels;
    used[i] = configs[configindex].frequsedbybaseline[i];
  }
  for(int i=0;i<configs[configindex].numpolycos;i++)
  {
    ok = ok && configs[configindex].polycos[i]->setFrequencyValues(freqtablelength, frequencies, bandwidths, numchannels, used);
  }
  delete [] frequencies;
  delete [] bandwidths;
  delete [] numchannels;
  delete [] used;
  return ok;
}

void Configuration::makeFortranString(string line, int length, char * destination)
{
  int linelength = line.length();
  
  if(linelength <= length)
  {
    strcpy(destination, line.c_str());
    for(int i=0;i<length-linelength;i++)
      destination[i+linelength] = ' ';
  }
  else
  {
    strcpy(destination, (line.substr(0, length-1)).c_str());
    destination[length-1] = line.at(length-1);
  }
}

void Configuration::getinputline(ifstream * input, std::string * line, std::string startofheader)
{
  if(input->eof())
    cerror << startl << "Error - trying to read past the end of file!!!" << endl;
  input->get(header, HEADER_LENGTH);
  if(strncmp(header, startofheader.c_str(), startofheader.length()) != 0) //not what we expected
    cerror << startl << "Error - we thought we were reading something starting with '" << startofheader << "', when we actually got '" << header << "'" << endl;
  getline(*input,*line);
}

void Configuration::getinputline(ifstream * input, std::string * line, std::string startofheader, int intval)
{
  char buffer[HEADER_LENGTH+1];
  sprintf(buffer, "%s%i", startofheader.c_str(), intval);
  getinputline(input, line, string(buffer));
}

void Configuration::getMJD(int & d, int & s, int year, int month, int day, int hour, int minute, int second)
{
  d = year*367 - int(7*(year + int((month + 9)/12))/4) + int(275*month/9) + day - 678987;

  s = 3600*hour + 60*minute + second;
}

void Configuration::mjd2ymd(int mjd, int & year, int & month, int & day)
{
  int j = mjd + 32044 + 2400001;
  int g = j / 146097;
  int dg = j % 146097;
  int c = ((dg/36524 + 1)*3)/4;
  int dc = dg - c*36524;
  int b = dc / 1461;
  int db = dc % 1461;
  int a = ((db/365 + 1)*3)/4;
  int da = db - a*365;
  int y = g*400 + c*100 + b*4 + a;
  int m = (da*5 + 308)/153 - 2;
  int d = da - ((m + 4)*153)/5 + 122;
  
  year = y - 4800 + (m + 2)/12;
  month = (m + 2)%12 + 1;
  day = d + 1;
}

