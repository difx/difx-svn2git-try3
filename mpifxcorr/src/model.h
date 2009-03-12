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
#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <vector>

//forward declaration of Configuration
class Configuration;

using namespace std;

/**
 @class Model
 @brief Container class for geometric model information, in scan-based form

 Contains a list station and source positions, and a list of scans.  Each scan contains a polynomial
 representation of {u,v,w} and delay terms as loaded from the .model file, and allows easy access via 
 interpolation to any point in the experiment

 @author Adam Deller
 */
class Model{
  public:
    /**
     * Constructor: Loads the model information from the supplied file into memory
     * @param config The configuration object, containing all information about correlation setup
     * @param uvwfilename The file to load from
     */
    Model(Configuration * conf, string modelfilename);

    ~Model();

    /**
     * Returns whether the Model file was opened and parsed successfully
     * @return Whether this Model object was successfully created
     */
    inline bool openSuccess() { return opensuccess; }

    /**
     * Returns whether the pointing centre is correlated for this scan
     * @return Whether the pointing centre is correlated for the scan
     */
    inline bool isPointingCentreCorrelated(int scan) { return scantable[scan].pointingcentrecorrelated; }

    /**
     * Returns the delay, in microseconds, for the specified antenna pointing centre
     * at the specified time
     * @return Delay in microseconds
     */
    double getDelay(int scanindex, double offsettime, int antennaindex, int scansourceindex);

    /**
     * Returns the number of phase centres to be correlated for this scan
     * @return The number of phase centres to be correlated for this scan
     */
    inline int getNumPhaseCentres(int scanindex) { return scantable[scanindex].numphasecentres; }

    enum axistype {ALTAZ=0, RADEC=1, ORB=2, XY=3};

  private:
    typedef struct {
      string name;
      double x,y,z,axisoffset;
      axistype mount;
    } station;

    typedef struct {
      string name;
      double ra, dec;
      int qual;
      char calcode;
    } source;

    typedef struct {
      int offsetseconds, durationseconds, nummodelsamples;
      string obsmodename;
      source * pointingcentre;
      int numphasecentres;
      bool pointingcentrecorrelated;
      source ** phasecentres;
      double **** u;
      double **** v;
      double **** w;
      double **** delay;
      double **** wet;
      double **** dry;
    } scan;

    typedef struct {
      int mjd; //days
      int taiutc; //TAI - UTC in seconds
      double ut1utc; //UT1 - UTC in seconds
      double xpole; //offset in arcseconds
      double ypole; //offset in arcseconds
    } eop;

    bool readInfoTable(ifstream * input);
    bool readCommonTable(ifstream * input);
    bool readStationTable(ifstream * input);
    bool readSourceTable(ifstream * input);
    bool readEOPTable(ifstream * input);
    bool readScanTable(ifstream * input);
    axistype getMount(string mount);

    int modelmjd, modelstartseconds, numstations, numsources, numscans, numeops;
    int polyorder, modelincsecs;
    bool opensuccess;
    Configuration * config;
    station * stationtable;
    source * sourcetable;
    eop * eoptable;
    scan * scantable;
};

#endif
