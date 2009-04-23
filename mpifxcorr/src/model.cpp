/***************************************************************************
 *   Copyright (C) 2009 by Adam Deller                                     *
 *                                                                         *
 *   This program is free for non-commercial use: see the license file     *
 *   at  http://cira.ivec.org/dokuwiki/doku.php/difx/documentation for     *
 *   more details.                                                         *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: $
// $HeadURL: $
// $LastChangedRevision: $
// $Author: $
// $LastChangedDate: $
//
//============================================================================

#include "architecture.h"
#include "configuration.h"
#include "alert.h"
#include "model.h"

Model::Model(Configuration * conf, string modelfilename)
  : config(conf)
{
  ifstream * input = new ifstream(modelfilename.c_str());
  if(!input->is_open() || input->bad()) {
    cfatal << startl << "Error opening model file " << modelfilename << " - aborting!!!" << endl;
    opensuccess = false;
  }

  if(opensuccess) 
    opensuccess = readInfoTable(input);
  if(opensuccess)
    opensuccess = readCommonTable(input);
  if(opensuccess)
    opensuccess = readStationTable(input);
  if(opensuccess)
    opensuccess = readSourceTable(input);
  if(opensuccess)
    opensuccess = readEOPTable(input);
  if(opensuccess)
    opensuccess = readScanTable(input);

  tpowerarray = vectorAlloc_f64(polyorder+1);
}

void Model::interpolateUVW(int scanindex, double offsettime, int antennaindex, int scansourceindex, double* uvw)
{
  int scansample;
  double deltat;
  double * coeffs;

  //work out the correct sample and offset from that sample
  scansample = int(offsettime/double(modelincsecs));
  deltat = offsettime - scansample*modelincsecs;
  tpowerarray[0] = 1.0;
  for(int i=0;i<polyorder;i++)
    tpowerarray[i+1] = tpowerarray[i]*deltat;

  //calculate the uvw values
  coeffs = scantable[scanindex].u[scansample][antennaindex][scansourceindex];
  vectorDotProduct_f64(tpowerarray, coeffs, polyorder+1, &(uvw[0]));
  coeffs = scantable[scanindex].v[scansample][antennaindex][scansourceindex];
  vectorDotProduct_f64(tpowerarray, coeffs, polyorder+1, &(uvw[1]));
  coeffs = scantable[scanindex].w[scansample][antennaindex][scansourceindex];
  vectorDotProduct_f64(tpowerarray, coeffs, polyorder+1, &(uvw[2]));
}

void Model::calculateDelayIntepolator(int scanindex, f64 offsettime, f64 timespan, int numincrements, int antennaindex, int scansourceindex, int order, f64 * delaycoeffs)
{
  int scansample, status;
  double deltat;
  double delaysamples[3];

  //check that order is ok
  if(order < 0 || order > 2) {
    csevere << startl << "Model delay interpolator asked to produce " << order << "th order output - can only do 0, 1 or 2! Setting order to 1, but chaos is likely..." << endl;
    order = 1;
  } 

  //work out the correct sample and offset for the midrange of the timespan
  scansample = int(offsettime/double(modelincsecs));
  deltat = offsettime + timespan/2.0 - scansample*modelincsecs;
  tpowerarray[0] = 1.0;
  for(int i=0;i<polyorder;i++)
    tpowerarray[i+1] = tpowerarray[i]*deltat;
  
  //zero-th order interpolation - the simplest case
  if(order==0) {
    status = vectorDotProduct_f64(tpowerarray, scantable[scanindex].delay[scansample][antennaindex][scansourceindex], polyorder+1, delaycoeffs);
    if (status != vecNoErr)
      cerror << startl << "Error calculating zero-th order interpolation in Model" << endl;
    return; //note return
  }

  //If not 0th order interpolation, need to fill out all 3 spots
  vectorDotProduct_f64(tpowerarray, scantable[scanindex].delay[scansample][antennaindex][scansourceindex], polyorder+1, &(delaysamples[1]));
  if (status != vecNoErr)
    cerror << startl << "Error calculating sample 1 for interpolation in Model" << endl;
  deltat = offsettime - scansample*modelincsecs;
  for(int i=0;i<polyorder;i++)
    tpowerarray[i+1] = tpowerarray[i]*deltat;
  vectorDotProduct_f64(tpowerarray, scantable[scanindex].delay[scansample][antennaindex][scansourceindex], polyorder+1, &(delaysamples[0]));
  if (status != vecNoErr)
    cerror << startl << "Error calculating sample 0 for interpolation in Model" << endl;
  deltat = offsettime + timespan - scansample*modelincsecs;
  for(int i=0;i<polyorder;i++)
    tpowerarray[i+1] = tpowerarray[i]*deltat;
  vectorDotProduct_f64(tpowerarray, scantable[scanindex].delay[scansample][antennaindex][scansourceindex], polyorder+1, &(delaysamples[2]));
  if (status != vecNoErr)
    cerror << startl << "Error calculating sample 2 for interpolation in Model" << endl;
 
  //linear interpolation
  if(order==1) {
    delaycoeffs[0] = (delaysamples[2]-delaysamples[0])/numincrements;
    delaycoeffs[1] = delaysamples[0] + (delaysamples[1] - (delaycoeffs[0]*numincrements/2.0 + delaysamples[0]))/3.0;
    return; //note return
  }

  //quadratic interpolation
  if(order==2) {
    delaycoeffs[0] = (2.0*delaysamples[0]-4.0*delaysamples[1]+2.0*delaysamples[2])/(numincrements*numincrements);
    delaycoeffs[1] = (-3.0*delaysamples[0]+4.0*delaysamples[1]-delaysamples[2])/numincrements;
    delaycoeffs[2] = delaysamples[0];
  }
}

bool Model::readInfoTable(ifstream * input)
{
  string line = "";
  while (!(line.substr(0,21) == "# INFORMATION ######!") && !input->eof())
    getline(*input, line); //skip any whitespace/comments
  //nothing here is worth saving, so just skip it all
  config->getinputline(input, &line, "CALC SERVER");
  config->getinputline(input, &line, "CALC PROGRAM");
  config->getinputline(input, &line, "CALC VERSION");
  config->getinputline(input, &line, "JOB ID");
  config->getinputline(input, &line, "OBSCODE");
}

bool Model::readCommonTable(ifstream * input)
{
  int year, month, day, hour, minute, second;
  string line = "";
  while (!(line.substr(0,21) == "# COMMON SETTINGS ##!") && !input->eof())
    getline(*input, line); //skip any whitespace/comments
  config->getinputline(input, &line, "START YEAR");
  year = atoi(line.c_str());
  config->getinputline(input, &line, "START MONTH");
  month = atoi(line.c_str());
  config->getinputline(input, &line, "START DAY");
  day = atoi(line.c_str());
  config->getinputline(input, &line, "START HOUR");
  hour = atoi(line.c_str());
  config->getinputline(input, &line, "START MINUTE");
  minute = atoi(line.c_str());
  config->getinputline(input, &line, "START SECOND");
  second = atoi(line.c_str());
  config->getMJD(modelmjd, modelstartseconds, year, month, day, hour, minute, second);
  config->getinputline(input, &line, "POLYNOMIAL ORDER");
  polyorder = atoi(line.c_str());
  config->getinputline(input, &line, "INTERVAL (SECS)");
  modelincsecs = atoi(line.c_str());
}
bool Model::readStationTable(ifstream * input)
{
  string line = "";
  while (!(line.substr(0,21) == "# STATION TABLE ####!") && !input->eof())
    getline(*input, line); //skip any whitespace/comments
  config->getinputline(input, &line, "NUM STATIONS");
  numstations = atoi(line.c_str());
  stationtable = new station[numstations];
  for(int i=0;i<numstations;i++) {
    config->getinputline(input, &(stationtable[i].name), "STATION");
    //trim the whitespace off the end
    while((stationtable[i].name).at((stationtable[i].name).length()-1) == ' ')
      stationtable[i].name = (stationtable[i].name).substr(0, (stationtable[i].name).length()-1);
    config->getinputline(input, &line, "STATION");
    stationtable[i].mount = getMount(line);
    config->getinputline(input, &line, "STATION");
    stationtable[i].axisoffset = atoi(line.c_str());
    config->getinputline(input, &line, "STATION");
    stationtable[i].x = atoi(line.c_str());
    config->getinputline(input, &line, "STATION");
    stationtable[i].y = atoi(line.c_str());
    config->getinputline(input, &line, "STATION");
    stationtable[i].z = atoi(line.c_str());
  }
}
bool Model::readSourceTable(ifstream * input)
{
  string line = "";
  while (!(line.substr(0,21) == "# SOURCE TABLE #####!") && !input->eof())
    getline(*input, line); //skip any whitespace/comments
  config->getinputline(input, &line, "NUM SOURCES");
  numsources = atoi(line.c_str());
  sourcetable = new source[numsources];
  for(int i=0;i<numsources;i++) {
    config->getinputline(input, &(sourcetable[i].name), "SOURCE");
    //trim the whitespace off the end
    while((sourcetable[i].name).at((sourcetable[i].name).length()-1) == ' ')
      sourcetable[i].name = (sourcetable[i].name).substr(0, (sourcetable[i].name).length()-1);
    config->getinputline(input, &line, "SOURCE");
    sourcetable[i].ra = atof(line.c_str());
    config->getinputline(input, &line, "SOURCE");
    sourcetable[i].dec = atof(line.c_str());
    config->getinputline(input, &line, "SOURCE");
    sourcetable[i].calcode = line.at(0);
    config->getinputline(input, &line, "SOURCE");
    sourcetable[i].qual = atoi(line.c_str());
  }
}
bool Model::readEOPTable(ifstream * input)
{
  string line = "";
  while (!(line.substr(0,21) == "# EOP TABLE ########!") && !input->eof())
    getline(*input, line); //skip any whitespace/comments
  config->getinputline(input, &line, "NUM EOPS");
  numeops = atoi(line.c_str());
  eoptable = new eop[numeops];
  for(int i=0;i<numeops;i++) {
    config->getinputline(input, &line, "EOP");
    eoptable[i].mjd = atoi(line.c_str());
    config->getinputline(input, &line, "EOP");
    eoptable[i].taiutc = atoi(line.c_str());
    config->getinputline(input, &line, "EOP");
    eoptable[i].ut1utc = atof(line.c_str());
    config->getinputline(input, &line, "EOP");
    eoptable[i].xpole = atof(line.c_str());
    config->getinputline(input, &line, "EOP");
    eoptable[i].ypole = atof(line.c_str());
  }
}
bool Model::readScanTable(ifstream * input)
{
  int sourcestoread, at, next;
  string line = "";
  while (!(line.substr(0,21) == "# SCAN TABLE #######!") && !input->eof())
    getline(*input, line); //skip any whitespace/comments
  config->getinputline(input, &line, "NUM SCANS");
  numscans = atoi(line.c_str());
  scantable = new scan[numscans];
  for(int i=0;i<numscans;i++) {
    config->getinputline(input, &line, "SCAN");
    scantable[i].offsetseconds = atoi(line.c_str());
    config->getinputline(input, &line, "SCAN");
    scantable[i].durationseconds = atoi(line.c_str());
    config->getinputline(input, &(scantable[i].obsmodename), "SCAN");
    config->getinputline(input, &line, "SCAN");
    scantable[i].pointingcentre = &(sourcetable[atoi(line.c_str())]);
    config->getinputline(input, &line, "SCAN");
    scantable[i].numphasecentres = atoi(line.c_str());
    scantable[i].phasecentres = new source*[scantable[i].numphasecentres];
    scantable[i].pointingcentrecorrelated = false;
    sourcestoread = scantable[i].numphasecentres+1;
    for(int j=0;j<scantable[i].numphasecentres;j++) {
      config->getinputline(input, &line, "SCAN");
      scantable[i].phasecentres[j] = &(sourcetable[atoi(line.c_str())]);
      if(scantable[i].phasecentres[j] == scantable[i].pointingcentre) {
        scantable[i].pointingcentrecorrelated = true;
        sourcestoread--;
      }
    }
    config->getinputline(input, &line, "SCAN");
    scantable[i].nummodelsamples = atoi(line.c_str());
    scantable[i].u = new f64***[scantable[i].nummodelsamples];
    scantable[i].v = new f64***[scantable[i].nummodelsamples];
    scantable[i].w = new f64***[scantable[i].nummodelsamples];
    scantable[i].delay = new f64***[scantable[i].nummodelsamples];
    scantable[i].wet = new f64***[scantable[i].nummodelsamples];
    scantable[i].dry = new f64***[scantable[i].nummodelsamples];
    for(int j=0;j<scantable[i].nummodelsamples;j++) {
      scantable[i].u[j] = new f64**[numstations];
      scantable[i].v[j] = new f64**[numstations];
      scantable[i].w[j] = new f64**[numstations];
      scantable[i].delay[j] = new f64**[numstations];
      scantable[i].wet[j] = new f64**[numstations];
      scantable[i].dry[j] = new f64**[numstations];
      for(int k=0;k<numstations;k++) {
        scantable[i].u[j][k] = new f64*[sourcestoread];
        scantable[i].v[j][k] = new f64*[sourcestoread];
        scantable[i].w[j][k] = new f64*[sourcestoread];
        scantable[i].delay[j][k] = new f64*[sourcestoread];
        scantable[i].wet[j][k] = new f64*[sourcestoread];
        scantable[i].dry[j][k] = new f64*[sourcestoread];
        for(int l=0;l<sourcestoread;l++) {
          scantable[i].u[j][k][l] = vectorAlloc_f64(polyorder+1);
          config->getinputline(input, &line, "SCAN");
          at = 0;
          for(int m=0;m<polyorder+1;m++) {
            next = line.find_first_of('\t', at);
            scantable[i].u[j][k][l][m] = atof((line.substr(at, next-at)).c_str());
            at = next+1;
          }
          scantable[i].v[j][k][l] = vectorAlloc_f64(polyorder+1);
          config->getinputline(input, &line, "SCAN");
          at = 0;
          for(int m=0;m<polyorder+1;m++) {
            next = line.find_first_of('\t', at);
            scantable[i].v[j][k][l][m] = atof((line.substr(at, next-at)).c_str());
            at = next+1;
          }
          scantable[i].w[j][k][l] = vectorAlloc_f64(polyorder+1);
          config->getinputline(input, &line, "SCAN");
          at = 0;
          for(int m=0;m<polyorder+1;m++) {
            next = line.find_first_of('\t', at);
            scantable[i].w[j][k][l][m] = atof((line.substr(at, next-at)).c_str());
            at = next+1;
          }
          scantable[i].delay[j][k][l] = vectorAlloc_f64(polyorder+1);
          config->getinputline(input, &line, "SCAN");
          at = 0;
          for(int m=0;m<polyorder+1;m++) {
            next = line.find_first_of('\t', at);
            scantable[i].delay[j][k][l][m] = atof((line.substr(at, next-at)).c_str());
            at = next+1;
          }
          scantable[i].wet[j][k][l] = vectorAlloc_f64(polyorder+1);
          config->getinputline(input, &line, "SCAN");
          at = 0;
          for(int m=0;m<polyorder+1;m++) {
            next = line.find_first_of('\t', at);
            scantable[i].wet[j][k][l][m] = atof((line.substr(at, next-at)).c_str());
            at = next+1;
          }
          scantable[i].dry[j][k][l] = vectorAlloc_f64(polyorder+1);
          config->getinputline(input, &line, "SCAN");
          at = 0;
          for(int m=0;m<polyorder+1;m++) {
            next = line.find_first_of('\t', at);
            scantable[i].dry[j][k][l][m] = atof((line.substr(at, next-at)).c_str());
            at = next+1;
          }
        }
      }
    }
  }
}

//utility routine which returns an integer which FITS expects based on the type of mount
Model::axistype Model::getMount(string mount)
{
  if(mount.compare("azel") == 0 || mount.compare("altz") == 0) //its an azel mount
    return ALTAZ;
    
  if(mount.compare("equa") == 0 || mount.compare("hadec") == 0) //equatorial mount
    return RADEC;
    
  if(mount.compare("orbi") == 0) //orbital mount
    return ORB;
    
  if((mount.substr(0,2)).compare("xy") == 0) //xy mount
    return XY;
    
  //otherwise unknown
  cerror << startl << "Warning - unknown mount type: Assuming Az-El" << endl;
  return ALTAZ;
}
