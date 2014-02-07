/***************************************************************************
 *   Copyright (C) 2009-2014 by Walter Brisken                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
/*===========================================================================
 * SVN properties (DO NOT CHANGE)
 *
 * $Id$
 * $HeadURL$
 * $LastChangedRevision$
 * $Author$
 * $LastChangedDate$
 *
 *==========================================================================*/

#ifndef __CORRPARAM_H__
#define __CORRPARAM_H__

#include <string>
#include <set>
#include <vector>
#include <map>
#include <list>
#include <iostream>
#include <difxio.h>

#include "vextables.h"

extern const double MJD_UNIX0;	// MJD at beginning of unix time
extern const double SEC_DAY;
extern const double MUSEC_DAY;

enum V2D_Mode
{
	V2D_MODE_NORMAL = 0,	// for almost all purposes
	V2D_MODE_PROFILE = 1	// to produce pulsar profiles
};

// see http://cira.ivec.org/dokuwiki/doku.php/difx/configuration

class PhaseCentre
{
public:
	//constants
	static const std::string DEFAULT_NAME;
	static const double DEFAULT_RA;
	static const double DEFAULT_DEC;

	//constructors
	PhaseCentre(double ra, double dec, std::string name);
	PhaseCentre();

	//methods
	void initialise(double ra, double dec, std::string name);

	//variables
	double ra;	//radians
	double dec;	//radians
	std::string difxName;
	char calCode;
	int qualifier;
	// ephemeris
        std::string ephemType;  // type of ephemeris (defaults to "" for unknown)
	std::string ephemObject;    // name of the object in the ephemeris
	std::string ephemFile;	    // file containing a JPL ephemeris
        std::string orientationFile;// file containing JPL spacecraft
                                    // orientation data
	std::string naifFile;	    // file containing naif time data
	double ephemDeltaT;	    // tabulated ephem. nterval (seconds, default 24)
	double ephemStellarAber;    // 0 = don't apply (default), 1 = apply, other: scale correction accordingly
	double ephemClockError;	    // (sec) 0.0 is no error
                                // This is the clock error in the ephemeris
                                // providing the position of the spacecraft
        double sc_epoch;  // Epoch to use for calculating position for
                          // correlating the observations for spacecraft and
                          // Solar system objects.  Input string
                          // may be an MJD, ISO 8601, VLBA, or VEX time.
                          // If not specified, the default (0.0) is to
                          // continuously update the position 
                          // throughout the experiment.  
	int gpsId;		    // GPS satellite number [0 means not a GPS satellite]
};

class SourceSetup
{
public:
	SourceSetup(const std::string &name);
	int setkv(const std::string &key, const std::string &value);
	int setkv(const std::string &key, const std::string &value, PhaseCentre * pc);

	bool doPointingCentre;		// Whether or not to correlate the pointing centre
	std::string vexName;		// Source name as appears in vex file
	PhaseCentre pointingCentre;	// The source which is at the pointing centre
	std::vector<PhaseCentre> phaseCentres; // Additional phase centres to be correlated
};

class ZoomFreq
{
public:
	//constructor
	ZoomFreq();

	//method
	void initialise(double freq, double bw, bool corrparent, int specavg);

	//variables
	double frequency, bandwidth;
	int spectralaverage;
	bool correlateparent;
};

class SpacecraftGroundClockBreak
{
public:
    SpacecraftGroundClockBreak()
            : mjd_start(-1), mjd_sync(-1),
              day_fraction_start(-1.0),
              day_fraction_sync(-1.0),
              clock_break_fudge_seconds(0.0) {}
    SpacecraftGroundClockBreak(int mjd_start_, double day_fraction_start_,
                               int mjd_sync_, double day_fraction_sync_,
                               double clock_break_fudge_seconds_)
            : mjd_start(mjd_start_), mjd_sync(mjd_sync_),
              day_fraction_start(day_fraction_start_),
              day_fraction_sync(day_fraction_sync_),
              clock_break_fudge_seconds(clock_break_fudge_seconds_) {}

