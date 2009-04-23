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
#include "mode.h"
#include "math.h"
#include "architecture.h"
#include "alert.h"

//using namespace std;
const float Mode::TINY = 0.000000001;


Mode::Mode(Configuration * conf, int confindex, int dsindex, int recordedbandchan, int bpersend, int gsamples, int nrecordedfreqs, double recordedbw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int nrecordedbands, int nzoombands, int nbits, int unpacksamp, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs, double bclock)
  : config(conf), configindex(confindex), datastreamindex(dsindex), recordedbandchannels(recordedbandchan), blockspersend(bpersend), guardsamples(gsamples), twicerecordedbandchannels(recordedbandchan*2), numrecordedfreqs(nrecordedfreqs), numrecordedbands(nrecordedbands), numzoombands(nzoombands), numbits(nbits), unpacksamples(unpacksamp), recordedbandwidth(recordedbw), blockclock(bclock), filterbank(fbank), calccrosspolautocorrs(cacorrs), fringerotationorder(fringerotorder), arraystridelength(arraystridelen), recordedfreqclockoffsets(recordedfreqclkoffs), recordedfreqlooffsets(recordedfreqlooffs)
{
  int status, localfreqindex;
  int decimationfactor = config->getDDecimationFactor(configindex, datastreamindex);

  initok = true;
  numstrides = twicerecordedbandchannels/arraystridelength;
  sampletime = 1.0/(2.0*recordedbandwidth); //microseconds
  fftdurationmicrosec = twicerecordedbandchannels*sampletime;
  fractionalLoFreq = false;
  for(int i=0;i<numrecordedfreqs;i++)
  {
    if(config->getDRecordedFreq(configindex, datastreamindex, i) - int(config->getDRecordedFreq(configindex, datastreamindex, i)) > TINY)
      fractionalLoFreq = true;
  }

  //now do the rest of the initialising
  samplesperblock = int(recordedbandwidth*2/blockclock);
  if(samplesperblock == 0)
  {
    cfatal << startl << "Error!!! Samplesperblock is less than 1, current implementation cannot handle this situation.  Aborting!" << endl;
    initok = false;
  }
  else
  {
    bytesperblocknumerator = (numrecordedbands*samplesperblock*numbits*decimationfactor)/8;
    if(bytesperblocknumerator == 0)
    {
      bytesperblocknumerator = 1;
      bytesperblockdenominator = 8/(numrecordedbands*samplesperblock*numbits*decimationfactor);
      unpacksamples += bytesperblockdenominator*sizeof(u16)*samplesperblock;
    }
    else
    {
      bytesperblockdenominator = 1;
    }
    samplesperlookup = (numrecordedbands*sizeof(u16)*samplesperblock*bytesperblockdenominator)/bytesperblocknumerator;
    numlookups = (unpacksamples*bytesperblocknumerator)/(bytesperblockdenominator*sizeof(u16)*samplesperblock);
    if(samplesperblock > 1)
      numlookups++;

    unpackedarrays = new f32*[numrecordedbands];
    fftoutputs = new cf32*[numrecordedbands + numzoombands];
    conjfftoutputs = new cf32*[numrecordedbands + numzoombands];
    for(int i=0;i<numrecordedbands;i++)
    {
      unpackedarrays[i] = vectorAlloc_f32(unpacksamples);
      fftoutputs[i] = vectorAlloc_cf32(recordedbandchannels+1);
      conjfftoutputs[i] = vectorAlloc_cf32(recordedbandchannels+1);
    }
    for(int i=0;i<numzoombands;i++)
    {
      localfreqindex = config->getDLocalZoomFreqIndex(confindex, dsindex, i);
      //cout << "Setting the zoom band pointers for zoom band " << i << ", which has is from zoom freq " << localfreqindex << endl;
      //cout << "The parent freq index for this band is " << config->getDZoomFreqParentFreqIndex(confindex, dsindex, localfreqindex) << " and the channel offset is " << config->getDZoomFreqChannelOffset(confindex, dsindex, localfreqindex) << endl;
      //cout.flush();
      fftoutputs[i+numrecordedbands] = &(fftoutputs[config->getDZoomFreqParentFreqIndex(confindex, dsindex, localfreqindex)][config->getDZoomFreqChannelOffset(confindex, dsindex, localfreqindex)]);
      conjfftoutputs[i+numrecordedbands] = &(conjfftoutputs[config->getDZoomFreqParentFreqIndex(confindex, dsindex, localfreqindex)][config->getDZoomFreqChannelOffset(confindex, dsindex, localfreqindex)]);
      //cout << "Done doing the zoom band" << endl;
      //cout.flush();
    }
    dataweight = 0.0;

    lookup = vectorAlloc_s16((MAX_U16+1)*samplesperlookup);
    linearunpacked = vectorAlloc_s16(numlookups*samplesperlookup);

    //initialise the fft info
    order = 0;
    while((twicerecordedbandchannels) >> order != 1)
      order++;
    flag = vecFFT_NoReNorm;
    hint = vecAlgHintFast;

    switch(fringerotationorder) {
      case 2:
        piecewiserotator = vectorAlloc_cf32(arraystridelength);
        quadpiecerotator = vectorAlloc_cf32(arraystridelength);

        subquadxval  = vectorAlloc_f64(arraystridelength);
        subquadphase = vectorAlloc_f64(arraystridelength);
        subquadarg   = vectorAlloc_f32(arraystridelength);
        subquadsin   = vectorAlloc_f32(arraystridelength);
        subquadcos   = vectorAlloc_f32(arraystridelength);

        stepxoffsquared = vectorAlloc_f64(numstrides);
        tempstepxval    = vectorAlloc_f64(numstrides);
      case 1:
        subxoff  = vectorAlloc_f64(arraystridelength);
        subxval  = vectorAlloc_f64(arraystridelength);
        subphase = vectorAlloc_f64(arraystridelength);
        subarg   = vectorAlloc_f32(arraystridelength);
        subsin   = vectorAlloc_f32(arraystridelength);
        subcos   = vectorAlloc_f32(arraystridelength);

        stepxoff  = vectorAlloc_f64(numstrides);
        stepxval  = vectorAlloc_f64(numstrides);
        stepphase = vectorAlloc_f64(numstrides);
        steparg   = vectorAlloc_f32(numstrides);
        stepsin   = vectorAlloc_f32(numstrides);
        stepcos   = vectorAlloc_f32(numstrides);
        stepcplx  = vectorAlloc_cf32(numstrides);

        complexunpacked = vectorAlloc_cf32(twicerecordedbandchannels);
        complexrotator = vectorAlloc_cf32(twicerecordedbandchannels);
        fftd = vectorAlloc_cf32(twicerecordedbandchannels);

        for(int i=0;i<arraystridelength;i++)
          subxoff[i] = fftdurationmicrosec*(double(i)/double(twicerecordedbandchannels));
        for(int i=0;i<numstrides;i++)
          stepxoff[i] = fftdurationmicrosec*(double(i*arraystridelength)/double(twicerecordedbandchannels));
        if(fringerotationorder == 2) {
          for(int i=0;i<numstrides;i++)
            stepxoffsquared[i] = stepxoff[i]*stepxoff[i];
        }

        status = vectorInitFFTC_cf32(&pFFTSpecC, order, flag, hint);
        if(status != vecNoErr)
          csevere << startl << "Error in FFT initialisation!!!" << status << endl;
        status = vectorGetFFTBufSizeC_cf32(pFFTSpecC, &fftbuffersize);
        if(status != vecNoErr)
          csevere << startl << "Error in FFT buffer size calculation!!!" << status << endl;
        break;
      case 0: //zeroth order interpolation, can do "post-F"
        status = vectorInitFFTR_f32(&pFFTSpecR, order, flag, hint);
        if(status != vecNoErr)
          csevere << startl << "Error in FFT initialisation!!!" << status << endl;
        status = vectorGetFFTBufSizeR_f32(pFFTSpecR, &fftbuffersize);
        if(status != vecNoErr)
          csevere << startl << "Error in FFT buffer size calculation!!!" << status << endl;
        break;
    }

    //zero the Nyquist channel for every band - that is where the weight will be stored on all
    //baselines (the imag part) so the datastream channel for it must be zeroed
    for(int i=0;i<numrecordedbands;i++)
    {
      if(config->getDRecordedLowerSideband(configindex, datastreamindex, config->getDLocalRecordedFreqIndex(configindex, datastreamindex, i))) {
        fftoutputs[i][0].re = 0.0;
        fftoutputs[i][0].im = 0.0;
      }
      else {
        fftoutputs[i][recordedbandchannels].re = 0.0;
        fftoutputs[i][recordedbandchannels].im = 0.0;
      }
    }

    fftbuffer = vectorAlloc_u8(fftbuffersize);

    delaylength = blockspersend + guardsamples/twicerecordedbandchannels + 1;
    delays = vectorAlloc_f64(delaylength);

    //cout << "arraystridelength is " << arraystridelength << endl;
    subfracsamparg = vectorAlloc_f32(arraystridelength);
    subfracsampsin = vectorAlloc_f32(arraystridelength);
    subfracsampcos = vectorAlloc_f32(arraystridelength);
    subchannelfreqs = vectorAlloc_f32(arraystridelength);
    lsbsubchannelfreqs = vectorAlloc_f32(arraystridelength);
    /*cout << "subfracsamparg is " << subfracsamparg << endl;
    cout << "subfracsampsin is " << subfracsampsin << endl;
    cout << "subfracsampcos is " << subfracsampcos << endl;
    cout << "subchannelfreqs is " << subchannelfreqs << endl;
    cout << "lsbsubchannelfreqs is " << lsbsubchannelfreqs << endl;*/
    for(int i=0;i<arraystridelength;i++) {
      subchannelfreqs[i] = (float)((TWO_PI*(i+1)*recordedbandwidth)/recordedbandchannels);
      lsbsubchannelfreqs[i] = (float)((-TWO_PI*(arraystridelength-i)*recordedbandwidth)/recordedbandchannels);
    }

    stepfracsamparg = vectorAlloc_f32(numstrides/2);
    stepfracsampsin = vectorAlloc_f32(numstrides/2);
    stepfracsampcos = vectorAlloc_f32(numstrides/2);
    stepfracsampcplx = vectorAlloc_cf32(numstrides/2);
    stepchannelfreqs = vectorAlloc_f32(numstrides/2);
    lsbstepchannelfreqs = vectorAlloc_f32(numstrides/2);
    /*cout << "stepfracsamparg is " << stepfracsamparg << endl;
    cout << "stepfracsampsin is " << stepfracsampsin << endl;
    cout << "stepfracsampcos is " << stepfracsampcos << endl;
    cout << "stepfracsampcplx is " << stepfracsampcplx << endl;
    cout << "stepchannelfreqs is " << stepchannelfreqs << endl;
    cout << "lsbstepchannelfreqs is " << lsbstepchannelfreqs << endl;*/

    for(int i=0;i<numstrides/2;i++) {
      stepchannelfreqs[i] = (float)((TWO_PI*i*arraystridelength*recordedbandwidth)/recordedbandchannels);
      lsbstepchannelfreqs[i] = (float)((-TWO_PI*(numstrides/2-i)*arraystridelength*recordedbandwidth)/recordedbandchannels);
    }

    fracsamprotator = vectorAlloc_cf32(recordedbandchannels + 1);
    /*cout << "Numstrides is " << numstrides << ", recordedbandchannels is " << recordedbandchannels << ", arraystridelength is " << arraystridelength << endl;
    cout << "fracsamprotator is " << fracsamprotator << endl;
    cout << "stepchannelfreqs[5] is " << stepchannelfreqs[5] << endl;
    cout << "subchannelfreqs[5] is " << subchannelfreqs[5] << endl;
    cout << "stepchannelfreqs(last) is " << stepchannelfreqs[numstrides/2-1] << endl;
    fracmult = vectorAlloc_f32(recordedbandchannels + 1);
    fracmultcos = vectorAlloc_f32(recordedbandchannels + 1);
    fracmultsin = vectorAlloc_f32(recordedbandchannels + 1);
    complexfracmult = vectorAlloc_cf32(recordedbandchannels + 1);

    channelfreqs = vectorAlloc_f32(recordedbandchannels + 1);
    for(int i=0;i<recordedbandchannels + 1;i++)
      channelfreqs[i] = (float)((TWO_PI*i*recordedbandwidth)/recordedbandchannels);
    lsbchannelfreqs = vectorAlloc_f32(recordedbandchannels + 1);
    for(int i=0;i<recordedbandchannels + 1;i++)
      lsbchannelfreqs[i] = (float)((-TWO_PI*(recordedbandchannels-i)*recordedbandwidth)/recordedbandchannels);
    */

    //space for the autocorrelations
    if(calccrosspolautocorrs)
      autocorrwidth = 2;
    else
      autocorrwidth = 1;
    autocorrelations = new cf32**[autocorrwidth];
    for(int i=0;i<autocorrwidth;i++)
    {
      autocorrelations[i] = new cf32*[numrecordedbands+numzoombands];
      for(int j=0;j<numrecordedbands;j++)
        autocorrelations[i][j] = vectorAlloc_cf32(recordedbandchannels+1);
      for(int j=0;j<numzoombands;j++)
      {
        localfreqindex = config->getDLocalZoomFreqIndex(confindex, dsindex, j);
        autocorrelations[i][j+numrecordedbands] = &(autocorrelations[i][config->getDZoomFreqParentFreqIndex(confindex, dsindex, localfreqindex)][config->getDZoomFreqChannelOffset(confindex, dsindex, localfreqindex)]);
      }
    }
  }
}

