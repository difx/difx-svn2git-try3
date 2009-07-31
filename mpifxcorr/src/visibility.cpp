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
#include "config.h"
#include "visibility.h"
#include "core.h"
#include "datastream.h"
#include <dirent.h>
#include <cmath>
#include <string>
#include <string.h>
#include <stdio.h>
#include <iomanip>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <difxmessage.h>
#include "alert.h"

Visibility::Visibility(Configuration * conf, int id, int numvis, int eseconds, int scan, int scanstartsec, int startns, const string * pnames, bool mon, int port, char * hname, int * sock, int monskip)
  : config(conf), visID(id), numvisibilities(numvis), executeseconds(eseconds), currentscan(scan), currentstartseconds(scanstartsec), currentstartns(startns), polnames(pnames), monitor(mon), portnum(port), hostname(hname), mon_socket(sock), monitor_skip(monskip)
{
  int status;

  //cverbose << startl << "About to create visibility " << id << "/" << numvis << endl;
  estimatedbytes = 0;
  model = config->getModel();

  if(visID == 0)
    *mon_socket = -1;
  maxproducts = config->getMaxProducts();
  autocorrwidth = 1;
  if (maxproducts > 2 && config->writeAutoCorrs(config->getScanConfigIndex(currentscan)))
    autocorrwidth = 2;
  first = true;
  configuredok = true;
  currentsubints = 0;
  numdatastreams = config->getNumDataStreams();
  resultlength = config->getMaxPostavResultLength();
  results = vectorAlloc_cf32(resultlength);
  estimatedbytes += 8*resultlength; //for the results
  estimatedbytes += 4*(config->getNumDataStreams() + config->getNumBaselines())*config->getDNumTotalBands(0,0); //a rough stab at the calibration arrays
  status = vectorZero_cf32(results, resultlength);
  if(status != vecNoErr)
    csevere << startl << "Error trying to zero when creating visibility " << visID << endl;
  numbaselines = config->getNumBaselines();
  currentconfigindex = config->getScanConfigIndex(currentscan);
  expermjd = config->getStartMJD();
  experseconds = config->getStartSeconds();
  offsetns = 0;
  changeConfig(currentconfigindex);

  //set up the initial time period this Visibility will be responsible for
  offsetns = offsetns + offsetnsperintegration;
  subintsthisintegration = (int)((((long long)config->getIntTime(currentconfigindex))*1000000000)/config->getSubintNS(currentconfigindex));
  if(offsetns >= config->getSubintNS(currentconfigindex)/2)
  {
    offsetns -= config->getSubintNS(currentconfigindex)/2;
    subintsthisintegration++;
  }
  for(int i=0;i<visID;i++)
    updateTime();
}


Visibility::~Visibility()
{
  vectorFree(results);
  for(int i=0;i<numdatastreams;i++)
    delete [] autocorrcalibs[i];
  delete [] autocorrcalibs;

  for(int i=0;i<numbaselines;i++)
  {
    for(int j=0;j<config->getBNumFreqs(currentconfigindex, i);j++)
      delete [] baselineweights[i][j];
    delete [] baselineweights[i];
  }
  delete [] baselineweights;

  if(pulsarbinon) {
    for(int i=0;i<config->getFreqTableLength();i++) {
      for(int j=0;j<config->getFNumChannels(i)+1;j++)
        vectorFree(binweightsums[i][j]);
      for(int j=0;j<((config->scrunchOutputOn(currentconfigindex))?1:config->getNumPulsarBins(currentconfigindex));j++)
        vectorFree(binscales[i][j]);
      delete [] binweightsums[i];
      delete [] binscales[i];
    }
    delete [] binweightsums;
    delete [] binscales;
    vectorFree(binweightdivisor);
  }
}

bool Visibility::addData(cf32* subintresults)
{
  int status;

  status = vectorAdd_cf32_I(subintresults, results, resultlength);
  if(status != vecNoErr)
    csevere << startl << "Error copying results in Vis. " << visID << endl;
  currentsubints++;

  return (currentsubints==subintsthisintegration); //are we finished integrating?
}

void Visibility::increment()
{
  int status;
  int sec = experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds;

  cinfo << startl << "Vis. " << visID << " is incrementing, since currentsubints = " << currentsubints << ".  The approximate mjd/seconds is " << expermjd + sec/86400 << "/" << (sec)%86400 << endl;

  currentsubints = 0;
  for(int i=0;i<numvisibilities;i++) //adjust the start time and offset
    updateTime();

  status = vectorZero_cf32(results, resultlength);
  if(status != vecNoErr)
    csevere << startl << "Error trying to zero when incrementing visibility " << visID << endl;

  if(pulsarbinon) {
    for(int i=0;i<config->getFreqTableLength();i++) {
      for(int j=0;j<config->getFNumChannels(i)+1;j++) {
        if(config->scrunchOutputOn(currentconfigindex))
        {
          binweightsums[i][j][0] = 0.0;
        }
        else
        {
          status = vectorZero_f32(binweightsums[i][j], config->getNumPulsarBins(currentconfigindex));
          if(status != vecNoErr)
            csevere << startl << "Error trying to zero binweightsums when incrementing visibility " << visID << endl;
        }
      }
    }
  }
}