    int mjd_start, mjd_sync;
    double day_fraction_start, day_fraction_sync;
    double clock_break_fudge_seconds;
};

class GlobalZoom
{
public:
	GlobalZoom(const std::string &name) : difxName(name) {};
	int setkv(const std::string &key, const std::string &value, ZoomFreq * zoomFreq);
	int setkv(const std::string &key, const std::string &value);

	std::string difxName;	// Name in .v2d file of this global zoom band set
	std::vector<ZoomFreq> zoomFreqs;
};

class AntennaSetup
{
public:
	AntennaSetup(const std::string &name);
	int setkv(const std::string &key, const std::string &value);
	int setkv(const std::string &key, const std::string &value, ZoomFreq * zoomFreq);
	void copyGlobalZoom(const GlobalZoom &globalZoom);

	std::string vexName;	// Antenna name as it appears in vex file
	std::string difxName;	// Antenna name (if different) to appear in difx
        std::string calcName;   // Antenna name (if different) to provide to the
                                //     delay model software (CALC)
	double X, Y, Z;		// Station coordinates to override vex
	double axisOffset;	// [m]
	int clockorder;		// Order of clock poly (if overriding)
	double clock2, clock3, clock4, clock5;	// Clock coefficients (if overriding)
	std::vector<double> freqClockOffs; // clock offsets for the individual frequencies
	std::vector<double> freqClockOffsDelta; // clock offsets between pols for the individual frequencies
	std::vector<double> freqPhaseDelta; // Phase difference between pols for each frequency
	std::vector<double> loOffsets; //LO offsets for each individual frequency
	VexClock clock;
	double deltaClock;	// sec
	double deltaClockRate;	// sec/sec
	// flag
	// media
	bool polSwap;		// If true, swap polarizations
	std::string format;	// Override format from .vex file.
				// This is sometimes needed because format not known always at scheduling time
				// Possible values: S2 VLBA MkIV/Mark4 Mark5B . Is converted to all caps on load
	enum DataSource dataSource;
	enum SamplingType dataSampling;
	std::vector<VexBasebandFile> basebandFiles;	// files to correlate
	int networkPort;	// For eVLBI : port for this antenna
	int windowSize;		// For eVLBI : TCP window size
	int phaseCalIntervalMHz;// 0 if no phase cal extraction, positive gives interval between tones to extract
	enum ToneSelection toneSelection;	// Which tones to propagate to FITS
	double toneGuardMHz;	// to avoid getting tones too close to band edges; default = bandwidth/8
	int tcalFrequency;	// Hz (= 80 for VLBA)

	// No more than one of the following can be used at a time:
	std::vector<ZoomFreq> zoomFreqs;//List of zoom freqs to add for this antenna
	std::string globalZoom;	// A reference to a global zoom table

