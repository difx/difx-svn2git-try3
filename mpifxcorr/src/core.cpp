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
#include "core.h"
#include "fxmanager.h"
#include "alert.h"

Core::Core(int id, Configuration * conf, int * dids, MPI_Comm rcomm)
  : mpiid(id), config(conf), return_comm(rcomm)
{
  int status;
  double guardratio, maxguardratio;
  estimatedbytes = config->getEstimatedBytes();

  //Get all the correlation parameters from config
  model = config->getModel();
  numdatastreams = config->getNumDataStreams();
  numbaselines = config->getNumBaselines();
  maxpreavresultlength = config->getMaxResultLength();
  maxpostavresultlength = config->getMaxPostavResultLength();
  numprocessthreads = config->getCNumProcessThreads(mpiid - numdatastreams - fxcorr::FIRSTTELESCOPEID);
  currentconfigindex = 0;
  startmjd = config->getStartMJD();
  startseconds = config->getStartSeconds();

  //work out the biggest overhead from any of the active configurations
  maxguardratio = 1.0;
  databytes = config->getMaxDataBytes();
  for(int i=0;i<config->getNumConfigs();i++)
  {
    guardratio = double(config->getSubintNS(i) + config->getGuardNS(i))/double(config->getSubintNS(i));
    if(guardratio > maxguardratio)
    {
      databytes = int((((long long)config->getMaxDataBytes())*((long long)(config->getSubintNS(i)+config->getGuardNS(i))))/config->getSubintNS(i));
      maxguardratio = guardratio;
    }
  }

  //if we have a mark5 datastream, make this number a bit bigger to be safe
  int overheadbytes = 0;
  for(int i=0;i<numdatastreams;i++)
  {
    if(config->isMkV(i) || config->isNativeMkV(i))
      overheadbytes = config->getFrameBytes(0, i);
  }
  databytes += overheadbytes;

  //allocate the send/receive circular buffer (length RECEIVE_RING_LENGTH)
  controllength = config->getMaxBlocksPerSend() + 4;
  procslots = new processslot[RECEIVE_RING_LENGTH];
  for(int i=0;i<RECEIVE_RING_LENGTH;i++)
  {
    procslots[i].results = vectorAlloc_cf32(maxpostavresultlength);
    //set up the info for this slot, using the first configuration
    status = vectorZero_cf32(procslots[i].results, maxpostavresultlength);
    if(status != vecNoErr)
      csevere << startl << "Error trying to zero results in core " << mpiid << ", processing slot " << i << endl;
    procslots[i].resultsvalid = CR_VALIDVIS;
    procslots[i].configindex = currentconfigindex;
    procslots[i].preavresultlength = config->getResultLength(currentconfigindex);
    procslots[i].postavresultlength = config->getPostavResultLength(currentconfigindex);
    procslots[i].slotlocks = new pthread_mutex_t[numprocessthreads];
    procslots[i].copylocks = new pthread_mutex_t[numbaselines+1];
    for(int j=0;j<numprocessthreads;j++)
      pthread_mutex_init(&(procslots[i].slotlocks[j]), NULL);
    for(int j=0;j<numbaselines+1;j++)
      pthread_mutex_init(&(procslots[i].copylocks[j]), NULL);
    procslots[i].datalengthbytes = new int[numdatastreams];
    procslots[i].databuffer = new u8*[numdatastreams];
    procslots[i].controlbuffer = new s32*[numdatastreams];
    procslots[i].keepprocessing = true;
    procslots[i].numpulsarbins = config->getNumPulsarBins(currentconfigindex);
    procslots[i].scrunchoutput = config->scrunchOutputOn(currentconfigindex);
    procslots[i].pulsarbin = config->pulsarBinOn(currentconfigindex);
    for(int j=0;j<numdatastreams;j++)
    {
      procslots[i].databuffer[j] = vectorAlloc_u8(databytes);
      procslots[i].controlbuffer[j] = vectorAlloc_s32(controllength);
      estimatedbytes += databytes;
      estimatedbytes += controllength;
    }
  }

  //initialise the threads that do the actual processing
  processthreads = new pthread_t[numprocessthreads];
  processconds = new pthread_cond_t[numprocessthreads];
  processthreadinitialised = new bool[numprocessthreads];
  threadbytes = new int[numprocessthreads];
  for(int i=0;i<numprocessthreads;i++)
  {
    pthread_cond_init(&processconds[i], NULL);
    processthreadinitialised[i] = false;
    threadbytes[i] = 8*maxpreavresultlength;
  }

  //initialise the MPI communication objects
  datarequests = new MPI_Request[numdatastreams];
  controlrequests = new MPI_Request[numdatastreams];
  msgstatuses = new MPI_Status[numdatastreams];

  //copy the datastream ids
  datastreamids = new int[numdatastreams];
  for(int i=0;i<numdatastreams;i++)
    datastreamids[i] = dids[i];

  //initialise the binary message infrastructure
  difxMessageInitBinary();
}


Core::~Core()
{
  for(int i=0;i<RECEIVE_RING_LENGTH;i++)
  {
    for(int j=0;j<numdatastreams;j++)
    {
      vectorFree(procslots[i].databuffer[j]);
      vectorFree(procslots[i].controlbuffer[j]);
    }
    delete [] procslots[i].slotlocks;
    delete [] procslots[i].datalengthbytes;
    delete [] procslots[i].databuffer;
    delete [] procslots[i].controlbuffer;
    vectorFree(procslots[i].results);
  }
  delete [] threadbytes;
  delete [] processthreads;
  delete [] processconds;
  delete [] processthreadinitialised;
  delete [] procslots;
  delete [] datarequests;
  delete [] controlrequests;
  delete [] msgstatuses;
  delete [] datastreamids;
}