void Visibility::updateTime()
{
  int configindex;

  offsetns = offsetns+offsetnsperintegration;
  subintsthisintegration = (int)((((long long)config->getIntTime(currentconfigindex))*1000000000)/config->getSubintNS(currentconfigindex));
  if(offsetns >= config->getSubintNS(currentconfigindex)/2)
  {
    offsetns -= config->getSubintNS(currentconfigindex);
    subintsthisintegration++;
  }

  currentstartseconds += (int)config->getIntTime(currentconfigindex);
  currentstartns += (int)((config->getIntTime(currentconfigindex)-(int)config->getIntTime(currentconfigindex))*1000000000 + 0.5);
  currentstartseconds += currentstartns/1000000000;
  currentstartns %= 1000000000;

  if(currentscan < model->getNumScans() && currentstartseconds >= model->getScanDuration(currentscan)) {
    currentscan++;
    currentstartseconds = 0;
    currentstartns = 0;
    offsetns = offsetnsperintegration;
    subintsthisintegration = (int)((((long long)config->getIntTime(currentconfigindex))*1000000000)/config->getSubintNS(currentconfigindex));
    if(offsetns >= config->getSubintNS(currentconfigindex)/2)
    {
      offsetns -= config->getSubintNS(currentconfigindex);
      subintsthisintegration++;
    }
  }

  if(currentscan < model->getNumScans())
    configindex = config->getScanConfigIndex(currentscan);
  while(configindex < 0 && currentscan < model->getNumScans())
    configindex = config->getScanConfigIndex(++currentscan);

  if(configindex != currentconfigindex && currentscan < model->getNumScans())
  {
    changeConfig(configindex);
  }
}