Mode::~Mode()
{
  int status;

  cdebug << startl << "Starting a mode destructor" << endl;

  for(int i=0;i<numrecordedbands;i++)
  {
    vectorFree(unpackedarrays[i]);
    vectorFree(fftoutputs[i]);
    vectorFree(conjfftoutputs[i]);
  }
  delete [] unpackedarrays;
  delete [] fftoutputs;
  delete [] conjfftoutputs;

  switch(fringerotationorder) {
    case 2:
      vectorFree(piecewiserotator);
      vectorFree(quadpiecerotator);

      vectorFree(subquadxval);
      vectorFree(subquadphase);
      vectorFree(subquadarg);
      vectorFree(subquadsin);
      vectorFree(subquadcos);

      vectorFree(stepxoffsquared);
      vectorFree(tempstepxval);
    case 1:
      vectorFree(subxoff);
      vectorFree(subxval);
      vectorFree(subphase);
      vectorFree(subarg);
      vectorFree(subsin);
      vectorFree(subcos);

      vectorFree(stepxoff);
      vectorFree(stepxval);
      vectorFree(stepphase);
      vectorFree(steparg);
      vectorFree(stepsin);
      vectorFree(stepcos);
      vectorFree(stepcplx);

      vectorFree(complexunpacked);
      vectorFree(complexrotator);
      vectorFree(fftd);

      status = vectorFreeFFTC_cf32(pFFTSpecC);
      if(status != vecNoErr)
        csevere << startl << "Error in freeing FFT spec!!!" << status << endl;
      break;
    case 0: //zeroth order interpolation, "post-F"
      status = vectorFreeFFTR_f32(pFFTSpecR);
      if(status != vecNoErr)
        csevere << startl << "Error in freeing FFT spec!!!" << status << endl;
      break;
  }

  vectorFree(lookup);
  vectorFree(linearunpacked);
  vectorFree(fftbuffer);
  vectorFree(delays);

  vectorFree(subfracsamparg);
  vectorFree(subfracsampsin);
  vectorFree(subfracsampcos);
  vectorFree(subchannelfreqs);
  vectorFree(lsbsubchannelfreqs);

  vectorFree(stepfracsamparg);
  vectorFree(stepfracsampsin);
  vectorFree(stepfracsampcos);
  vectorFree(stepfracsampcplx);
  vectorFree(stepchannelfreqs);
  vectorFree(lsbstepchannelfreqs);

  vectorFree(fracsamprotator);
  //vectorFree(fracmult);
  //vectorFree(fracmultcos);
  //vectorFree(fracmultsin);
  //vectorFree(complexfracmult);
  //vectorFree(channelfreqs);

  for(int i=0;i<autocorrwidth;i++)
  {
    for(int j=0;j<numrecordedbands;j++)
      vectorFree(autocorrelations[i][j]);
    delete [] autocorrelations[i];
  }
  delete [] autocorrelations;

  cdebug << startl << "Ending a mode destructor" << endl;
}