void Core::execute()
{
  int perr, status, lastconfigindex;
  bool terminate;
  processthreadinfo * threadinfos = new processthreadinfo[numprocessthreads];
  
  terminate = false;
  numreceived = 0;
  cverbose << startl << "Core " << mpiid << " has started executing!!! Numprocessthreads is " << numprocessthreads << endl;

  //get the lock for the first slot, one per thread
  for(int i=0;i<numprocessthreads;i++)
  {
    perr = pthread_mutex_lock(&(procslots[numreceived].slotlocks[i]));
    if(perr != 0)
      csevere << startl << "Error in Core " << mpiid << " attempt to lock mutex" << numreceived << " of thread " << i << endl;
  }

  //cverbose << startl << "Core about to fill up receive ring buffer" << endl;
  //start off by filling up the data and control buffers for all slots
  for(int i=0;i<RECEIVE_RING_LENGTH-1;i++)
    receivedata(numreceived++, &terminate);
  //cverbose << startl << "Core has filled up receive ring buffer" << endl;

  //now we have the lock on the last slot in the ring.  Launch processthreads
  for(int i=0;i<numprocessthreads;i++)
  {
    threadinfos[i].thiscore = this;
    threadinfos[i].processthreadid = i;
    perr = pthread_create(&processthreads[i], NULL, Core::launchNewProcessThread, (void *)(&threadinfos[i]));
    if(perr != 0)
      csevere << startl << "Error in launching Core " << mpiid << " processthread " << i << "!!!" << endl;
    while(!processthreadinitialised[i])
    {
      perr = pthread_cond_wait(&processconds[i], &(procslots[numreceived].slotlocks[i]));
      if (perr != 0)
        csevere << startl << "Error waiting on receivethreadinitialised condition!!!!" << endl;
    }
  }

  cverbose << startl << "Estimated memory usage by Core is now " << getEstimatedBytes()/(1024.0*1024.0) << " MB" << endl;
  lastconfigindex = procslots[0].configindex;
  while(!terminate) //the data is valid, so keep processing
  {
    //increment and receive some more data
    receivedata(numreceived++ % RECEIVE_RING_LENGTH, &terminate);

    //send the results back
    MPI_Ssend(procslots[numreceived%RECEIVE_RING_LENGTH].results, procslots[numreceived%RECEIVE_RING_LENGTH].postavresultlength*2, MPI_FLOAT, fxcorr::MANAGERID, procslots[numreceived%RECEIVE_RING_LENGTH].resultsvalid, return_comm);
    if(procslots[numreceived%RECEIVE_RING_LENGTH].configindex != lastconfigindex)
    {
      cverbose << startl << "After config change, estimated memory usage by Core is " << getEstimatedBytes()/(1024.0*1024.0) << " MB" << endl;
    }

    //zero the results buffer for this slot and set the status back to valid
    status = vectorZero_cf32(procslots[numreceived%RECEIVE_RING_LENGTH].results, procslots[numreceived%RECEIVE_RING_LENGTH].postavresultlength);
    if(status != vecNoErr)
      csevere << startl << "Error trying to zero results in Core!!!" << endl;
    procslots[numreceived%RECEIVE_RING_LENGTH].resultsvalid = CR_VALIDVIS;
  }

  //Run through the shutdown sequence
//  cinfo << startl << "Core " << mpiid << " commencing termination sequence" << endl;
  for(int i=0;i<numprocessthreads;i++)
  {
    //Unlock the mutex we are currently holding for this thread
    perr = pthread_mutex_unlock(&(procslots[(numreceived+RECEIVE_RING_LENGTH-1) % RECEIVE_RING_LENGTH].slotlocks[i]));
    if(perr != 0)
      csevere << startl << "Error in Core " << mpiid << " attempt to unlock mutex" << (numreceived+RECEIVE_RING_LENGTH-1) % RECEIVE_RING_LENGTH << " of thread " << i << endl;
  }

  //ensure all the results we have sitting around have been sent
  for(int i=1;i<RECEIVE_RING_LENGTH-1;i++)
  {
//    cinfo << startl << "Core " << mpiid << " about to send final values from section " << i << endl;
    for(int j=0;j<numprocessthreads;j++)
    {
      //Lock and unlock first to ensure the threads have finished working on this slot
      perr = pthread_mutex_lock(&(procslots[(numreceived+i) % RECEIVE_RING_LENGTH].slotlocks[j]));
      if(perr != 0)
        csevere << startl << "Error in Core " << mpiid << " attempt to unlock mutex" << (numreceived+i) % RECEIVE_RING_LENGTH << " of thread " << j << endl;
      perr = pthread_mutex_unlock(&(procslots[(numreceived+i) % RECEIVE_RING_LENGTH].slotlocks[j]));
      if(perr != 0)
        csevere << startl << "Error in Core " << mpiid << " attempt to unlock mutex" << (numreceived+i) % RECEIVE_RING_LENGTH << " of thread " << j << endl;
    }
    //send the results
    MPI_Ssend(procslots[(numreceived+i)%RECEIVE_RING_LENGTH].results, procslots[(numreceived+i)%RECEIVE_RING_LENGTH].postavresultlength*2, MPI_FLOAT, fxcorr::MANAGERID, procslots[numreceived%RECEIVE_RING_LENGTH].resultsvalid, return_comm);
  }

//  cinfo << startl << "CORE " << mpiid << " is about to join the processthreads" << endl;

  //join the process threads, they have to already be finished anyway
  for(int i=0;i<numprocessthreads;i++)
  {
    perr = pthread_join(processthreads[i], NULL);
    if(perr != 0)
      csevere << startl << "Error in Core " << mpiid << " attempt to join processthread " << i << endl;
  }
  delete [] threadinfos;

//  cinfo << startl << "CORE " << mpiid << " terminating" << endl;
}

void * Core::launchNewProcessThread(void * tdata)
{
  processthreadinfo * mydata = (processthreadinfo *)tdata;
  (mydata->thiscore)->loopprocess(mydata->processthreadid);

  return 0;
}

int Core::getEstimatedBytes()
{
  int toreturn = estimatedbytes;
  for(int i=0;i<numprocessthreads;i++)
    toreturn += threadbytes[i];
  return toreturn;
}