	// antenna-specific start and stop times
	double mjdStart;
	double mjdStop;
	// spacecraft ephemeris
        std::string ephemType;  // type of ephemeris (defaults to "" for unknown)
	std::string ephemObject;// name of the object in the ephemeris
	std::string ephemFile;  // file containing a JPL/Other ephemeris
        std::string orientationFile; // file containing JPL/Other spacecraft
                                // orientation data
	std::string naifFile;   // file containing naif time data
        std::string JPLplanetaryephem;// file containing the JPL planetary ephemeris
	double ephemDeltaT;     // tabulated ephem. interval (seconds, default 24)
	double ephemClockError;	// (sec) 0.0 is no error
                                // This is the clock error in the ephemeris
                                // providing the position of the spacecraft
        std::string spacecraft_time_type; // type of spacecraft clock
                                //     "Local" onboard maser gives timestamp
                                //     "GroundReception" the spacecraft has an
                                //         onboard maser to drive the sampler,
                                //         but the ground station recorder marks
                                //         a timestamp at the time of reception
                                //         at some point during the recording.
                                //     "GroundClock" the spacecraft sampler is
                                //         driven by a continuous clock signal
                                //         transmitted from the ground station.
        std::vector<SpacecraftGroundClockBreak> spacecraft_ground_clock_recording_breaks;
                                // List of clock break times for the
                                //      "GroundReception" time type for
                                //      the spacecraft timekeeping.  This should
                                //      be a string of the form
                                //      start@YYYYyDDDdHHhMMmSS.SSSSSSs/sync@YYYYyDDDdHHhMMmSS.SSSSSSs/clockfudge@SS.SSSSSSSSS
                                //      such as
                                //      start@2011y335d15h30m00s/sync@2011y335d15h30m00s/clockfudge@0.0E0
                                //      The first time part gives the start time
                                //      for which the new clock information is
                                //      valid.  The second time part gives the
                                //      instant at which the recorder syncs the
                                //      time between the ground station and
                                //      the spacecraft.  The third part gives
                                //      an additional ground station recording
                                //      time offset between the actual recording
                                //      time and the indicated time (extra
                                //      seconds that the indicated time is late)
                                //      in units of seconds.
        double SC_recording_delay; // This is the time between reception of the
                                // wavefront at the astronomical antenna
                                // phase center and the
                                // transmission of the data by the
                                // spacecraft to the ground station, in s.
                                // If the spacecraft_time_type is local
                                // (timestamp from its own local clock),
                                // then this is not used.  The regular
                                // clock offset should be used instead. 
        // spacecraft ground station (GS) information
        bool GS_exists;         // Is there a ground station for this antenna?
    	std::string GS_Name;    // Ground station name
        std::string GS_difxName;// Ground station name (if different) to appear in difx
        std::string GS_calcName;// Ground station name (if different) to provide to the
                                //     delay model software (CALC)
	double GS_X, GS_Y, GS_Z;// Ground station coordinates [m]
        double GS_dX, GS_dY, GS_dZ;// Ground station position velocity [m/s]
                                //     Note that the velocity is provided in the
                                //     *.v2d file in units of [m/yr]
        double GS_pos_epoch;    // Epoch [mjd] for which the ground station
                                //     position is valid
	std::string GS_axisType;
        double GS_axisOffset0,GS_axisOffset1,GS_axisOffset2;	// (m)
	int GS_clockorder;	// Order of GS clock poly
	double GS_clock0;	// GS clock offset (sec)
	double GS_clock1;	// GS clock rate (sec/sec)
	double GS_clock2, GS_clock3, GS_clock4, GS_clock5;	// GS clock coefficients
        double GS_clockEpoch;   // GS clock epoch (MJD)
        int SC_pos_offset_refmjd; // Reference MJD for the spacecraft
                                // position offset information
        double SC_pos_offset_reffracDay; /* Reference MJD fractional day
                                // for the spacecraft
                                // position offset information */
        int SC_pos_offsetorder; // Order of SC pos offset poly
        simple3Vector SC_pos_offset0; // spacecraft position offset poly (m)
        simple3Vector SC_pos_offset1; // spacecraft position offset poly (m/s^1)
        simple3Vector SC_pos_offset2; // spacecraft position offset poly (m/s^2)
        simple3Vector SC_pos_offset3; // spacecraft position offset poly (m/s^3)
        simple3Vector SC_pos_offset4; // spacecraft position offset poly (m/s^4)
        simple3Vector SC_pos_offset5; // spacecraft position offset poly (m/s^5)
};

class CorrSetup
{
public:
	CorrSetup(const std::string &name = "setup_default");
	int setkv(const std::string &key, const std::string &value);
	bool correlateFreqId(int freqId) const;
	double bytesPerSecPerBLPerBand() const;
	int checkValidity() const;