float Mode::unpack(int sampleoffset)
{
  int status, leftoversamples, stepin = 0;

  if(bytesperblockdenominator/bytesperblocknumerator == 0)
    leftoversamples = 0;
  else
    leftoversamples = sampleoffset%(bytesperblockdenominator/bytesperblocknumerator);
  unpackstartsamples = sampleoffset - leftoversamples;
  if(samplesperblock > 1)
    stepin = unpackstartsamples%(samplesperblock*bytesperblockdenominator);
  u16 * packed = (u16 *)(&(data[((unpackstartsamples/samplesperblock)*bytesperblocknumerator)/bytesperblockdenominator]));

  //copy from the lookup table to the linear unpacked array
  for(int i=0;i<numlookups;i++)
  {
    status = vectorCopy_s16(&lookup[packed[i]*samplesperlookup], &linearunpacked[i*samplesperlookup], samplesperlookup);
    if(status != vecNoErr) {
      csevere << startl << "Error in lookup for unpacking!!!" << status << endl;
      return 0;
    }
  }

  //split the linear unpacked array into the separate subbands
  status = vectorSplitScaled_s16f32(&(linearunpacked[stepin*numrecordedbands]), unpackedarrays, numrecordedbands, unpacksamples);
  if(status != vecNoErr) {
    csevere << startl << "Error in splitting linearunpacked!!!" << status << endl;
    return 0;
  }

  return 1.0;
}