void Core::loopprocess(int threadid)
{
  int perr, numprocessed, startblock, numblocks, lastconfigindex, numpolycos, maxbins, maxchan, maxpolycos, stadumpchannels;
  double sec;
  bool pulsarbin, somepulsarbin, somescrunch, dumpingsta, nowdumpingsta;
  processslot * currentslot;
  Polyco ** polycos=0;
  Polyco * currentpolyco=0;
  Mode ** modes;
  threadscratchspace * scratchspace = new threadscratchspace;
  scratchspace->threadresults = vectorAlloc_cf32(maxpreavresultlength);
  scratchspace->dsweights = new f32[numdatastreams];
  if(scratchspace->threadresults == NULL) {
    cfatal << startl << "Could not allocate thread results!!! Aborting." << endl;
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  scratchspace->pulsarscratchspace=0;
  scratchspace->pulsaraccumspace=0;
  scratchspace->starecord = 0;

  pulsarbin = false;
  somepulsarbin = false;
  somescrunch = false;
  dumpingsta = false;
  maxpolycos = 0;
  maxchan = config->getMaxNumChannels();
  maxbins = config->getMaxNumPulsarBins();
  scratchspace->rotator = vectorAlloc_cf32(maxchan);
  scratchspace->rotated = vectorAlloc_cf32(maxchan);
  scratchspace->channelsums = vectorAlloc_cf32(maxchan);
  scratchspace->argument = vectorAlloc_f32(3*maxchan);

  //work out whether we'll need to do any pulsar binning, and work out the maximum # channels (and # polycos if applicable)
  for(int i=0;i<config->getNumConfigs();i++)
  {
    if(config->pulsarBinOn(i))
    {
      //pulsarbin = true;
      somepulsarbin = true;
      somescrunch = somescrunch || config->scrunchOutputOn(i);
      numpolycos = config->getNumPolycos(i);
      if(numpolycos > maxpolycos)
        maxpolycos = numpolycos;
    }
  }

  //create the necessary pulsar scratch space if required
  if(somepulsarbin)
  {
    scratchspace->pulsarscratchspace = vectorAlloc_cf32(maxchan+1);
    if(somescrunch) //need separate accumulation space
    {
      scratchspace->pulsaraccumspace = new cf32*****[numbaselines];
      createPulsarAccumSpace(scratchspace->pulsaraccumspace, procslots[0].configindex, -1, threadid); //don't need to delete old space
    }
    scratchspace->bins = new s32*[config->getFreqTableLength()];
    for(int i=0;i<config->getFreqTableLength();i++)
    {
      scratchspace->bins[i] = vectorAlloc_s32(config->getFNumChannels(i)+1);
    }
  }

  //set to first configuration and set up, creating Modes, Polycos etc
  lastconfigindex = procslots[0].configindex;
  modes = new Mode*[numdatastreams];
  if(somepulsarbin)
    polycos = new Polyco*[maxpolycos];
  updateconfig(lastconfigindex, lastconfigindex, threadid, startblock, numblocks, numpolycos, pulsarbin, modes, polycos, true);
  numprocessed = 0;
//  cinfo << startl << "Core thread id " << threadid << " will be processing from block " << startblock << ", length " << numblocks << endl;

  //lock the end section
  perr = pthread_mutex_lock(&(procslots[RECEIVE_RING_LENGTH-1].slotlocks[threadid]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying lock mutex " << RECEIVE_RING_LENGTH-1 << endl;

  //grab the lock we really want, unlock the end section and signal the main thread we're ready to go
  perr = pthread_mutex_lock(&(procslots[0].slotlocks[threadid]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying lock mutex 0" << endl; 
  perr = pthread_mutex_unlock(&(procslots[RECEIVE_RING_LENGTH-1].slotlocks[threadid]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying unlock mutex " << RECEIVE_RING_LENGTH-1 << endl;
  processthreadinitialised[threadid] = true;
  perr = pthread_cond_signal(&processconds[threadid]);
  if(perr != 0)
    csevere << startl << "Core processthread " << mpiid << "/" << threadid << " error trying to signal main thread to wake up!!!" << endl;
  if(threadid == 0)
    cinfo << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " is about to start processing" << endl;

  //while valid, process data
  while(procslots[(numprocessed)%RECEIVE_RING_LENGTH].keepprocessing)
  {
    currentslot = &(procslots[numprocessed%RECEIVE_RING_LENGTH]);
    if(pulsarbin)
    {
      sec = double(startseconds + model->getScanStartSec(currentslot->offsets[0], startmjd, startseconds) + currentslot->offsets[1]) + ((double)currentslot->offsets[2])/1000000000.0;
      //get the correct Polyco for this time range and set it up correctly
      currentpolyco = Polyco::getCurrentPolyco(currentslot->configindex, startmjd, sec/86400.0, polycos, numpolycos, false);
      if(currentpolyco == NULL)
      {
        cfatal << startl << "Could not locate a polyco to cover time " << startmjd + sec/86400.0<< " - aborting!!!" << endl;
        currentpolyco = Polyco::getCurrentPolyco(currentslot->configindex, startmjd, sec/86400.0, polycos, numpolycos, true);
	MPI_Abort(MPI_COMM_WORLD, 1);
      }
      currentpolyco->setTime(startmjd, sec/86400.0);
    }

    //if necessary, allocate/reallocate space for the STAs
    nowdumpingsta = config->dumpSTA();
    if(nowdumpingsta != dumpingsta) {
      if (scratchspace->starecord != 0) {
        delete scratchspace->starecord;
        scratchspace->starecord = 0;
      }
      if(nowdumpingsta) {
        stadumpchannels = config->getSTADumpChannels();
        scratchspace->starecord = (DifxMessageSTARecord*)malloc(sizeof(DifxMessageSTARecord) + sizeof(f32)*stadumpchannels);
        scratchspace->starecord->threadId = threadid;
        scratchspace->starecord->nThreads = numprocessthreads;
        scratchspace->starecord->nChan = stadumpchannels;
      }
      dumpingsta = nowdumpingsta;
    }

    //process our section of responsibility for this time range
    processdata(numprocessed++ % RECEIVE_RING_LENGTH, threadid, startblock, numblocks, modes, currentpolyco, scratchspace);

    currentslot = &(procslots[numprocessed%RECEIVE_RING_LENGTH]);
    //if the configuration changes from this segment to the next, change our setup accordingly
    if(currentslot->configindex != lastconfigindex)
    {
      cinfo << startl << "Core " << mpiid << " threadid " << threadid << ": changing config to " << currentslot->configindex << endl;
      updateconfig(lastconfigindex, currentslot->configindex, threadid, startblock, numblocks, numpolycos, pulsarbin, modes, polycos, false);
      cinfo << startl << "Core " << mpiid << " threadid " << threadid << ": config changed successfully - pulsarbin is now " << pulsarbin << endl;
      createPulsarAccumSpace(scratchspace->pulsaraccumspace, currentslot->configindex, lastconfigindex, threadid);
      lastconfigindex = currentslot->configindex;
    }
  }

  //fallen out of loop, so must be finished.  Unlock held mutex
//  cinfo << startl << "PROCESS " << mpiid << "/" << threadid << " process thread about to free resources and exit" << endl;
  perr = pthread_mutex_unlock(&(procslots[numprocessed % RECEIVE_RING_LENGTH].slotlocks[threadid]));
  if (perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying unlock mutex " << (numprocessed)%RECEIVE_RING_LENGTH << endl;

  //free resources
  for(int j=0;j<numdatastreams;j++)
    delete modes[j];
  delete [] modes;
  if(somepulsarbin)
  {
    if(threadid > 0 && pulsarbin)
    {
      for(int i=0;i<numpolycos;i++)
        delete polycos[i];
    }
    if(somepulsarbin)
    {
      for(int i=0;i<config->getFreqTableLength();i++)
      {
        //if(config->getFWiderBandIndex(i) < 0)
        vectorFree(scratchspace->bins[i]);
      }
      delete [] scratchspace->bins;
    }
    delete [] polycos;
    vectorFree(scratchspace->pulsarscratchspace);
    if(somescrunch)
    {
      createPulsarAccumSpace(scratchspace->pulsaraccumspace, -1, procslots[(numprocessed+1)%RECEIVE_RING_LENGTH].configindex, threadid);
      delete [] scratchspace->pulsaraccumspace;
    }
  }
  delete [] scratchspace->dsweights;
  vectorFree(scratchspace->threadresults);
  vectorFree(scratchspace->rotator);
  vectorFree(scratchspace->rotated);
  vectorFree(scratchspace->channelsums);
  vectorFree(scratchspace->argument);
  if(scratchspace->starecord != 0) {
    delete scratchspace->starecord;
  }
  delete scratchspace;

  cinfo << startl << "PROCESS " << mpiid << "/" << threadid << " process thread exiting!!!" << endl;
}

void Core::receivedata(int index, bool * terminate)
{
  MPI_Status mpistatus;
  int perr;

  if(*terminate)
    return; //don't try to read, we've already finished

  //Get the instructions on the time offset from the FxManager node
  MPI_Recv(&(procslots[index].offsets), 3, MPI_INT, fxcorr::MANAGERID, MPI_ANY_TAG, return_comm, &mpistatus);
  if(mpistatus.MPI_TAG == CR_TERMINATE)
  {
    *terminate = true;
//    cinfo << startl << "Core " << mpiid << " has received a terminate signal!!!" << endl;
    procslots[index].keepprocessing = false;
    return; //note return here!!!
  }

  //work out if the source has changed, and if so, whether we need to change the modes and baselines
  currentconfigindex = config->getScanConfigIndex(procslots[index].offsets[0]);
  if(procslots[index].configindex != currentconfigindex)
  {
    cinfo << startl << "Config has changed! Old config: " << procslots[index].configindex << ", new config " << currentconfigindex << endl;
    procslots[index].configindex = currentconfigindex;
    if(procslots[index].configindex < 0)
    {
      cfatal << startl << "Core received a request to process data from scan " << procslots[index].offsets[0] << " which does not have a config - aborting!!!" << endl;
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    procslots[index].preavresultlength = config->getResultLength(currentconfigindex);
    procslots[index].postavresultlength = config->getPostavResultLength(currentconfigindex);
    procslots[index].numpulsarbins = config->getNumPulsarBins(currentconfigindex);
    procslots[index].scrunchoutput = config->scrunchOutputOn(currentconfigindex);
    procslots[index].pulsarbin = config->pulsarBinOn(currentconfigindex);
  }

  //now grab the data and delay info from the individual datastreams
  for(int i=0;i<numdatastreams;i++)
  {
    //cinfo << startl << "Core is about to post receive request for datastream " << i << endl;
    //get data
    MPI_Irecv(procslots[index].databuffer[i], databytes, MPI_UNSIGNED_CHAR, datastreamids[i], CR_PROCESSDATA, MPI_COMM_WORLD, &datarequests[i]);
    //also receive the offsets and rates
    MPI_Irecv(procslots[index].controlbuffer[i], controllength, MPI_INT, datastreamids[i], CR_PROCESSCONTROL, MPI_COMM_WORLD, &controlrequests[i]);
  }

  //wait for everything to arrive, store the length of the messages
  MPI_Waitall(numdatastreams, datarequests, msgstatuses);
  for(int i=0;i<numdatastreams;i++)
    MPI_Get_count(&(msgstatuses[i]), MPI_UNSIGNED_CHAR, &(procslots[index].datalengthbytes[i]));
  MPI_Waitall(numdatastreams, controlrequests, msgstatuses);

  //lock the next slot, unlock the one we just finished with
  for(int i=0;i<numprocessthreads;i++)
  {
    perr = pthread_mutex_lock(&(procslots[(index+1)%RECEIVE_RING_LENGTH].slotlocks[i]));
    if(perr != 0)
      csevere << startl << "CORE " << mpiid << " error trying lock mutex " << (index+1)%RECEIVE_RING_LENGTH << endl;

    perr = pthread_mutex_unlock(&(procslots[index].slotlocks[i]));
    if(perr != 0)
      csevere << startl << "CORE " << mpiid << " error trying unlock mutex " << index << endl;
  }
}

void Core::processdata(int index, int threadid, int startblock, int numblocks, Mode ** modes, Polyco * currentpolyco, threadscratchspace * scratchspace)
{
  int status, perr;
  int resultindex, copyindex, cindex, ds1index, ds2index, maxproducts, binloop, blockcount, maxblocks, shiftcount;
  int freqchannels, freqindex, channelinc, copylength, copiedchans, safelength, currentlength;
  int nyquistchannel, nyquistoffset;
  double offsetmins, blockns;
  f32 bweight;
  Mode * m1, * m2;
  cf32 * vis1;
  cf32 * vis2;
  f32 * acdata;
  cf32 tempsum;
  cf32 * sumpointer;
  bool datastreamsaveraged;
  bool writecrossautocorrs;

  writecrossautocorrs = modes[0]->writeCrossAutoCorrs();
  maxproducts = config->getMaxProducts();

  //set up the mode objects that will do the station-based processing
  for(int j=0;j<numdatastreams;j++)
  {
    //zero the autocorrelations and set delays
    modes[j]->zeroAutocorrelations();
    modes[j]->setValidFlags(&(procslots[index].controlbuffer[j][3]));
    modes[j]->setData(procslots[index].databuffer[j], procslots[index].datalengthbytes[j], procslots[index].controlbuffer[j][0], procslots[index].controlbuffer[j][1], procslots[index].controlbuffer[j][2]);
    modes[j]->setOffsets(procslots[index].offsets[0], procslots[index].offsets[1], procslots[index].offsets[2]);
  }
  //zero the results for this slot, for this thread
  status = vectorZero_cf32(scratchspace->threadresults, procslots[index].preavresultlength);
  if(status != vecNoErr)
    csevere << startl << "Error trying to zero threadresults!!!" << endl;

  //process each FFT chunk in turn
  blockcount = 0;
  shiftcount = 0;
  blockns = ((double)(config->getSubintNS(procslots[index].configindex)))/((double)(config->getBlocksPerSend(procslots[index].configindex)));
  maxblocks = ((int)(model->getMaxNSBetweenUVShifts(procslots[index].offsets[0])/blockns));
  for(int i=startblock;i<startblock+numblocks;i++)
  {
    resultindex = 0;

    //do the station-based processing for this FFT chunk
    for(int j=0;j<numdatastreams;j++)
    {
      scratchspace->dsweights[j] = modes[j]->process(i);
    }

    //if necessary, work out the pulsar bins
    if(procslots[index].pulsarbin)
    {
      offsetmins = ((double)i)*((double)config->getSubintNS(procslots[index].configindex))/(60000000000.0);
      currentpolyco->getBins(offsetmins, scratchspace->bins);
    }

    //do the cross multiplication - gets messy for the pulsar binning
    for(int j=0;j<numbaselines;j++)
    {
      //get the two modes that contribute to this baseline
      ds1index = config->getBOrderedDataStream1Index(procslots[index].configindex, j);
      ds2index = config->getBOrderedDataStream2Index(procslots[index].configindex, j);
      m1 = modes[ds1index];
      m2 = modes[ds2index];

      //add the desired results into the resultsbuffer, for each phasecentre/freq/pulsar bin/polarisation pair
      for(int k=0;k<config->getBNumFreqs(procslots[index].configindex, j);k++)
      {
        freqchannels = config->getFNumChannels(config->getBFreqIndex(procslots[index].configindex, j, k));
        //the Nyquist channel referred to here is for the *first* datastream of the baseline, in the event
        //that one datastream has USB and the other has LSB
        nyquistchannel = freqchannels;
        if(config->getFreqTableLowerSideband(config->getBFreqIndex(procslots[index].configindex, j, k))) {
          nyquistchannel = 0;
        }

        //loop through each polarisation for this frequency
        for(int p=0;p<config->getBNumPolProducts(procslots[index].configindex,j,k);p++)
        {
          //get the appropriate arrays to multiply
          vis1 = m1->getFreqs(config->getBDataStream1BandIndex(procslots[index].configindex, j, k, p));
          vis2 = m2->getConjugatedFreqs(config->getBDataStream2BandIndex(procslots[index].configindex, j, k, p));

          if(procslots[index].pulsarbin)
          {
            //multiply into scratch space
            status = vectorMul_cf32(vis1,vis2, scratchspace->pulsarscratchspace, freqchannels+1);
            if(status != vecNoErr)
              csevere << startl << "Error trying to xmac baseline " << j << " frequency " << k << " polarisation product " << p << ", status " << status << endl;

            //if scrunching, add into temp accumulate space, otherwise add into normal space
            if(procslots[index].scrunchoutput)
            {
              bweight = scratchspace->dsweights[ds1index]*scratchspace->dsweights[ds2index];
              for(int l=1-nyquistchannel/freqchannels;l<freqchannels+1-nyquistchannel/freqchannels;l++)
              {
                //the first zero (the source slot) is because we are limiting to one pulsar ephemeris for now
                scratchspace->pulsaraccumspace[j][k][0][p][scratchspace->bins[config->getBFreqIndex(procslots[index].configindex, j, k)][l]][l].re += scratchspace->pulsarscratchspace[l].re;
                scratchspace->pulsaraccumspace[j][k][0][p][scratchspace->bins[config->getBFreqIndex(procslots[index].configindex, j, k)][l]][l].im += scratchspace->pulsarscratchspace[l].im;
              }
              scratchspace->pulsaraccumspace[j][k][0][p][0][nyquistchannel].im += bweight;
            }
            else
            {
              bweight = scratchspace->dsweights[ds1index]*scratchspace->dsweights[ds2index]/(freqchannels+1);
              for(int l=1-nyquistchannel/freqchannels;l<freqchannels+1-nyquistchannel/freqchannels;l++)
              {
                cindex = resultindex + (scratchspace->bins[config->getBFreqIndex(procslots[index].configindex, j, k)][l]*config->getBNumPolProducts(procslots[index].configindex,j,k) + p)*(freqchannels+1) + l;
                scratchspace->threadresults[cindex].re += scratchspace->pulsarscratchspace[l].re;
                scratchspace->threadresults[cindex].im += scratchspace->pulsarscratchspace[l].im;
                scratchspace->threadresults[cindex-l+nyquistchannel].im += bweight;
              }
            }
          }
          else
          {
            //not pulsar binning, so this is nice and simple - just cross multiply accumulate
            status = vectorAddProduct_cf32(&(vis1[1-nyquistchannel/freqchannels]), &(vis2[1-nyquistchannel/freqchannels]), &(scratchspace->threadresults[resultindex+1-nyquistchannel/freqchannels]), freqchannels);
            if(status != vecNoErr)
              csevere << startl << "Error trying to xmac baseline " << j << " frequency " << k << " polarisation product " << p << ", status " << status << endl;
            scratchspace->threadresults[resultindex+nyquistchannel].im += scratchspace->dsweights[ds1index]*scratchspace->dsweights[ds2index];
            resultindex += freqchannels+1;
          }
          if(procslots[index].pulsarbin && !procslots[index].scrunchoutput)
            //finished all the products of this frequency, so add the requisite increment to resultindex
            resultindex += config->getBNumPolProducts(procslots[index].configindex,j,k)*procslots[index].numpulsarbins*(freqchannels+1);
        }
      }
    }
    blockcount++;
    if(blockcount == maxblocks)
    {
      //shift/average and then lock results and copy data
      uvshiftAndAverage(index, threadid, (startblock+shiftcount*maxblocks+((double)maxblocks)/2.0)*blockns, currentpolyco, scratchspace);

      //reset the blockcount, increment shiftcount
      blockcount = 0;
      shiftcount++;
    }
  }

  if(blockcount != 0)
    uvshiftAndAverage(index, threadid, (startblock+shiftcount*maxblocks+((double)blockcount)/2.0)*blockns, currentpolyco, scratchspace);

  datastreamsaveraged = false;
  //if STA send needed but we can average datastream results in freq first, do so
  if(scratchspace->starecord != 0 && procslots[index].preavresultlength != procslots[index].postavresultlength && config->getMinPostAvFreqChannels(procslots[index].configindex) >= config->getSTADumpChannels())
  {
    for(int i=0;i<numdatastreams;i++) {
      modes[i]->averageFrequency();
    }
    datastreamsaveraged = true;
  }

  //if required, send off a message with the STA results
  //(before averaging in case high spectral resolution for the dump is required)
  if(scratchspace->starecord != 0) {
    scratchspace->starecord->sec = model->getScanStartSec(procslots[index].offsets[0], startmjd, startseconds) + procslots[index].offsets[1];
    scratchspace->starecord->ns = procslots[index].offsets[2];
    for (int i=0;i<numdatastreams;i++) {
      scratchspace->starecord->antId = i;
      for (int j=0;j<config->getDNumTotalBands(procslots[index].configindex, i);j++) {
        scratchspace->starecord->nChan = config->getSTADumpChannels();
        freqindex = config->getDTotalFreqIndex(procslots[index].configindex, i, j);
        freqchannels = config->getFNumChannels(freqindex);
        if(datastreamsaveraged)
          freqchannels /= config->getFChannelsToAverage(freqindex);
        if (freqchannels < scratchspace->starecord->nChan)
          scratchspace->starecord->nChan = freqchannels;
        channelinc = freqchannels/scratchspace->starecord->nChan;
        scratchspace->starecord->bandId = j;
        nyquistoffset = (config->getFreqTableLowerSideband(freqindex))?1:0;
        acdata = (f32*)(modes[i]->getAutocorrelation(false, j));
        for (int k=0;k<scratchspace->starecord->nChan;k++) {
          //cout << "Doing channel " << k << endl;
          scratchspace->starecord->data[k] = acdata[2*(k*channelinc + nyquistoffset)];
          for (int l=1;l<channelinc;l++)
            scratchspace->starecord->data[k] += acdata[2*(k*channelinc+l+nyquistoffset)];
        }
        //cout << "About to send the binary message" << endl;
        difxMessageSendBinary((const char *)(scratchspace->starecord), BINARY_STA, sizeof(DifxMessageSTARecord) + scratchspace->starecord->nChan);
      }
    }
    //cout << "Finished doing some STA stuff" << endl;
  }

  //if required, average the datastreams down in frequency
  if(procslots[index].preavresultlength != procslots[index].postavresultlength && !datastreamsaveraged) {
    for(int i=0;i<numdatastreams;i++) {
      modes[i]->averageFrequency();
    }
  }

  //lock the thread extra "copy" lock, so we're the only one adding to the result array (datastream section)
  perr = pthread_mutex_lock(&(procslots[index].copylocks[numbaselines]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying lock copy mutex!!!" << endl;

  //copy the autocorrelations
  for(int j=0;j<numdatastreams;j++)
  {
    for(int k=0;k<config->getDNumTotalBands(procslots[index].configindex, j);k++)
    {
      freqindex = config->getDTotalFreqIndex(procslots[index].configindex, j, k);
      if(config->isFrequencyUsed(procslots[index].configindex, freqindex)) {
        freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
        nyquistoffset = (config->getFreqTableLowerSideband(freqindex))?1:0;
        //put autocorrs in resultsbuffer
        status = vectorAdd_cf32_I(modes[j]->getAutocorrelation(false, k), &procslots[index].results[scratchspace->copyindex+nyquistoffset], freqchannels);
        if(status != vecNoErr)
          csevere << startl << "Error copying autocorrelations for datastream " << j << ", band " << k << endl;
        if(k>=config->getDNumRecordedBands(procslots[index].configindex, j)) {
          //need to get the weight from the parent band
          if(config->getFreqTableLowerSideband(freqindex))
            nyquistchannel = 0;
          else
            nyquistchannel = freqchannels;
          int parentfreqindex = config->getDZoomFreqParentFreqIndex(procslots[index].configindex, j, freqindex);
          for(int l=0;l<config->getDNumRecordedBands(procslots[index].configindex, j);l++) {
            if(config->getDRecordedFreqIndex(procslots[index].configindex, j, l) == parentfreqindex && config->getDZoomBandPol(procslots[index].configindex, j, k-config->getDNumRecordedBands(procslots[index].configindex, j)) == config->getDRecordedBandPol(procslots[index].configindex, j, l)) {
              procslots[index].results[scratchspace->copyindex+nyquistchannel].im += modes[j]->getAutocorrelation(false, l)[nyquistchannel].im;
            }
          }
        }
        scratchspace->copyindex += freqchannels+1;
      }
    }
    if(writecrossautocorrs && maxproducts > 2) //want the cross-polarisation autocorrs as well
    {
      for(int k=0;k<config->getDNumTotalBands(procslots[index].configindex, j);k++)
      {
        freqindex = config->getDTotalFreqIndex(procslots[index].configindex, j, k);
        if(config->isFrequencyUsed(procslots[index].configindex, freqindex)) {
          freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
          nyquistoffset = (config->getFreqTableLowerSideband(freqindex))?1:0;
          //put autocorrs in resultsbuffer
          status = vectorAdd_cf32_I(modes[j]->getAutocorrelation(true, k), &procslots[index].results[scratchspace->copyindex]+nyquistoffset, freqchannels);
          if(status != vecNoErr)
            csevere << startl << "Error copying cross-polar autocorrelations for datastream " << j << ", band " << k << endl;
          if(k>=config->getDNumRecordedBands(procslots[index].configindex, j)) {
            //need to get the weight from the parent band
            if(config->getFreqTableLowerSideband(freqindex))
              nyquistchannel = 0;
            else
              nyquistchannel = freqchannels;
            int parentfreqindex = config->getDZoomFreqParentFreqIndex(procslots[index].configindex, j, freqindex);
            for(int l=0;l<config->getDNumRecordedBands(procslots[index].configindex, j);l++) {
              if(config->getDRecordedFreqIndex(procslots[index].configindex, j, l) == parentfreqindex && config->getDZoomBandPol(procslots[index].configindex, j, k-config->getDNumRecordedBands(procslots[index].configindex, j)) == config->getDRecordedBandPol(procslots[index].configindex, j, l)) {
                procslots[index].results[scratchspace->copyindex+nyquistchannel].im += modes[j]->getAutocorrelation(true, l)[nyquistchannel].im;
              }
            }
          }
          scratchspace->copyindex += freqchannels+1;
        }
      }
    }
  }

  //unlock the thread extra "copy" lock (datastream section)
  perr = pthread_mutex_unlock(&(procslots[index].copylocks[numbaselines]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying unlock copy mutex!!!" << endl;

  //grab the next lock
  perr = pthread_mutex_lock(&(procslots[(index+1)%RECEIVE_RING_LENGTH].slotlocks[threadid]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying lock mutex " << (index+1)%RECEIVE_RING_LENGTH << endl;

  //unlock the one we had
  perr = pthread_mutex_unlock(&(procslots[index].slotlocks[threadid]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << mpiid << "/" << threadid << " error trying unlock mutex " << index << endl; 
}

void Core::uvshiftAndAverage(int index, int threadid, double nsoffset, Polyco * currentpolyco, threadscratchspace * scratchspace)
{
  int status, startbaseline, savedcopyindex, binloop;
  int freqindex, freqchannels, nyquistoffset, channelinc;

  //first scale the pulsar data if necessary
  if(procslots[index].pulsarbin && procslots[index].scrunchoutput)
  {
    f64 * binweights = currentpolyco->getBinWeights();

    for(int i=0;i<numbaselines;i++)
    {
      for(int j=0;j<config->getBNumFreqs(procslots[index].configindex, i);j++)
      {
        freqchannels = config->getFNumChannels(config->getBFreqIndex(procslots[index].configindex, i, j));
        //the Nyquist channel referred to here is for the *first* datastream of the baseline, in the event
        //that one datastream has USB and the other has LSB
        nyquistoffset = 0;
        if(config->getFreqTableLowerSideband(config->getBFreqIndex(procslots[index].configindex, i, j))) {
          nyquistoffset = 1;
        }
        //for(int s=0;s<model->getNumPhaseCentres(procslots[index].offsets[0]);s++)
        for(int s=0;s<1;s++) //currently hardcoded to only one phase centre for pulsar binning
        {
          for(int k=0;k<config->getBNumPolProducts(procslots[index].configindex,i,j);k++)
          {
            for(int l=0;l<procslots[index].numpulsarbins;l++)
            {
              //Scale the accumulation space
              status = vectorMulC_f32_I((f32)(binweights[l]), (f32*)(&(scratchspace->pulsaraccumspace[i][j][s][k][l][nyquistoffset])), 2*freqchannels);
              if(status != vecNoErr)
                csevere << startl << "Error trying to scale for scrunch!!!" << endl;
            }
          }
        }
      }
    }
  }

  scratchspace->resultindex = 0;
  scratchspace->copyindex = 0;
  startbaseline = (threadid*numbaselines)/numprocessthreads;
  binloop = 1;
  if(procslots[index].pulsarbin && !procslots[index].scrunchoutput)
    binloop = procslots[index].numpulsarbins;

  //skip ahead to the position in threadresults where this thread will start
  for(int i=0;i<startbaseline;i++)
  {
    //get to where we will start
    for(int j=0;j<config->getBNumFreqs(procslots[index].configindex, i);j++)
    {
      freqindex = config->getBFreqIndex(procslots[index].configindex, i, j);
      freqchannels = config->getFNumChannels(freqindex);
      channelinc = config->getFChannelsToAverage(freqindex);
      for(int b=0;b<binloop;b++)
      {
        for(int k=0;k<config->getBNumPolProducts(procslots[index].configindex,i,j);k++)
        {
          scratchspace->resultindex += freqchannels + 1;
          for(int s=0;s<model->getNumPhaseCentres(procslots[index].offsets[0]);s++)
            scratchspace->copyindex += freqchannels/channelinc + 1;
        }
      }
    }
  }

  //now rotate (if necessary) and average in frequency (if necessary) while copying
  //from threadresults to the slot results
  for(int i=startbaseline;i<numbaselines;i++)
  {
    uvshiftAndAverageBaseline(index, threadid, nsoffset, scratchspace, i);
  }

  //save the copyindex so we know where the autocorrelations will go
  savedcopyindex = scratchspace->copyindex;

  //back to the start and do the others we skipped earlier
  scratchspace->resultindex = 0;
  scratchspace->copyindex = 0;
  for(int i=0;i<startbaseline;i++)
  {
    uvshiftAndAverageBaseline(index, threadid, nsoffset, scratchspace, i);
  }

  //clear the pulsar accumulation vector if necessary
  //threadresults is cleared at the start of processData() so no need to do that now
  if(procslots[index].pulsarbin && procslots[index].scrunchoutput)
  {
    for(int i=0;i<numbaselines;i++)
    {
      for(int j=0;j<config->getBNumFreqs(procslots[index].configindex, i);j++)
      {
        //for(int s=0;s<model->getNumPhaseCentres(procslots[index].offsets[0]);s++)
        for(int s=0;s<1;s++) //currently hardcoded to only one phase centre result when pulsar binning
        {
          for(int k=0;k<config->getBNumPolProducts(procslots[index].configindex,i,j);k++)
          {
            for(int l=0;l<procslots[index].numpulsarbins;l++)
            {
              //zero the accumulation space for next time
              status = vectorZero_cf32(scratchspace->pulsaraccumspace[i][j][s][k][l], freqchannels+1);
              if(status != vecNoErr)
                csevere << startl << "Error trying to zero pulsaraccumspace!!!" << endl;
            }
          }
        }
      }
    }
  }

  //replace the copyindex for the autocorrelations
  scratchspace->copyindex = savedcopyindex;
}

void Core::uvshiftAndAverageBaseline(int index, int threadid, double nsoffset, threadscratchspace * scratchspace, int baseline)
{
  int status, perr, binloop, srcoffset;
  int freqindex, freqchannels, nyquistchannel, nyquistoffset, channelinc, sidebandinc;
  int startchannel, endchannel, atchannel, channelcount, numaverages;
  int antenna1index, antenna2index;
  float baselineweight;
  double bandwidth, lofrequency, pointingcentredelay1, pointingcentredelay2, applieddelay, turns, edgeturns;
  double * phasecentredelay1;
  double * phasecentredelay2;
  cf32* srcpointer;

  binloop = 1;
  if(procslots[index].pulsarbin)
    binloop = procslots[index].numpulsarbins;

  //allocate space for the phase centre delays if necessary, and calculate pointing centre delays
  if(model->getNumPhaseCentres(procslots[index].offsets[0]) > 1)
  {
    phasecentredelay1 = new double[model->getNumPhaseCentres(procslots[index].offsets[0])];
    phasecentredelay2 = new double[model->getNumPhaseCentres(procslots[index].offsets[0])];

    antenna1index = config->getDModelFileIndex(procslots[index].configindex, config->getBDataStream1Index(procslots[index].configindex, baseline));
    antenna2index = config->getDModelFileIndex(procslots[index].configindex, config->getBDataStream2Index(procslots[index].configindex, baseline));
    model->calculateDelayInterpolator(procslots[index].offsets[0], procslots[index].offsets[1] + double(procslots[index].offsets[2]+nsoffset)/1000000000.0, 0.0, 0, antenna1index, 0, 0, &pointingcentredelay1);
    model->calculateDelayInterpolator(procslots[index].offsets[0], procslots[index].offsets[1] + double(procslots[index].offsets[2]+nsoffset)/1000000000.0, 0.0, 0, antenna2index, 0, 0, &pointingcentredelay2);
    for(int s=0;s<model->getNumPhaseCentres(procslots[index].offsets[0]);s++)
    {
      model->calculateDelayInterpolator(procslots[index].offsets[0], procslots[index].offsets[1] + double(procslots[index].offsets[2]+nsoffset)/1000000000.0, 0.0, 0, antenna1index, s+1, 0, &phasecentredelay1[s]);
      model->calculateDelayInterpolator(procslots[index].offsets[0], procslots[index].offsets[1] + double(procslots[index].offsets[2]+nsoffset)/1000000000.0, 0.0, 0, antenna2index, s+1, 0, &phasecentredelay2[s]);
    }
  }

  //lock the mutex for this segment of the copying
  perr = pthread_mutex_lock(&(procslots[index].copylocks[baseline]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << threadid << " error trying lock copy mutex for baseline " << baseline << "!!!" << endl;

  for(int j=0;j<config->getBNumFreqs(procslots[index].configindex, baseline);j++)
  {
    freqindex = config->getBFreqIndex(procslots[index].configindex, baseline, j);
    freqchannels = config->getFNumChannels(freqindex);
    channelinc = config->getFChannelsToAverage(freqindex);
    bandwidth = config->getFreqTableBandwidth(freqindex);
    lofrequency = config->getFreqTableFreq(freqindex);
    numaverages = freqchannels/channelinc;
    sidebandinc = 1;
    startchannel = 0;
    nyquistoffset = 0;
    nyquistchannel = freqchannels;
    endchannel = freqchannels;
    if(config->getFreqTableLowerSideband(freqindex))
    {
      sidebandinc = -1;
      startchannel = freqchannels;
      endchannel = 0;
      nyquistchannel = 0;
      nyquistoffset = 1;
    }

    for(int s=0;s<model->getNumPhaseCentres(procslots[index].offsets[0]);s++)
    {
      srcoffset = 0;
      channelcount = 0;
      if(model->getNumPhaseCentres(procslots[index].offsets[0]) > 1)
      {
        //work out the correct rotator for this frequency and phase centre
        applieddelay = (phasecentredelay2[s]-phasecentredelay1[s]) - (pointingcentredelay2-pointingcentredelay1);
        if(fabs(applieddelay) > 1.0e-20)
        {
          edgeturns = applieddelay*lofrequency;
          edgeturns -= floor(edgeturns);
          for(int r=startchannel;r!=endchannel;r+=sidebandinc)
          {
            atchannel = r-nyquistoffset;
            turns = applieddelay*double(channelcount)*bandwidth/double(freqchannels) + edgeturns;
            scratchspace->argument[atchannel] = (turns-floor(turns))*TWO_PI;
            channelcount++;
          }
          status = vectorSinCos_f32(scratchspace->argument, &(scratchspace->argument[freqchannels]), &(scratchspace->argument[2*freqchannels]), freqchannels);
          if(status != vecNoErr)
            csevere << startl << "Error in phase shift, sin/cos!!!" << status << endl;
          status = vectorRealToComplex_f32(&(scratchspace->argument[2*freqchannels]), &(scratchspace->argument[freqchannels]), scratchspace->rotator, freqchannels);
          if(status != vecNoErr)
            csevere << startl << "Error in phase shift, real to complex!!!" << status << endl;
        }
      }
      for(int k=0;k<config->getBNumPolProducts(procslots[index].configindex,baseline,j);k++)
      {
        if(procslots[index].pulsarbin && procslots[index].scrunchoutput)
          baselineweight = scratchspace->pulsaraccumspace[baseline][j][0][k][0][nyquistchannel].im;
        else
          baselineweight = scratchspace->threadresults[scratchspace->resultindex+srcoffset+nyquistchannel].im;
        for(int b=0;b<binloop;b++)
        {
          if(model->getNumPhaseCentres(procslots[index].offsets[0]) > 1 && fabs(applieddelay) > 1.0e-20)
          {
            if(procslots[index].pulsarbin && procslots[index].scrunchoutput)
              status = vectorMul_cf32(scratchspace->rotator, &(scratchspace->pulsaraccumspace[baseline][j][0][k][b][nyquistoffset]), scratchspace->rotated, freqchannels);
            else
              status = vectorMul_cf32(scratchspace->rotator, &(scratchspace->threadresults[scratchspace->resultindex+srcoffset+nyquistoffset]), scratchspace->rotated, freqchannels);
            if(status != vecNoErr)
              csevere << startl << "Error in phase shift, multiplication!!!" << status << endl;
            srcpointer = scratchspace->rotated;
          }
          else
          {
            if(procslots[index].pulsarbin && procslots[index].scrunchoutput)
              srcpointer = scratchspace->pulsaraccumspace[baseline][j][0][k][b];
            else
              srcpointer = scratchspace->threadresults + scratchspace->resultindex + srcoffset + nyquistoffset;
          }

          //now average (or just copy) from the designated pointer to the main result buffer
          if(channelinc == 1) //this frequency is not averaged
          {
            status = vectorAdd_cf32_I(srcpointer, &(procslots[index].results[scratchspace->copyindex+nyquistoffset]), freqchannels);
            if(status != vecNoErr)
              cerror << startl << "Error trying to copy frequency " << j << " of baseline " << baseline << " when not averaging in frequency" << endl;
            procslots[index].results[scratchspace->copyindex+nyquistchannel].im = baselineweight;
            if(b==binloop-1 || !procslots[index].scrunchoutput) //don't advance too early when scrunching
              scratchspace->copyindex += freqchannels+1;
          }
          else //this frequency *is* averaged - deal with it
          {
            for(int l=0;l<numaverages;l++)
            {
              status = vectorMean_cf32(&(srcpointer[l*channelinc]), channelinc, &(scratchspace->channelsums[l]), vecAlgHintFast);
              if(status != vecNoErr)
                cerror << startl << "Error trying to average frequency " << j << " of baseline " << baseline << endl;
            }
            status = vectorAdd_cf32_I(scratchspace->channelsums, &(procslots[index].results[scratchspace->copyindex+nyquistoffset]), numaverages);
            procslots[index].results[scratchspace->copyindex+nyquistchannel/channelinc].im = baselineweight;
            if(b==binloop-1 || !procslots[index].scrunchoutput) //don't advance too early when scrunching
              scratchspace->copyindex += numaverages+1;
          }
          srcoffset += freqchannels+1;
        }
      }
    }
    scratchspace->resultindex += srcoffset;
  }

  //unlock the mutex for this segment of the copying
  perr = pthread_mutex_unlock(&(procslots[index].copylocks[baseline]));
  if(perr != 0)
    csevere << startl << "PROCESSTHREAD " << threadid << " error trying lock copy mutex for baseline " << baseline << "!!!" << endl;

  //free the phasecentredelay vectors if necessary
  if(model->getNumPhaseCentres(procslots[index].offsets[0]) > 1)
  {
    delete [] phasecentredelay1;
    delete [] phasecentredelay2;
  }
}

void Core::createPulsarAccumSpace(cf32****** pulsaraccumspace, int newconfigindex, int oldconfigindex, int threadid)
{
  int status, freqchannels;

  if(oldconfigindex >= 0 && config->pulsarBinOn(oldconfigindex) && config->scrunchOutputOn(oldconfigindex))
  {
    //need to delete the old pulsar accumulation space
    for(int i=0;i<numbaselines;i++)
    {
      for(int j=0;j<config->getBNumFreqs(oldconfigindex, i);j++)
      {
        freqchannels = config->getFNumChannels(config->getBFreqIndex(oldconfigindex, i, j));
        //for(int s=0;s<config->getMaxPhaseCentres(oldconfigindex);s++)
        for(int s=0;s<1;s++) //forced to single pulsar ephemeris for now
        {
          for(int k=0;k<config->getBNumPolProducts(oldconfigindex,i,j);k++)
          {
            for(int l=0;l<config->getNumPulsarBins(oldconfigindex);l++)
            {
              threadbytes[threadid] -= 8*(freqchannels+1);
              vectorFree(pulsaraccumspace[i][j][s][k][l]);
            }
            delete [] pulsaraccumspace[i][j][s][k];
          }
          delete [] pulsaraccumspace[i][j][s];
        }
        delete [] pulsaraccumspace[i][j];
      }
      delete [] pulsaraccumspace[i];
    }
  }

  if(newconfigindex >= 0 && config->pulsarBinOn(newconfigindex) && config->scrunchOutputOn(newconfigindex))
  {
    //need to create a new pulsar accumulation space
    for(int i=0;i<numbaselines;i++)
    {
      pulsaraccumspace[i] = new cf32****[config->getBNumFreqs(newconfigindex, i)];
      for(int j=0;j<config->getBNumFreqs(newconfigindex, i);j++)
      {
        freqchannels = config->getFNumChannels(config->getBFreqIndex(newconfigindex, i, j));
        pulsaraccumspace[i][j] = new cf32***[config->getMaxPhaseCentres(newconfigindex)];
        //for(int s=0;s<config->getMaxPhaseCentres(newconfigindex);s++)
        for(int s=0;s<1;s++) //forced to single pulsar ephemeris for now
        {
          pulsaraccumspace[i][j][s] = new cf32**[config->getBNumPolProducts(newconfigindex,i,j)];
          for(int k=0;k<config->getBNumPolProducts(newconfigindex,i,j);k++)
          {
            pulsaraccumspace[i][j][s][k] = new cf32*[config->getNumPulsarBins(newconfigindex)];
            for(int l=0;l<config->getNumPulsarBins(newconfigindex);l++)
            {
              pulsaraccumspace[i][j][s][k][l] = vectorAlloc_cf32(freqchannels + 1);
              threadbytes[threadid] += 8*(freqchannels+1);
              status = vectorZero_cf32(pulsaraccumspace[i][j][s][k][l], freqchannels+1);
              if(status != vecNoErr)
                csevere << startl << "Error trying to zero pulsaraccumspace!!!" << endl;
            }
          }
        }
      }
    }
  }
}

void Core::updateconfig(int oldconfigindex, int configindex, int threadid, int & startblock, int & numblocks, int & numpolycos, bool & pulsarbin, Mode ** modes, Polyco ** polycos, bool first)
{
  Polyco ** currentpolycos;
  int blockspersend = config->getBlocksPerSend(configindex);
  startblock = 0;
  numblocks = 0;

  //figure out what section we are responsible for
  for(int i=0;i<=threadid;i++)
  {
    startblock += numblocks;
    numblocks = blockspersend/numprocessthreads + ((i < blockspersend%numprocessthreads)?1:0);
  }

  if(!first) //need to delete the old stuff
  {
    for(int i=0;i<numdatastreams;i++) {
      threadbytes[threadid] -= modes[i]->getEstimatedBytes();
      delete modes[i];
    }
    if(threadid > 0 && pulsarbin)
    {
      //only delete the polycos if they were a copy (threadid > 0)
      for(int i=0;i<numpolycos;i++) {
        threadbytes[threadid] -= polycos[i]->getEstimatedBytes();
        delete polycos[i];
      }
    }
  }

  //get the config to create the appropriate Modes for us
  for(int i=0;i<numdatastreams;i++) {
    modes[i] = config->getMode(configindex, i);
    if(!modes[i]->initialisedOK())
      MPI_Abort(MPI_COMM_WORLD, 1);
    threadbytes[threadid] += modes[i]->getEstimatedBytes();
  }

  pulsarbin = config->pulsarBinOn(configindex);
  if(pulsarbin)
  {
    currentpolycos = config->getPolycos(configindex);
    numpolycos = config->getNumPolycos(configindex);
    for(int i=0;i<numpolycos;i++)
    {
      //if we are not the first thread, create a copy of the Polyco for our use
      polycos[i] = (threadid==0)?currentpolycos[i]:new Polyco(*currentpolycos[i]);
      if(threadid == 0)
        threadbytes[threadid] += polycos[i]->getEstimatedBytes();
    }
    //cinfo << startl << "Core " << mpiid << " thread " << threadid << ": polycos created/copied successfully!"  << endl;
  }
}