	double getMinRecordedBandwidth() const { return minRecordedBandwidth; }
	double getMaxRecordedBandwidth() const { return maxRecordedBandwidth; }
	void addRecordedBandwidth(double bw);
	int nInputChans(double bw) const { return static_cast<int>(bw / FFTSpecRes + 0.5); }
	int nOutputChans(double bw) const { return static_cast<int>(bw / outputSpecRes + 0.5); }
	int minInputChans() const { return static_cast<int>(getMinRecordedBandwidth() / FFTSpecRes + 0.5); }
	int maxInputChans() const { return static_cast<int>(getMaxRecordedBandwidth() / FFTSpecRes + 0.5); }
	int minOutputChans() const { return static_cast<int>(getMinRecordedBandwidth() / outputSpecRes + 0.5); }
	int maxOutputChans() const { return static_cast<int>(getMaxRecordedBandwidth() / outputSpecRes + 0.5); }
	int testSpectralResolution() const;
	int testXMACLength() const;
	int testStrideLength() const;
	int specAvg() const;


	std::string corrSetupName;

	bool explicitXmacLength;// Whether the xmacLength parameter was explicitly set
	bool explicitStrideLength;// Whether the strideLength parameter was explicitly set
	bool explicitFFTSpecRes;// Whether .v2d set the resolution of FFTs
	bool explicitOutputSpecRes; // Whether .v2d set the output resolution
	bool explicitGuardNS;	// Whether the guardNS parameter was explicitly set
	double tInt;		// integration time
	bool doPolar;		// false for no cross pol, true for full pol
	bool doAuto;		// write autocorrelations
        bool doMSAcalibration;  // calculate the mount-source angle (parallactic
                                // angle for on-axis sources with traditional
                                // telescopes) correction and apply this in the
                                // FITS (delay) model components (MC) table
                                // output during conversion to FITS.
        double MC_table_output_interval; // The time interval, in seconds, at
                                // which to report the (delay) model component
                                // (MC table) values in the output FITS files.
                                // The default value of 0.0 results in
                                // the tabulated values occuring at polyInterval
                                // seconds (defualts to
                                // DIFXIO_DEFAULT_POLY_INTERVAL).  Note that in
                                // any case, the interval will be no longer than
                                // polyInterval seconds.
	int subintNS;		// Duration of a subintegration in nanoseconds
	int guardNS;		// Number of "guard" ns tacked on to end of a send
	double FFTSpecRes;	// Hz; resolution of initial FFTs
	double outputSpecRes;	// Hz; resolution of averaged output FFTs
	double suppliedSpecAvg;	// specAvg supplied by .v2d file
	int nFFTChan;		// This and the next parameter can be used to override the above two if all channels are the same width
	int nOutputChan;	//
	int maxNSBetweenUVShifts; //Mostly for multi-phase centres
	int maxNSBetweenACAvg;	// Mostly for sending STA dumps
	int fringeRotOrder;	// 0, 1, or 2
	int strideLength;	// The number of channels to do at a time
				// when fringeRotOrder > 0
	int xmacLength;		// Number of channels to do at a time when xmac'ing
	int numBufferedFFTs;	// Number of FFTs to do in Mode before XMAC'ing
	std::set<int> freqIds;	// which bands to correlate
	std::string binConfigFile;
	std::string phasedArrayConfigFile;
	char onlyPol;		// which polarization to correlate
private:
	void addFreqId(int freqId);

	std::set<double> recordedBandwidths;	// Hz; list of all unique recorded bandwidths using this setup
	double minRecordedBandwidth;		// Hz; cached by addRecordedBandwidth
	double maxRecordedBandwidth;		// Hz; ""
};


class CorrRule
{
public:
	CorrRule(const std::string &name = "rule_default");

	int setkv(const std::string &key, const std::string &value);
	bool match(const std::string &scan, const std::string &source, const std::string &mode, char cal, int qual) const;

	std::string ruleName;

	std::list<std::string> scanName;
	std::list<std::string> sourceName;
	std::list<std::string> modeName;
	std::list<char> calCode;
	std::list<int> qualifier;

	std::string corrSetupName;	/* pointer to CorrSetup */
};

class CorrParams : public VexInterval
{
public:
	CorrParams();
	CorrParams(const std::string &fileName);
	int checkSetupValidity();

	int loadShelves(const std::string &fileName);
	const char *getShelf(const std::string &vsn) const;