float Mode::process(int index)  //frac sample error, fringedelay and wholemicroseconds are in microseconds 
{
  double phaserotation, averagedelay, nearestsampletime, starttime, finaloffset, lofreq, distance, walltimesecs;
  f32 phaserotationfloat, fracsampleerror;
  int status, count, nearestsample, integerdelay, sidebandoffset;
  cf32* fftptr;
  cf32* fracsampptr1;
  cf32* fracsampptr2;
  f32* currentsubchannelfreqs;
  f32* currentstepchannelfreqs;
  int indices[10];
  //cout << "hello" << endl;
  //cout.flush();
  //cout << "subquadsin is " << subquadsin << endl;
  //cout.flush();
  if(datalengthbytes == 0 || !(delays[index] > MAX_NEGATIVE_DELAY) || !(delays[index+1] > MAX_NEGATIVE_DELAY))
  {
    for(int i=0;i<numrecordedbands;i++)
    {
      status = vectorZero_cf32(fftoutputs[i], recordedbandchannels+1);
      if(status != vecNoErr)
        csevere << startl << "Error trying to zero fftoutputs when data is bad!" << endl;
      status = vectorZero_cf32(conjfftoutputs[i], recordedbandchannels+1);
      if(status != vecNoErr)
        csevere << startl << "Error trying to zero fftoutputs when data is bad!" << endl;
    }
    //cout << "Mode for DS " << datastreamindex << " is bailing out of index " << index << " because datalengthbytes is " << datalengthbytes << ", delays[index] was " << delays[index] << " and delays[index+1] was " << delays[index+1] << endl;
    return 0.0; //don't process crap data
  }
  //cout << "subquadsin is now " << subquadsin << endl;
  //cout.flush();

  averagedelay = (delays[index] + delays[index+1])/2.0;
  fftstartmicrosec = index*twicerecordedbandchannels*sampletime;
  starttime = (offsetseconds-bufferseconds)*1000000.0 + (double(offsetns)/1000.0 + fftstartmicrosec - buffermicroseconds) - averagedelay;
  nearestsample = int(starttime/sampletime + 0.5);
  walltimesecs = offsetseconds + ((double)offsetns)/1000000000.0 + fftstartmicrosec;

  //if we need to, unpack some more data - first check to make sure the pos is valid at all
  if(nearestsample < -1 || (((nearestsample + twicerecordedbandchannels)/samplesperblock)*bytesperblocknumerator)/bytesperblockdenominator > datalengthbytes)
  {
    cerror << startl << "MODE error for datastream " << datastreamindex << " - trying to process data outside range - aborting!!! nearest sample was " << nearestsample << ", the max bytes should be " << datalengthbytes << ".  bufferseconds was " << bufferseconds << ", offsetseconds was " << offsetseconds << ", buffermicroseconds was " << buffermicroseconds << ", offsetns was " << offsetns << ", index was " << index << ", average delay was " << averagedelay << " composed of previous delay " << delays[index] << " and next delay " << delays[index+1] << endl;
    return 0.0;
  }
  if(nearestsample == -1)
  {
    nearestsample = 0;
    dataweight = unpack(nearestsample);
  }
  else if(nearestsample < unpackstartsamples || nearestsample > unpackstartsamples + unpacksamples - twicerecordedbandchannels)
    //need to unpack more data
    dataweight = unpack(nearestsample);
  //cout << "done some unpacking" << endl;

  if(!(dataweight > 0.0))
    return 0.0;

  //cout << "Good to go" << endl;

  nearestsampletime = nearestsample*sampletime;
  fracsampleerror = float(starttime - nearestsampletime);
  fftstartmicrosec = index*twicerecordedbandchannels*sampletime;

  switch(fringerotationorder) {
    case 0: //post-F
      integerdelay = int(averagedelay);
      break;
    case 1: //linear
      integerdelay = int(delays[index]);
      a = (delays[index+1]-delays[index])/fftdurationmicrosec;
      b = delays[index] - integerdelay;

      status = vectorMulC_f64(subxoff, a, subxval, arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error in linearinterpolate, subval multiplication" << endl;
      status = vectorMulC_f64(stepxoff, a, stepxval, numstrides);
      if(status != vecNoErr)
        csevere << startl << "Error in linearinterpolate, stepval multiplication" << endl;
      status = vectorAddC_f64_I(b, subxval, arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error in linearinterpolate, subval addition!!!" << endl;
      break;
    case 2: //quadratic
      integerdelay = int(delays[0]);
      a = a0;
      b = b0 + fftstartmicrosec*a0*2.0;
      c = c0 + fftstartmicrosec*b0 + fftstartmicrosec*fftstartmicrosec*a0;

      status = vectorMulC_f64(subxoff, b + a*stepxoff[1], subxval, arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error in quadinterpolate, subval multiplication" << endl;
      status = vectorMulC_f64(subxoff, 2*a*stepxoff[1], subquadxval, arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error in quadinterpolate, subquadval multiplication" << endl;
      status = vectorMulC_f64(stepxoff, b, stepxval, numstrides);
      if(status != vecNoErr)
        csevere << startl << "Error in quadinterpolate, stepval multiplication" << endl;
      status = vectorMulC_f64(stepxoffsquared, a, tempstepxval, numstrides);
      if(status != vecNoErr)
        csevere << startl << "Error in quadinterpolate, tempstepval multiplication" << endl;
      status = vectorAdd_f64_I(tempstepxval, stepxval, numstrides);
      if(status != vecNoErr)
        csevere << startl << "Error in quadinterpolate, stepval addition!!!" << endl;
      status = vectorAddC_f64_I(c, subxval, arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error in quadinterpolate, subval addition!!!" << endl;
      break;
  }

  //cout << "Done initial fringe rotation stuff, subquadsin is now " << subquadsin << endl;

  for(int i=0;i<numrecordedfreqs;i++)
  {
    count = 0;
    //updated so that Nyquist channel is not accumulated for either USB or LSB data
    sidebandoffset = 0;
    currentsubchannelfreqs = subchannelfreqs;
    currentstepchannelfreqs = stepchannelfreqs;
    fracsamprotator[0].re = 1.0;
    fracsamprotator[0].im = 0.0;
    fracsampptr1 = &(fracsamprotator[1]);
    fracsampptr2 = &(fracsamprotator[1+arraystridelength]);
    if(config->getDRecordedLowerSideband(configindex, datastreamindex, i)) {
      sidebandoffset = 1;
      currentsubchannelfreqs = lsbsubchannelfreqs;
      currentstepchannelfreqs = lsbstepchannelfreqs;
      fracsamprotator[recordedbandchannels].re = 1.0;
      fracsamprotator[recordedbandchannels].im = 0.0;
      fracsampptr1 = &(fracsamprotator[recordedbandchannels-arraystridelength]);
      fracsampptr2 = &(fracsamprotator[0]);
    }

    //create the array for fractional sample error correction - including the post-f fringe rotation
    //cout << "Currentsubchannelfreqs[0] is " << currentsubchannelfreqs[0] << endl;

    //get ready to apply fringe rotation, if its pre-F
    lofreq = config->getDRecordedFreq(configindex, datastreamindex, i);
    switch(fringerotationorder) {
      case 1:
        status = vectorMulC_f64(subxval, lofreq, subphase, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in linearinterpolate lofreq sub multiplication!!!" << status << endl;
        status = vectorMulC_f64(stepxval, lofreq, stepphase, numstrides);
        if(status != vecNoErr)
          csevere << startl << "Error in linearinterpolate lofreq step multiplication!!!" << status << endl;
        for(int j=0;j<arraystridelength;j++) {
          subarg[j] = -TWO_PI*(subphase[j] - int(subphase[j]));
        }
        for(int j=0;j<numstrides;j++) {
          steparg[j] = -TWO_PI*(stepphase[j] - int(stepphase[j]));
        }
        status = vectorSinCos_f32(subarg, subsin, subcos, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in sin/cos of sub rotate argument!!!" << endl;
        status = vectorSinCos_f32(steparg, stepsin, stepcos, numstrides);
        if(status != vecNoErr)
          csevere << startl << "Error in sin/cos of step rotate argument!!!" << endl;
        status = vectorRealToComplex_f32(subcos, subsin, complexrotator, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error assembling sub into complex!!!" << endl;
        status = vectorRealToComplex_f32(stepcos, stepsin, stepcplx, numstrides);
        if(status != vecNoErr)
          csevere << startl << "Error assembling step into complex!!!" << endl;
        for(int j=1;j<numstrides;j++) {
          status = vectorMulC_cf32(complexrotator, stepcplx[j], &complexrotator[j*arraystridelength], arraystridelength);
          if(status != vecNoErr)
            csevere << startl << "Error doing the time-saving complex multiplication!!!" << endl;
        }
        break;
      case 2:
        status = vectorMulC_f64(subxval, lofreq, subphase, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in quadinterpolate lofreq sub multiplication!!!" << status << endl;
        status = vectorMulC_f64(subquadxval, lofreq, subquadphase, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in quadinterpolate lofreq subquad multiplication!!!" << status << endl;
        status = vectorMulC_f64(stepxval, lofreq, stepphase, numstrides);
        if(status != vecNoErr)
          csevere << startl << "Error in quadinterpolate lofreq step multiplication!!!" << status << endl;
        for(int j=0;j<arraystridelength;j++) {
          subarg[j] = -TWO_PI*(subphase[j] - int(subphase[j]));
          subquadarg[j] = -TWO_PI*(subquadphase[j] - int(subquadphase[j]));
        }
        for(int j=0;j<numstrides;j++) {
          steparg[j] = -TWO_PI*(stepphase[j] - int(stepphase[j]));
        }
        status = vectorSinCos_f32(subarg, subsin, subcos, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in sin/cos of sub rotate argument!!!" << endl;
        status = vectorSinCos_f32(subquadarg, subquadsin, subquadcos, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in sin/cos of subquad rotate argument!!!" << endl;
        status = vectorSinCos_f32(steparg, stepsin, stepcos, numstrides);
        if(status != vecNoErr)
          csevere << startl << "Error in sin/cos of step rotate argument!!!" << endl;
        status = vectorRealToComplex_f32(subcos, subsin, piecewiserotator, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error assembling sub into complex" << endl;
        status = vectorRealToComplex_f32(subquadcos, subquadsin, quadpiecerotator, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error assembling sub into complex" << endl;
        status = vectorRealToComplex_f32(stepcos, stepsin, stepcplx, numstrides);
        if(status != vecNoErr)
          csevere << startl << "Error assembling step into complex" << endl;
        for(int j=0;j<numstrides;j++) {
          status = vectorMulC_cf32(piecewiserotator, stepcplx[j], &complexrotator[j*arraystridelength], arraystridelength);
          if(status != vecNoErr)
            csevere << startl << "Error doing the time-saving complex mult (striding)" << endl;
          status = vectorMul_cf32_I(quadpiecerotator, piecewiserotator, arraystridelength);
          if(status != vecNoErr)
            csevere << startl << "Error doing the time-saving complex mult (adjusting linear gradient)" << endl;
        }
        break;
    }

    status = vectorMulC_f32(currentsubchannelfreqs, fracsampleerror - recordedfreqclockoffsets[i], subfracsamparg, arraystridelength);
    if(status != vecNoErr) {
      csevere << startl << "Error in frac sample correction, arg generation (sub)!!!" << status << endl;
      exit(1);
    }
    status = vectorMulC_f32(currentstepchannelfreqs, fracsampleerror - recordedfreqclockoffsets[i], stepfracsamparg, numstrides/2);
    if(status != vecNoErr)
      csevere << startl << "Error in frac sample correction, arg generation (step)!!!" << status << endl;

    //status = vectorMulC_f32(currentchannelfreqptr, fracsampleerror - recordedfreqclockoffsets[i], fracmult, recordedbandchannels + 1);
    //if(status != vecNoErr)
    //  csevere << startl << "Error in frac sample correction!!!" << status << endl;

    //sort out any LO offsets (+ fringe rotation if its post-F)
    if(fringerotationorder == 0) { // do both LO offset and fringe rotation
      phaserotation = (averagedelay-integerdelay)*lofreq;
      if(fractionalLoFreq)
        phaserotation += integerdelay*(lofreq-int(lofreq));
      phaserotation -= walltimesecs*recordedfreqlooffsets[i];
      phaserotationfloat = (f32)(-TWO_PI*(phaserotation-int(phaserotation)));
      status = vectorAddC_f32_I(phaserotationfloat, subfracsamparg, arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error in post-f phase rotation addition (+ maybe LO offset correction), sub!!!" << status << endl;
      fracsamprotator[sidebandoffset*recordedbandchannels].re = cos(phaserotationfloat);
      fracsamprotator[sidebandoffset*recordedbandchannels].im = sin(phaserotationfloat);
      //status = vectorAddC_f32_I(phaserotationfloat, fracmult, recordedbandchannels+1);
      //if(status != vecNoErr)
      //  csevere << startl << "Error in post-f phase rotation addition (and maybe LO offset correction)!!!" << status << endl;
    }
    else { //not post-F, must take care of LO offsets if present
      if(recordedfreqlooffsets[i] > 0.0 || recordedfreqlooffsets[i] < 0.0)
      {
        phaserotation = -walltimesecs*recordedfreqlooffsets[i];
        phaserotationfloat = (f32)(-TWO_PI*(phaserotation-int(phaserotation)));
        status = vectorAddC_f32_I(phaserotationfloat, subfracsamparg, arraystridelength);
        if(status != vecNoErr)
          csevere << startl << "Error in LO offset correction (sub)!!!" << status << endl;
        //status = vectorAddC_f32_I(phaserotationfloat, stepfracsamparg, numstrides/2);
        //if(status != vecNoErr)
        //  csevere << startl << "Error in LO offset correction (step)!!!" << status << endl;
        fracsamprotator[sidebandoffset*recordedbandchannels].re = cos(phaserotationfloat);
        fracsamprotator[sidebandoffset*recordedbandchannels].im = sin(phaserotationfloat);
        //status = vectorAddC_f32_I(phaserotationfloat, fracmult, recordedbandchannels+1);
        //if(status != vecNoErr)
        //  csevere << startl << "Error in LO offset correction!!!" << status << endl;
      }
    }

    //create the fractional sample correction array
    status = vectorSinCos_f32(subfracsamparg, subfracsampsin, subfracsampcos, arraystridelength);
    if(status != vecNoErr)
      csevere << startl << "Error in frac sample correction, sin/cos (sub)!!!" << status << endl;
    status = vectorSinCos_f32(stepfracsamparg, stepfracsampsin, stepfracsampcos, numstrides/2);
    if(status != vecNoErr)
      csevere << startl << "Error in frac sample correction, sin/cos (sub)!!!" << status << endl;
    status = vectorRealToComplex_f32(subfracsampcos, subfracsampsin, fracsampptr1, arraystridelength);
    if(status != vecNoErr)
      csevere << startl << "Error in frac sample correction, real to complex (sub)!!!" << status << endl;
    status = vectorRealToComplex_f32(stepfracsampcos, stepfracsampsin, stepfracsampcplx, numstrides/2);
    if(status != vecNoErr)
      csevere << startl << "Error in frac sample correction, real to complex (step)!!!" << status << endl;
    for(int j=1;j<numstrides/2;j++) {
      status = vectorMulC_cf32(fracsampptr1, stepfracsampcplx[j], &(fracsampptr2[(j-1)*arraystridelength]), arraystridelength);
      if(status != vecNoErr)
        csevere << startl << "Error doing the time-saving complex multiplication in frac sample correction!!!" << endl;
    }

    //status = vectorSinCos_f32(fracmult, fracmultsin, fracmultcos, recordedbandchannels + 1);
    //if(status != vecNoErr)
    //  csevere << startl << "Error in frac sample correction!!!" << status << endl; 
    //status = vectorRealToComplex_f32(fracmultcos, fracmultsin, complexfracmult, recordedbandchannels + 1);
    //if(status != vecNoErr)
    //  csevere << startl << "Error in frac sample correction!!!" << status << endl;

    for(int j=0;j<numrecordedbands;j++)
    {
      if(config->matchingRecordedBand(configindex, datastreamindex, i, j))
      {
        indices[count++] = j;
        switch(fringerotationorder) {
          case 0: //post-F
            fftptr = (config->getDRecordedLowerSideband(configindex, datastreamindex, i))?conjfftoutputs[j]:fftoutputs[j];

            //do the fft
            status = vectorFFT_RtoC_f32(&(unpackedarrays[j][nearestsample - unpackstartsamples]), (f32*)fftptr, pFFTSpecR, fftbuffer);
            if(status != vecNoErr)
              csevere << startl << "Error in FFT!!!" << status << endl;
            //fix the lower sideband if required
            if(config->getDRecordedLowerSideband(configindex, datastreamindex, i))
            {
              status = vectorConjFlip_cf32(fftptr, fftoutputs[j], recordedbandchannels + 1);
              if(status != vecNoErr)
                csevere << startl << "Error in conjugate!!!" << status << endl;
            }
            break;
          case 1:
          case 2:
            status = vectorRealToComplex_f32(&(unpackedarrays[j][nearestsample - unpackstartsamples]), NULL, complexunpacked, twicerecordedbandchannels);
            if(status != vecNoErr)
              csevere << startl << "Error in real->complex conversion" << endl;
            status = vectorMul_cf32_I(complexrotator, complexunpacked, twicerecordedbandchannels);
            if(status != vecNoErr)
              csevere << startl << "Error in fringe rotation!!!" << status << endl;
            status = vectorFFT_CtoC_cf32(complexunpacked, fftd, pFFTSpecC, fftbuffer);
            if(status != vecNoErr)
              csevere << startl << "Error doing the FFT!!!" << endl;
            if(config->getDRecordedLowerSideband(configindex, datastreamindex, i)) {
              status = vectorCopy_cf32(&(fftd[recordedbandchannels+1]), &(fftoutputs[j][1]), recordedbandchannels-1);
              fftoutputs[j][recordedbandchannels] = fftd[0];
            }
            else {
              status = vectorCopy_cf32(fftd, fftoutputs[j], recordedbandchannels);
            }
            if(status != vecNoErr)
              csevere << startl << "Error copying FFT results!!!" << endl;
            break;
        }

        /*Ipp32fc touse;
        touse.re = 1.1;
        touse.im = -1.2;
        for(int n=0;n<numrecordedbands;n++)
        {
          status = ippsSet_32fc(touse, fftoutputs[n], recordedbandchannels+1);
          if(status != vecNoErr)
            csevere << startl << "Error trying to zero fftoutputs when data is bad!" << endl;
          status = ippsSet_32fc(touse, conjfftoutputs[n], recordedbandchannels+1);
          if(status != vecNoErr)
            csevere << startl << "Error trying to zero fftoutputs when data is bad!" << endl;
        }
        return dataweight;*/

        //do the frac sample correct (+ fringe rotate if its post-f)
        status = vectorMul_cf32_I(fracsamprotator, fftoutputs[j], recordedbandchannels + 1);
        if(status != vecNoErr)
          csevere << startl << "Error in application of frac sample correction!!!" << status << endl;
        //status = vectorMul_cf32_I(complexfracmult, fftoutputs[j], recordedbandchannels + 1);
        //if(status != vecNoErr)
        //  csevere << startl << "Error in frac sample correction!!!" << status << endl;

        //do the conjugation
        status = vectorConj_cf32(fftoutputs[j], conjfftoutputs[j], recordedbandchannels + 1);
        if(status != vecNoErr)
          csevere << startl << "Error in conjugate!!!" << status << endl;

        //do the autocorrelation (skipping Nyquist channel)
        status = vectorAddProduct_cf32(fftoutputs[j]+sidebandoffset, conjfftoutputs[j]+sidebandoffset, autocorrelations[0][j]+sidebandoffset, recordedbandchannels);
        if(status != vecNoErr)
          csevere << startl << "Error in autocorrelation!!!" << status << endl;

        //Add the weight in magic location (imaginary part of Nyquist channel)
        autocorrelations[0][j][recordedbandchannels*(1-sidebandoffset)].im += dataweight;
      }
    }

    //if we need to, do the cross-polar autocorrelations
    if(calccrosspolautocorrs && count > 1)
    {
      //cinfo << startl << "For frequency " << i << ", datastream " << datastreamindex << " has chosen bands " << indices[0] << " and " << indices[1] << endl; 
      status = vectorAddProduct_cf32(fftoutputs[indices[0]]+sidebandoffset, conjfftoutputs[indices[1]]+sidebandoffset, autocorrelations[1][indices[0]]+sidebandoffset, recordedbandchannels);
      if(status != vecNoErr)
        csevere << startl << "Error in cross-polar autocorrelation!!!" << status << endl;
      status = vectorAddProduct_cf32(fftoutputs[indices[1]]+sidebandoffset, conjfftoutputs[indices[0]]+sidebandoffset, autocorrelations[1][indices[1]]+sidebandoffset, recordedbandchannels);
      if(status != vecNoErr)
        csevere << startl << "Error in cross-polar autocorrelation!!!" << status << endl;
      //add the weight in magic location (imaginary part of Nyquist channel)
      autocorrelations[1][indices[0]][recordedbandchannels*(1-sidebandoffset)].im += dataweight;
      autocorrelations[1][indices[1]][recordedbandchannels*(1-sidebandoffset)].im += dataweight;
    }
  }

  return dataweight;
}

void Mode::zeroAutocorrelations()
{
  for(int i=0;i<autocorrwidth;i++)
  {
    for(int j=0;j<numrecordedbands;j++)
      vectorZero_cf32(autocorrelations[i][j], recordedbandchannels+1);
  }
}

void Mode::setDelays(f64 * d)
{
  //cout << "Mode about to copy delays, length " << delaylength << endl;
  int status = vectorCopy_f64(d, delays, delaylength);
  double s_a, s_b, s_c;
  double subintmicrosec = config->getSubintNS(configindex)/1000.0;
  if(status != vecNoErr)
    csevere << startl << "Error trying to copy delays!!!" << endl;

  //work out the twiddle factors used later to interpolate
  if(fringerotationorder == 2)
  {
    if(!(delays[0] > MAX_NEGATIVE_DELAY) || !(delays[blockspersend/2] > MAX_NEGATIVE_DELAY) || !(delays[blockspersend/2+1] > MAX_NEGATIVE_DELAY) || !(delays[blockspersend] > MAX_NEGATIVE_DELAY))
    {
      datalengthbytes = 0; //torch entire subint - temporary until switch to model
    }
    else
    {
      s_a = delays[0];
      if(blockspersend%2 == 0)
        s_b = delays[blockspersend/2];
      else
        s_b = 0.5*(delays[blockspersend/2]+delays[blockspersend/2 + 1]);
      s_c = delays[blockspersend];

      a0 = (s_c*2.0 - s_b*4.0 + s_a*2.0)/(subintmicrosec*subintmicrosec);
      b0 = (-s_c + s_b*4.0 - s_a*3.0)/(subintmicrosec);
      c0 = s_a - int(s_a);
      quadadd1 = b0/(2*a0);
      quadadd2 = c0 - (b0*b0)/(4*a0);
      if(a0 == 0)
        datalengthbytes = 0; //can't use the quadratic since that would involve divide by 0
    }
  }
}

void Mode::setData(u8 * d, int dbytes, double btime)
{
  data = d;
  datalengthbytes = dbytes;
  bufferseconds = int(btime);
  buffermicroseconds = (btime - int(btime))*1000000.0;
  unpackstartsamples = -999999999;
}

const float Mode::decorrelationpercentage[] = {0.63662, 0.88, 0.94, 0.96, 0.98, 0.99, 0.996, 0.998}; //note these are just approximate!!!


LBAMode::LBAMode(Configuration * conf, int confindex, int dsindex, int recordedbandchan, int bpersend, int gsamples, int nrecordedfreqs, double recordedbw, double * recordedfreqclkoffs, double * recordedfreqlooffs, int nrecordedbands, int nzoombands, int nbits, bool fbank, int fringerotorder, int arraystridelen, bool cacorrs, const s16* unpackvalues)
  : Mode(conf,confindex,dsindex,recordedbandchan,bpersend,gsamples,nrecordedfreqs,recordedbw,recordedfreqclkoffs,recordedfreqlooffs,nrecordedbands,nzoombands,nbits,recordedbandchan*2,fbank,fringerotorder,arraystridelen,cacorrs,(recordedbw<16.0)?recordedbw*2.0:32.0)
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

const s16 LBAMode::stdunpackvalues[] = {MAX_S16/4, -MAX_S16/4 - 1, 3*MAX_S16/4, -3*MAX_S16/4 - 1};
const s16 LBAMode::vsopunpackvalues[] = {-3*MAX_S16/4 - 1, MAX_S16/4, -MAX_S16/4 - 1, 3*MAX_S16/4};