//setup monitoring socket
int Visibility::openMonitorSocket(char *hostname, int port, int window_size, int *sock) {
  int status;
  int err=0;
  unsigned long ip_addr;
  struct hostent     *hostptr;
  struct sockaddr_in server;    /* Socket address */
  int saveflags,ret,back_err;
  fd_set fd_w;
  struct timeval timeout;

  timeout.tv_sec = 0;
  timeout.tv_usec = 100000;

  hostptr = gethostbyname(hostname);
  if (hostptr==NULL) {
    cwarn << startl << "Failed to look up hostname " << hostname << endl;
    return(1);
  }
  
  memcpy(&ip_addr, (char *)hostptr->h_addr, sizeof(ip_addr));
  memset((char *) &server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons((unsigned short)port); 
  server.sin_addr.s_addr = ip_addr;
  
  cinfo << startl << "Connecting to " << inet_ntoa(server.sin_addr) << endl;
    
  *sock = socket(AF_INET, SOCK_STREAM, 0);
  if (*sock==-1) {
    perror("Failed to allocate socket");
    return(1);
  }

  /* Set the window size to TCP actually works */
  status = setsockopt(*sock, SOL_SOCKET, SO_SNDBUF,
                      (char *) &window_size, sizeof(window_size));
  if (status!=0) {
    close(*sock);
    perror("Setting socket options");
    return(1);
  }

  saveflags=fcntl(*sock,F_GETFL,0);
  if(saveflags<0) {
    perror("fcntl1");
    err=errno;
    return 1;
  }

  /* Set non blocking */
  if(fcntl(*sock,F_SETFL,saveflags|O_NONBLOCK)<0) {
    perror("fcntl2");
    err=errno;
    return 1;
  }

  // try to connect    
  status = connect(*sock, (struct sockaddr *) &server, sizeof(server));
  back_err=errno;

  /* restore flags */
  if(fcntl(*sock,F_SETFL,saveflags)<0) {
    perror("fcntl3");
    err=errno;
    return 1;
  }

  /* return unless the connection was successful or the connect is
           still in progress. */

  if(status<0) {
    if (back_err!=EINPROGRESS) {
      perror("connect");
      err=errno;
      return 1;
    } else {

      FD_ZERO(&fd_w);
      FD_SET(*sock,&fd_w);

      status = select(FD_SETSIZE,NULL,&fd_w,NULL,&timeout);
      if(status < 0) {
	perror("select");
	err=errno;
	return 1;
      }

      /* 0 means it timeout out & no fds changed */
      if(status==0) {
	close(*sock);
	status=ETIMEDOUT;
	return 1;
      }

      /* Get the return code from the connect */
      socklen_t len=sizeof(ret);
      status=getsockopt(*sock,SOL_SOCKET,SO_ERROR,&ret,&len);
      if(status<0) {
	perror("getsockopt");
	status = errno;
	return 1;
      }

      /* ret=0 means success, otherwise it contains the errno */
      if(ret) {
	status=ret;
	return 1;
      }
    }
  }

  return 0;
} /* Setup Net */

int Visibility::sendMonitorData(bool tofollow) {
  char *ptr;
  int ntowrite, nwrote, atsec;

  //ensure the socket is open
  if(checkSocketStatus())
  {
    if(!tofollow)
      atsec = -1;
    else
      atsec = currentstartseconds + model->getScanStartSec(currentscan, expermjd, experseconds);

    ptr = (char*)(&atsec);
    nwrote = send(*mon_socket, ptr, 4, 0);
    if (nwrote < 4)
    {
      cerror << startl << "Error writing to network - will try to reconnect next Visibility 0 integration!" << endl;
      return 1;
    }

    if(tofollow)
    {
      ptr = (char*)results;
      ntowrite = resultlength*sizeof(cf32);

      while (ntowrite>0) {
        nwrote = send(*mon_socket, ptr, ntowrite, 0);
        if(errno == EPIPE)
        {
	  cwarn << startl << "Network seems to have dropped out!  Will try to reconnect shortly...!" << endl;
          return(1);
        }
        if (nwrote==-1) {
          if (errno == EINTR) continue;
          perror("Error writing to network");

          return(1);
        } else if (nwrote==0) {
          cwarn << startl << "Warning: Did not write any bytes!" << endl;
          return(1);
        } else {
          ntowrite -= nwrote;
          ptr += nwrote;
        }
      }
    }
  }
  return(0);
}

bool Visibility::checkSocketStatus()
{
  if(*mon_socket < 0)
  {
    if(visID != 0)
    {
      //don't even try to connect, unless you're the first visibility.  Saves trying to reconnect too often
      cerror << startl << "Visibility " << visID << " won't try to reconnect monitor - waiting for vis 0..." << endl;
      return false;
    }
    if(openMonitorSocket(hostname, portnum, Configuration::MONITOR_TCP_WINDOWBYTES, mon_socket) != 0)
    {
      *mon_socket = -1;
      cerror << startl << "WARNING: Monitor socket could not be opened - monitoring not proceeding! Will try again after " << numvisibilities << " integrations..." << endl;
      return false;
    }
  }
  return true;
}

void Visibility::writedata()
{
  f32 scale, divisor;
  int ds1, ds2, ds1bandindex, ds2bandindex, freqindex, freqchannels, nyquistchannel, nyquistoffset;
  int status, skip, count, binloop;
  int dumpmjd, intsec;
  double dumpseconds;

  cdebug << startl << "Vis. " << visID << " is starting to write out data" << endl;

  if(currentstartseconds + model->getScanStartSec(currentscan, expermjd, experseconds) >= executeseconds)
  {
    cdebug << startl << "Vis. " << visID << " is not writing out any data, since the time is past the end of the correlation" << endl;
    return; //NOTE EXIT HERE!!!
  }

  intsec = experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds;
  dumpmjd = expermjd + intsec/86400;
  dumpseconds = double(intsec%86400) + ((double)currentstartns)/1000000000.0 + config->getIntTime(currentconfigindex)/2.0;
  if(dumpseconds > 86400.0) {
    dumpmjd++;
    dumpseconds -= 86400.0;
  }

  if(currentsubints == 0) //nothing to write out
  {
    cdebug << startl << "Vis. " << visID << " is not writing out any data, since it accumulated no data" << endl;
    //if required, send a message to the monitor not to expect any data this integration - 
    //if we can't get through to the monitor, close the socket
    if(monitor && sendMonitorData(false) != 0) {
      cerror << startl << "tried to send a header only to monitor and it failed - closing socket!" << endl;
      close(*mon_socket);
      *mon_socket = -1;
    }
    return; //NOTE EXIT HERE!!!
  }

  skip = 0;
  count = 0;
  if(config->pulsarBinOn(currentconfigindex) && !config->scrunchOutputOn(currentconfigindex))
    binloop = config->getNumPulsarBins(currentconfigindex);
  else
    binloop = 1;

  for(int i=0;i<numbaselines;i++)
  {
    //skip through the baseline visibilities, grabbing the weights as you go and zeroing
    //that cheekily used Nyquist channel imaginary component of the results array
    for(int j=0;j<config->getBNumFreqs(currentconfigindex,i);j++) {
      freqindex = config->getBFreqIndex(currentconfigindex, i, j);
      freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
      //the Nyquist channel referred to here is for the *first* datastream of the baseline, in the event 
      //that one datastream has USB and the other has LSB
      nyquistchannel = freqchannels;
      if(config->getFreqTableLowerSideband(config->getBFreqIndex(currentconfigindex, i, j)))
        nyquistchannel = 0;
      for(int s=0;s<model->getNumPhaseCentres(currentscan);s++)
      {
        for(int b=0;b<binloop;b++)
        {
          for(int k=0;k<config->getBNumPolProducts(currentconfigindex, i, j);k++) {
            if(s==0) {
              if(binloop>1)
                baselineweights[i][j][b][k] = results[skip + nyquistchannel].im/(fftsperintegration*polyco->getBinWidth(b));
              else
                baselineweights[i][j][b][k] = results[skip + nyquistchannel].im/fftsperintegration;
            }
            results[skip + nyquistchannel].im = 0.0;
            skip += freqchannels+1;
          }
        }
      }
    }
  }

  for(int i=0;i<numdatastreams;i++)
  {
    for(int j=0;j<autocorrwidth;j++)
    {
      for(int k=0;k<config->getDNumTotalBands(currentconfigindex, i); k++)
      {
        freqindex = config->getDTotalFreqIndex(currentconfigindex, i, k);
        if(config->isFrequencyUsed(currentconfigindex, freqindex)) {
          freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
          nyquistchannel = freqchannels;
          nyquistoffset = 0;
          if(config->getFreqTableLowerSideband(freqindex)) {
            nyquistchannel = 0;
            nyquistoffset = 1;
          }

          //Grab the weight for this band and then remove it from the resultsarray
          autocorrweights[i][j][k] = results[skip+count+nyquistchannel].im/fftsperintegration;
          results[skip+count+nyquistchannel].im = 0.0;

          //work out the band average, for use in calibration (allows us to calculate fractional correlation)
          if(j==0) {
            status = vectorMean_cf32(&results[skip + count + nyquistoffset], freqchannels, &autocorrcalibs[i][k], vecAlgHintFast);
            if(status != vecNoErr)
              csevere << startl << "Error in getting average of autocorrelation!!!" << status << endl;
          }
          count += freqchannels + 1;
        }
      }
    }
  }
  count = 0;
  for(int i=0;i<numbaselines;i++) //do each baseline
  {
    ds1 = config->getBOrderedDataStream1Index(currentconfigindex, i);
    ds2 = config->getBOrderedDataStream2Index(currentconfigindex, i);
    for(int j=0;j<config->getBNumFreqs(currentconfigindex,i);j++) //do each frequency
    {
      freqindex = config->getBFreqIndex(currentconfigindex, i, j);
      freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
      for(int s=0;s<model->getNumPhaseCentres(currentscan);s++)
      {
        for(int b=0;b<binloop;b++)
        {
          for(int k=0;k<config->getBNumPolProducts(currentconfigindex, i, j);k++) //do each product of this frequency eg RR,LL,RL,LR
          {
            ds1bandindex = config->getBDataStream1BandIndex(currentconfigindex, i, j, k);
            ds2bandindex = config->getBDataStream2BandIndex(currentconfigindex, i, j, k);
            if(config->getDTsys(currentconfigindex, ds1) > 0.0 && config->getDTsys(currentconfigindex, ds2) > 0.0)
            {
              divisor = (Mode::getDecorrelationPercentage(config->getDNumBits(currentconfigindex, ds1)))*(Mode::getDecorrelationPercentage(config->getDNumBits(currentconfigindex, ds2)))*autocorrcalibs[ds1][ds1bandindex].re*autocorrcalibs[ds2][ds2bandindex].re;
              if(divisor > 0.0) //only do it if there is something to calibrate with
                scale = sqrt(config->getDTsys(currentconfigindex, ds1)*config->getDTsys(currentconfigindex, ds2)/divisor);
              else
                scale = 0.0;
            }
            else
            {
              //We want normalised correlation coefficients, so scale by number of contributing
              //samples rather than datastream tsys and decorrelation correction
              if(baselineweights[i][j][b][k] > 0.0)
                scale = 1.0/(baselineweights[i][j][b][k]*meansubintsperintegration*((float)(config->getBlocksPerSend(currentconfigindex)*2*freqchannels*config->getFChannelsToAverage(freqindex))));
              else
                scale = 0.0;
            }

            //amplitude calibrate the data
            if(scale > 0.0)
            {
              status = vectorMulC_f32_I(scale, (f32*)(&(results[count])), 2*(freqchannels+1));
              if(status != vecNoErr)
                csevere << startl << "Error trying to amplitude calibrate the baseline data!!!" << endl;
            }
            count += freqchannels+1;
          }
        }
      }
    }
  }

  if(config->writeAutoCorrs(currentconfigindex)) //if we need to, calibrate the autocorrs
  {
    for(int i=0;i<numdatastreams;i++) //do each datastream
    {
      for(int j=0;j<autocorrwidth;j++) //the parallel, (and the cross if needed) product for which this band is the first
      {
        for(int k=0;k<config->getDNumTotalBands(currentconfigindex, i); k++)
        {
          freqindex = config->getDTotalFreqIndex(currentconfigindex, i, k);
          if(config->isFrequencyUsed(currentconfigindex, freqindex)) {
            freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
            //calibrate the data
            divisor = (Mode::getDecorrelationPercentage(config->getDNumBits(currentconfigindex, i))*sqrt(autocorrcalibs[i][k].re*autocorrcalibs[i][(j==0)?k:config->getDMatchingBand(currentconfigindex, i, k)].re));
            if(divisor > 0.0)
            {
              scale = config->getDTsys(currentconfigindex, i)/divisor;
              if(scale > 0.0)
              {
                status = vectorMulC_f32_I(scale, (f32*)(&(results[count])), 2*(freqchannels+1));
                if(status != vecNoErr)
                  csevere << startl << "Error trying to amplitude calibrate the datastream data!!!" << endl;
              }
              else
              {
                //We want normalised correlation coefficients, so scale by number of contributing
                //samples rather than datastream tsys and decorrelation correction
                if(autocorrweights[i][j][k] > 0.0)
                {
                  scale = 1.0/(autocorrweights[i][j][k]*meansubintsperintegration*((float)(config->getBlocksPerSend(currentconfigindex)*2*freqchannels*config->getFChannelsToAverage(freqindex))));
                  status = vectorMulC_f32_I(scale, (f32*)(&(results[count])), 2*(freqchannels+1));
                  if(status != vecNoErr)
                    csevere << startl << "Error trying to amplitude calibrate the datastream data for the correlation coefficient case!!!" << endl;
                }
              }
            }
            count += freqchannels+1;
          }
        }
      }
    }
  }
/*
  //if necessary, work out the scaling factors for pulsar binning
  if(pulsarbinon) {
    int mjd = expermjd + (experseconds + currentstartseconds)/86400;
    double mjdfrac = (double((experseconds + currentstartseconds)%86400) + ((double)currentstartns)/1000000000.0 + config->getIntTime(currentconfigindex))/86400.0;
    double fftminutes = (((double)config->getSubintNS(currentconfigindex))/((double)config->getBlocksPerSend(currentconfigindex)))/60000000000.0;
    polyco = Polyco::getCurrentPolyco(currentconfigindex, mjd, mjdfrac, config->getPolycos(currentconfigindex), config->getNumPolycos(currentconfigindex), false);
    if (polyco == NULL) {
      cfatal << startl << "Could not locate a Polyco to cover the timerange MJD " << expermjd + (experseconds + currentstartseconds)/86400 << ", seconds " << (experseconds + currentstartseconds)%86400 << " - aborting" << endl;
      Polyco::getCurrentPolyco(currentconfigindex, mjd, mjdfrac, config->getPolycos(currentconfigindex), config->getNumPolycos(currentconfigindex), true);
      configuredok = false;
      return;
    }
    double * binweights = polyco->getBinWeights();

    //FIXME SOMEWHERE IN HERE, CHECK THE AMPLITUDE SCALING FOR MULTIPLE SCRUNCHED BINS!

    polyco->setTime(mjd, mjdfrac);
    for(int i=0;i<subintsthisintegration*config->getBlocksPerSend(currentconfigindex);i++) {
      polyco->getBins(i*fftminutes, pulsarbins);
      for(int j=0;j<config->getFreqTableLength();j++) {
        if (config->isFrequencyUsed(currentconfigindex, j)) {
          for(int k=0;k<config->getFNumChannels(j)/config->getFChannelsToAverage(freqindex)+1;k++) {
            if(config->scrunchOutputOn(currentconfigindex)) {
              binweightsums[j][k][0] += binweights[pulsarbins[j][k]];
            }
            else {
              binweightsums[j][k][pulsarbins[j][k]] += binweights[pulsarbins[j][k]];
            }
          }
        }
      }
    }
    cinfo << startl << "Done calculating weight sums" << endl;
    for(int i=0;i<config->getFreqTableLength();i++) {
      for(int j=0;j<config->getFNumChannels(i)+1;j++) {
        for(int k=0;k<binloop;k++)
            binscales[i][k][j].re = binscales[i][k][j].im = binweightsums[i][j][k] / binweightdivisor[k];
      }
    }
    cinfo << startl << "Done calculating scales" << endl;

    //do the calibration - should address the weight here as well!
    count = 0;
    for(int i=0;i<numbaselines;i++) //do each baseline
    {
      for(int j=0;j<config->getBNumFreqs(currentconfigindex,i);j++) //do each frequency
      {
        freqchannels = config->getFNumChannels(config->getBFreqIndex(currentconfigindex, i, j));
        for(int k=0;k<config->getBNumPolProducts(currentconfigindex, i, j);k++) //do each product of this frequency eg RR,LL,RL,LR
        {
          for(int b=0;b<binloop;b++)
          {
            status = vectorMul_f32_I((f32*)(binscales[config->getBFreqIndex(currentconfigindex, i, j)][b]), (f32*)(&(results[count])), 2*(freqchannels+1));
            if(status != vecNoErr)
              csevere << startl << "Error trying to pulsar amplitude calibrate the baseline data!!!" << endl;
            count += freqchannels+1;
          }
        }
      }
    }
    cinfo << startl << "Done the in-place multiplication" << endl;
  }*/
  
  //all calibrated, now just need to write out
  if(config->getOutputFormat() == Configuration::DIFX)
    writedifx(dumpmjd, dumpseconds);
  else
    writeascii(dumpmjd, dumpseconds);

  //send monitoring data, if we don't have to skip this one
  if(monitor) {
    if (visID % monitor_skip == 0) {
      if (sendMonitorData(true) != 0){ 
	cerror << startl << "Error sending monitoring data - closing socket!" << endl;
	close(*mon_socket);
	*mon_socket = -1;
      }
    } else {
    }
  }

  cdebug << startl << "Vis. " << visID << " has finished writing data" << endl;
}

void Visibility::writeascii(int dumpmjd, double dumpseconds)
{
  ofstream output;
  int binloop, freqchannels, freqindex;
  char datetimestring[26];

  int count = 0;
  int mjd = dumpmjd;
  int seconds = (int)dumpseconds;
  int microseconds = ((int)((dumpseconds - (double)seconds)*1000000.0 + 0.5));
  int hours = seconds/3600;
  int minutes = (seconds-hours*3600)/60;
  seconds = seconds - (hours*3600 + minutes*60);
  while(hours >= 24)
  {
     hours -= 24;
     mjd++;
  }
  sprintf(datetimestring, "%05u_%02u%02u%02u_%06u", mjd, hours, minutes, seconds, microseconds);
  cinfo << startl << "Mjd is " << mjd << ", hours is " << hours << ", minutes is " << minutes << ", seconds is " << seconds << endl;
  
  if(config->pulsarBinOn(currentconfigindex) && !config->scrunchOutputOn(currentconfigindex))
    binloop = config->getNumPulsarBins(currentconfigindex);
  else
    binloop = 1;

  for(int i=0;i<numbaselines;i++)
  {
    for(int j=0;j<config->getBNumFreqs(currentconfigindex,i);j++)
    {
      freqindex = config->getBFreqIndex(currentconfigindex, i, j);
      freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
      for(int s=0;s<model->getNumPhaseCentres(currentscan);s++)
      {
        for(int b=0;b<binloop;b++)
        {
          for(int k=0;k<config->getBNumPolProducts(currentconfigindex, i, j);k++)
          {
            //write out to a naive filename
            output.open(string(string("baseline_")+char('0' + i)+"_freq_"+char('0' + j)+"_product_"+char('0'+k)+"_"+datetimestring+"_source_"+char('0'+s)+"_bin_"+char('0'+b)+".output").c_str(), ios::out|ios::trunc);
            for(int l=0;l<freqchannels+1;l++)
              output << l << " " << sqrt(results[count + l].re*results[count + l].re + results[count + l].im*results[count + l].im) << " " << atan2(results[count + l].im, results[count + l].re) << endl;
            output.close();
            count += freqchannels+1;
          }
        }
      }
    }
  }

  if(config->writeAutoCorrs(currentconfigindex)) //if we need to, write out the autocorrs
  {
    for(int i=0;i<numdatastreams;i++)
    {
      for(int j=0;j<autocorrwidth;j++)
      {
        for(int k=0;k<config->getDNumTotalBands(currentconfigindex, i); k++)
        {
          freqindex = config->getDTotalFreqIndex(currentconfigindex, i, k);
          if(config->isFrequencyUsed(currentconfigindex, freqindex)) {
            freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
            //write out to naive filename
            output.open(string(string("datastream_")+char('0' + i)+"_crosspolar_"+char('0' + j)+"_product_"+char('0'+k)+"_"+datetimestring+"_bin_"+char('0'+0)+".output").c_str(), ios::out|ios::trunc);
            for(int l=0;l<freqchannels+1;l++)
              output << l << " " << sqrt(results[count + l].re*results[count + l].re + results[count + l].im*results[count + l].im) << " " << atan2(results[count + l].im, results[count + l].re) << endl;
            output.close();
            count += freqchannels+1;
          }
        }
      }
    }
  }
}

void Visibility::writedifx(int dumpmjd, double dumpseconds)
{
  ofstream output;
  char filename[256];
  int binloop, freqindex, numpolproducts, firstpolindex, lsboffset, freqchannels;
  int ant1index, ant2index, sourceindex, baselinenumber;
  double scanoffsetsecs;
  bool modelok;
  int count = 0;
  double buvw[3]; //the u,v and w for this baseline at this time
  char polpair[3]; //the polarisation eg RR, LL

  if(currentscan >= model->getNumScans()) {
    cwarn << startl << "Visibility will not write out time " << dumpmjd << "/" << dumpseconds << " since currentscan is " << currentscan << " and numscans is " << model->getNumScans() << endl;
    return;
  }

  if(config->pulsarBinOn(currentconfigindex) && !config->scrunchOutputOn(currentconfigindex))
    binloop = config->getNumPulsarBins(currentconfigindex);
  else
    binloop = 1;

  //work out the time of this integration
  dumpmjd = expermjd + (experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)/86400;
  dumpseconds = double((experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)%86400) + ((double)currentstartns)/1000000000.0 + config->getIntTime(currentconfigindex)/2.0;

  sprintf(filename, "%s/DIFX_%05d_%06d", config->getOutputFilename().c_str(), expermjd, experseconds);
  
  //work through each baseline visibility point
  for(int i=0;i<numbaselines;i++)
  {
    baselinenumber = config->getBNumber(currentconfigindex, i);
    for(int j=0;j<config->getBNumFreqs(currentconfigindex,i);j++)
    {
      freqindex = config->getBFreqIndex(currentconfigindex, i, j);
      freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
      numpolproducts = config->getBNumPolProducts(currentconfigindex, i, j);
      lsboffset = 0;
      if(config->getFreqTableLowerSideband(freqindex))
        lsboffset = 1;

      for(int s=0;s<model->getNumPhaseCentres(currentscan);s++)
      {
        //get the source-specific data
        sourceindex = model->getPhaseCentreSourceIndex(currentscan, s);
        scanoffsetsecs = currentstartseconds + ((double)currentstartns)/1e9 + config->getIntTime(currentconfigindex)/2.0;
        ant1index = config->getDModelFileIndex(currentconfigindex, config->getBOrderedDataStream1Index(currentconfigindex, i));
        ant2index = config->getDModelFileIndex(currentconfigindex, config->getBOrderedDataStream2Index(currentconfigindex, i));
        modelok = model->interpolateUVW(currentscan, scanoffsetsecs, ant1index, ant2index, s+1, buvw);
        if(!modelok)
          csevere << startl << "Could not calculate the UVW for this integration!!!" << endl;
        for(int b=0;b<binloop;b++)
        {
          for(int k=0;k<numpolproducts;k++) 
          {
            config->getBPolPair(currentconfigindex, i, j, k, polpair);

            //open the file for appending in ascii and write the ascii header
            output.open(filename, ios::app);
            writeDiFXHeader(&output, baselinenumber, dumpmjd, dumpseconds, currentconfigindex, sourceindex, freqindex, polpair, b, 0, baselineweights[i][j][b][k], buvw);

            //close, reopen in binary and write the binary data, then close again
            output.close();
            output.open(filename, ios::app|ios::binary);
            //For both USB and LSB data, the Nyquist channel is excised.  Thus, the numchannels that are written out represent the
            //the valid part of the band in both cases, and run from lowest frequency to highest frequency in both cases.  For USB
            //data, the first channel is the DC - for LSB data, the last channel is the DC
            output.write((char*)(results + count + lsboffset), freqchannels*sizeof(cf32));
            output.close();

            count += freqchannels + 1;
          }
        }
      }
    }
  }

  //now each autocorrelation visibility point if necessary
  if(config->writeAutoCorrs(currentconfigindex)) // FIXME -- bug lurks within?
  {
    buvw[0] = 0.0;
    buvw[1] = 0.0;
    buvw[2] = 0.0;
    for(int i=0;i<numdatastreams;i++)
    {
      baselinenumber = 257*(config->getDTelescopeIndex(currentconfigindex, i)+1);
      for(int j=0;j<autocorrwidth;j++)
      {
        for(int k=0;k<config->getDNumTotalBands(currentconfigindex, i); k++)
        {
          freqindex = config->getDTotalFreqIndex(currentconfigindex, i, k);
          if(config->isFrequencyUsed(currentconfigindex, freqindex)) {
            freqchannels = config->getFNumChannels(freqindex)/config->getFChannelsToAverage(freqindex);
            //open, write the header and close
            if(k<config->getDNumRecordedBands(currentconfigindex, i))
              polpair[0] = config->getDRecordedBandPol(currentconfigindex, i, k);
            else
              polpair[0] = config->getDZoomBandPol(currentconfigindex, i, k-config->getDNumRecordedBands(currentconfigindex, i));
            if(j==0)
              polpair[1] = polpair[0];
            else
              polpair[1] = config->getOppositePol(polpair[0]);
            output.open(filename, ios::app);
            writeDiFXHeader(&output, baselinenumber, dumpmjd, dumpseconds, currentconfigindex, sourceindex, freqindex, polpair, 0, 0, autocorrweights[i][j][k], buvw);
            output.close();
  
            //open, write the binary data and close
            output.open(filename, ios::app|ios::binary);
            //see baseline writing section for description of treatment of USB/LSB data and the Nyquist channel
            output.write((char*)(results + lsboffset + count), freqchannels*sizeof(cf32));
            output.close();
            count += freqchannels + 1;
          }
        }
      }
    }
  }
}

void Visibility::multicastweights()
{
  float * weight;
  double mjd;
  int dumpmjd, intsec;
  double dumpseconds;

  if((model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds) >= executeseconds)
  {
    cdebug << startl << "Vis. " << visID << " is not multicasting any weights, since the time is past the end of the correlation" << endl;
    return; //NOTE EXIT HERE!!!
  }

  weight = new float[numdatastreams];
  
  //work out the time of this integration
  intsec = experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds;
  dumpmjd = expermjd + intsec/86400;
  dumpseconds = double(intsec%86400) + ((double)currentstartns)/1000000000.0 + config->getIntTime(currentconfigindex)/2.0;
  if(dumpseconds > 86400.0) {
    dumpmjd++;
    dumpseconds -= 86400.0;
  }

  for(int i=0;i<numdatastreams;i++)
  {
    for(int j=0;j<config->getDNumRecordedBands(currentconfigindex, i);j++)
      weight[i] += autocorrweights[i][0][j]/config->getDNumRecordedBands(currentconfigindex, i);
  }

  mjd = dumpmjd + dumpseconds/86400.0;

  difxMessageSendDifxStatus(DIFX_STATE_RUNNING, "", mjd, numdatastreams, weight);

  delete [] weight;
} 


void Visibility::writeDiFXHeader(ofstream * output, int baselinenum, int dumpmjd, double dumpseconds, int configindex, int sourceindex, int freqindex, const char polproduct[3], int pulsarbin, int flag, float weight, double buvw[3])
{
  *output << setprecision(15);
  *output << "BASELINE NUM:       " << baselinenum << endl;
  *output << "MJD:                " << dumpmjd << endl;
  *output << "SECONDS:            " << dumpseconds << endl;
  *output << "CONFIG INDEX:       " << configindex << endl;
  *output << "SOURCE INDEX:       " << sourceindex << endl;
  *output << "FREQ INDEX:         " << freqindex << endl;
  *output << "POLARISATION PAIR:  " << polproduct[0] << polproduct[1] << endl;
  *output << "PULSAR BIN:         " << pulsarbin << endl;
  *output << "FLAGGED:            " << flag << endl;
  *output << "DATA WEIGHT:        " << weight << endl;
  *output << "U (METRES):         " << buvw[0] << endl;
  *output << "V (METRES):         " << buvw[1] << endl;
  *output << "W (METRES):         " << buvw[2] << endl;
}

void Visibility::changeConfig(int configindex)
{
  char polpair[3];
  bool found;
  polpair[2] = 0;
  int pulsarwidth;

  if(first) 
  {
    //can just allocate without freeing all the old stuff
    first = false;
    autocorrcalibs = new cf32*[numdatastreams];
    autocorrweights = new f32**[numdatastreams];
    baselineweights = new f32***[numbaselines];
    binweightsums = new f32**[config->getFreqTableLength()];
    binscales = new cf32**[config->getFreqTableLength()];
    pulsarbins = new s32*[config->getFreqTableLength()];
  }
  else
  {
    pulsarwidth = 1;
    if(pulsarbinon && !config->scrunchOutputOn(currentconfigindex))
      pulsarwidth = config->getNumPulsarBins(currentconfigindex);
    cverbose << startl << "Starting to delete some old arrays" << endl;
    //need to delete the old arrays before allocating the new ones
    for(int i=0;i<numdatastreams;i++) {
      delete [] autocorrcalibs[i];
      for(int j=0;j<autocorrwidth;j++)
        delete [] autocorrweights[i][j];
      delete [] autocorrweights[i];
    }
    for(int i=0;i<numbaselines;i++)
    {
      for(int j=0;j<config->getBNumFreqs(currentconfigindex, i);j++) {
        for(int k=0;k<pulsarwidth;k++)
          delete [] baselineweights[i][j][k];
        delete [] baselineweights[i][j];
      }
      delete [] baselineweights[i];
    }
    if(pulsarbinon) {
      cverbose << startl << "Starting to delete some pulsar arrays" << endl;
      for(int i=0;i<config->getFreqTableLength();i++) {
        for(int j=0;j<config->getFNumChannels(i)+1;j++)
          vectorFree(binweightsums[i][j]);
        for(int j=0;j<((config->scrunchOutputOn(currentconfigindex))?1:config->getNumPulsarBins(currentconfigindex));j++)
          vectorFree(binscales[i][j]);
        vectorFree(pulsarbins[i]);
        delete [] binweightsums[i];
        delete [] binscales[i];
      }
      vectorFree(binweightdivisor);
      cverbose << startl << "Finished deleting some pulsar arrays" << endl;
    }
  }

  //get the new parameters for this configuration from the config object
  currentconfigindex = configindex;
  autocorrwidth = 1;
  if (maxproducts > 2 && config->writeAutoCorrs(configindex))
    autocorrwidth = 2;
  pulsarbinon = config->pulsarBinOn(configindex);
  pulsarwidth = 1;
  if(pulsarbinon && !config->scrunchOutputOn(currentconfigindex))
    pulsarwidth = config->getNumPulsarBins(currentconfigindex);
  offsetnsperintegration = (int)(((long long)(1000000000.0*config->getIntTime(configindex)))%((long long)config->getSubintNS(configindex)));
  meansubintsperintegration =config->getIntTime(configindex)/(((double)config->getSubintNS(configindex))/1000000000.0);
  fftsperintegration = meansubintsperintegration*config->getBlocksPerSend(configindex);
  cverbose << startl << "For Visibility " << visID << ", offsetnsperintegration is " << offsetnsperintegration << ", subintns is " << config->getSubintNS(configindex) << ", and configindex is now " << configindex << endl;
  resultlength = config->getPostavResultLength(configindex);
  for(int i=0;i<numdatastreams;i++) {
    autocorrcalibs[i] = new cf32[config->getDNumTotalBands(configindex, i)];
    autocorrweights[i] = new f32*[autocorrwidth];
    for(int j=0;j<autocorrwidth;j++)
      autocorrweights[i][j] = new f32[config->getDNumTotalBands(configindex, i)];
  }

  //Set up the baseline weights array
  for(int i=0;i<numbaselines;i++)
  {
    baselineweights[i] = new f32**[config->getBNumFreqs(configindex, i)];
    for(int j=0;j<config->getBNumFreqs(configindex, i);j++) {
      baselineweights[i][j] = new f32*[pulsarwidth];
      for(int k=0;k<pulsarwidth;k++)
        baselineweights[i][j][k] = new f32[config->getBNumPolProducts(configindex, i, j)];
    }
  }

  //create the pulsar bin weight accumulation arrays
  if(pulsarbinon) {
    cverbose << startl << "Starting the pulsar bin initialisation" << endl;
    polyco = Polyco::getCurrentPolyco(configindex, expermjd + (experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)/86400, double((experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)%86400)/86400.0, config->getPolycos(configindex), config->getNumPolycos(configindex), false);
    if (polyco == NULL) {
      cfatal << startl << "Could not locate a Polyco to cover the timerange MJD " << expermjd + (experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)/86400 << ", seconds " << (experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)%86400 << " - aborting" << endl;
      Polyco::getCurrentPolyco(configindex, expermjd + (experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)/86400, double((experseconds + model->getScanStartSec(currentscan, expermjd, experseconds) + currentstartseconds)%86400)/86400.0, config->getPolycos(configindex), config->getNumPolycos(configindex), true);
      configuredok = false;
    }
    //polyco->setTime(expermjd + (experseconds + currentstartseconds)/86400, double((experseconds + currentstartseconds)%86400)/86400.0);
    if(config->scrunchOutputOn(configindex)) {
      binweightdivisor = vectorAlloc_f32(1);
      binweightdivisor[0] = 0.0;
      for (int i=0;i<config->getNumPulsarBins(configindex);i++)
      {
        binweightdivisor[0] += polyco->getBinWeightTimesWidth(i)*fftsperintegration;
      }
      binweightdivisor[0] /= double(config->getNumPulsarBins(configindex));
    }
    else {
      binweightdivisor = vectorAlloc_f32(config->getNumPulsarBins(configindex));
      for (int i=0;i<config->getNumPulsarBins(configindex);i++)
      {
        binweightdivisor[i] = polyco->getBinWeightTimesWidth(i)*fftsperintegration;
      }
    }
    for(int i=0;i<config->getFreqTableLength();i++) {
      binweightsums[i] = new f32*[config->getFNumChannels(i)+1];
      binscales[i] = new cf32*[config->scrunchOutputOn(configindex)?1:config->getNumPulsarBins(configindex)];
      pulsarbins[i] = vectorAlloc_s32(config->getFNumChannels(i)+1);
      for(int j=0;j<config->getFNumChannels(i)+1;j++) {
        if(config->scrunchOutputOn(configindex))
          binweightsums[i][j] = vectorAlloc_f32(1);
        else
          binweightsums[i][j] = vectorAlloc_f32(config->getNumPulsarBins(configindex));
      }
      for(int j=0;j<(config->scrunchOutputOn(configindex)?1:config->getNumPulsarBins(configindex));j++)
        binscales[i][j] = vectorAlloc_cf32(config->getFNumChannels(i) + 1);
    }
    cverbose << startl << "Finished the pulsar bin initialisation" << endl;
  }
}