	int setkv(const std::string &key, const std::string &value);
	int load(const std::string &fileName);
	void defaults();
	void defaultSetup();
	void defaultRule();
	void example();
	int sanityCheck();
	void addSourceSetup(const SourceSetup &toAdd);

	bool useAntenna(const std::string &antName) const;
	bool useBaseline(const std::string &ant1, const std::string &ant2) const;
	bool swapPol(const std::string &antName) const;
	const CorrSetup *getCorrSetup(const std::string &name) const;
	CorrSetup *getNonConstCorrSetup(const std::string &name);
	const SourceSetup *getSourceSetup(const std::string &name) const;
	const SourceSetup *getSourceSetup(const std::vector<std::string> &names) const;
	const PhaseCentre *getPhaseCentre(const std::string &difxname) const;
	const AntennaSetup *getAntennaSetup(const std::string &name) const;
        const AntennaSetup *getAntennaSetupExact(const std::string &name) const;
	const GlobalZoom *getGlobalZoom(const std::string &name) const;
        const VexClock *getAntennaClock(const std::string &antName) const;
	const VexClock *getAntennaGSClock(const std::string &antName) const;

	const std::string &findSetup(const std::string &scan, const std::string &source, const std::string &mode, char cal, int qual) const;
	const std::string &getNewSourceName(const std::string &origName) const;
	
	/* global parameters */
	int parseWarnings;
	std::string vexFile;
	std::string threadsFile;
	unsigned int minSubarraySize;
	double maxGap;		// days
	bool singleScan;
	bool fakeDatasource;	// if true, configure all datasources as FAKE
	bool singleSetup;
	bool allowOverlap;
	bool mediaSplit;	// split jobs on media change
	bool padScans;
	bool simFXCORR;		// set integration and start times to match VLBA HW correlator
	bool tweakIntTime;	// nadger the integration time to make values nice
        int DelayPolyOrder;     // sets delay polynomial order
        int DelayPolyInterval;  // [s] sets length of indivudal delay polynomial
	int nCore;
	int nThread;
	double maxLength;	// [days]
	double minLength;	// [days]
	double maxSize;		// [bytes] -- break jobs for output filesize
	std::string jobSeries;	// prefix name to job files
	int startSeries;	// start job series at this number
	int dataBufferFactor;
	int nDataSegments;
	int maxReadSize;	// Max (Bytes) amount of data to read into datastream at a time 
	int minReadSize;	// Min (Bytes) amount of data to read into datastream at a time
	unsigned int invalidMask;
	int visBufferLength;
	int overSamp;		// A user supplied override to oversample factor
	enum OutputFormatType outputFormat; // DIFX or ASCII

	std::list<std::string> antennaList;
	std::list<std::pair<std::string,std::string> > baselineList;

	/* manual forced job breaks */
	std::vector<double> manualBreaks;

	/* setups to apply */
	std::vector<CorrSetup> corrSetups;

	/* source setups to apply */
	std::vector<SourceSetup> sourceSetups;

	/* manually provided EOPs */
	std::vector<VexEOP> eops;

	/* antenna setups to apply */
	std::vector<AntennaSetup> antennaSetups;

	/* rules to determine which setups to apply */
	std::vector<CorrRule> rules;

	/* global zoom bands (referenced from a setup; applies to all antennas) */
	std::vector<GlobalZoom> globalZooms;

	enum V2D_Mode v2dMode;

	std::list<std::string> machines;	// List of computers for generation of .machines file

private:
	void addAntenna(const std::string &antName);
	void addBaseline(const std::string &baselineName);
	std::map<std::string,std::string> shelves;
};

std::ostream& operator << (std::ostream &os, const CorrSetup &x);
std::ostream& operator << (std::ostream &os, const CorrRule &x);
std::ostream& operator << (std::ostream &os, const CorrParams &x);

bool areCorrSetupsCompatible(const CorrSetup *A, const CorrSetup *B, const CorrParams *C);

#endif
