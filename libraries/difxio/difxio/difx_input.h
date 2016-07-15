/***************************************************************************
 *   Copyright (C) 2007-2015 by Walter Brisken & Adam Deller               *
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

#ifndef __DIFX_INPUT_H__
#define __DIFX_INPUT_H__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS // For non-compliant C++ compilers
#endif
#include <inttypes.h>
#include <string.h>
#include "parsedifx.h"

#ifdef __cplusplus
extern "C" {
#endif


// #define malloc jma_malloc_malloc
// #define free(p) jma_malloc_free((p), __FILE__, __LINE__)
// #define calloc jma_malloc_calloc
// #define realloc(nmemb,size) jma_malloc_realloc((nmemb),(size), __FILE__, __LINE__)
// #define strdup(s) jma_malloc_strdup((s))
// #define strndup(s,n) jma_malloc_strndup((s),(n))
extern void* jma_malloc_malloc(size_t size);
extern void jma_malloc_free(void *ptr, const char* file, int line);
extern void *jma_malloc_calloc(size_t nmemb, size_t size);
extern void *jma_malloc_realloc(void *ptr, size_t size, const char* file, int line);
extern char *jma_malloc_strdup(const char *s);

extern char *jma_malloc_strndup(const char *s, size_t n);




#define MAX_MODEL_ORDER                            5
#define MAX_PHS_CENTRES                         1000
#define MAX_ABER_CORR_STRING_LENGTH               16
#define MAX_PERFORM_DIRECTION_DERIVATIVE_STRING_LENGTH 16
#define MAX_DATA_SOURCE_NAME_LENGTH               16
#define MAX_ANTENNA_MOUNT_NAME_LENGTH              8
#define MAX_ANTENNA_SITE_NAME_LENGTH              16
#define MAX_SPACECRAFT_TIME_NAME_LENGTH           32
#define MAX_SAMPLING_NAME_LENGTH                  16
#define MAX_DELAY_SERVER_NAME_LENGTH              32
#define MAX_TONE_SELECTION_STRING_LENGTH          12
#define MAX_EOP_MERGE_MODE_STRING_LENGTH	      16
#define MAX_PHASED_ARRAY_TYPE_STRING_LENGTH	      16
#define MAX_PHASED_ARRAY_FORMAT_STRING_LENGTH	  16
#define MAX_TAPER_FUNCTION_STRING_LENGTH          16
#define MAX_SOURCE_COORDINATE_FRAME_STRING_LENGTH 32

#define DIFXIO_FILENAME_LENGTH      256
#define DIFXIO_NAME_LENGTH           32
#define DIFXIO_FORMAT_LENGTH        128
#define DIFXIO_CALCODE_LENGTH         4
#define DIFXIO_VERSION_LENGTH        64
#define DIFXIO_HOSTNAME_LENGTH       32
#define DIFXIO_OBSCODE_LENGTH         8
#define DIFXIO_SESSION_LENGTH         8
#define DIFXIO_TAPER_LENGTH           8
#define DIFXIO_SHELF_LENGTH           8
#define DIFXIO_RX_NAME_LENGTH         8
#define DIFXIO_ETH_DEV_SIZE          12

#define DIFXIO_POL_R            0x01
#define DIFXIO_POL_L            0x02
#define DIFXIO_POL_X            0x10
#define DIFXIO_POL_Y            0x20
#define DIFXIO_POL_ERROR        0x100
#define DIFXIO_POL_RL           (DIFXIO_POL_R | DIFXIO_POL_L)
#define DIFXIO_POL_XY           (DIFXIO_POL_X | DIFXIO_POL_Y)

#define DIFXIO_MAX_EOP_PER_FITS		         6

#define DIFXIO_DEFAULT_POLY_ORDER            5
#define DIFXIO_DEFAULT_POLY_INTERVAL       120
#define DIFXIO_SPACECRAFT_MAX_POLY_ORDER     6     
#define DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER 4 /* should be even ??? */
#if(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER>DIFXIO_SPACECRAFT_MAX_POLY_ORDER)
#  error "Bad DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER"
#endif
#define DIFXIO_DEFAULT_DELAY_MODEL_PRECISION 1E-13 /* 0.1 ps */
#define DIFXIO_DEFAULT_DELTA_LMN   1E-4
#define DIFXIO_DEFAULT_DELTA_XYZ (-1E-4)
#define DIFXIO_DELTA_LMN_N_FACTOR  10.0  /* Multiply \Delta LMN to get larger
                                            radial distance effect in delay
                                            difference
                                         */
#ifndef MICROSECONDS_PER_SECOND
#    define MICROSECONDS_PER_SECOND 1.0E6
#endif
#ifndef SECONDS_PER_MICROSECOND
#    define SECONDS_PER_MICROSECOND 1.0E-6
#endif
#ifndef SPEED_LIGHT
#    define SPEED_LIGHT 299792458.0      /* m/s */
#endif
#ifndef SEC_DAY
#    define SEC_DAY 86400
#endif
#ifndef SEC_DAY_DBL
#    define SEC_DAY_DBL 86400.0
#endif
#ifndef MJD_AT_UNIX_TIME_ORIGIN
#    define MJD_AT_UNIX_TIME_ORIGIN 40587/* The MJD value at Unix time 0 */
#    define MJD_AT_2000_01_01 51544      /* The MJD value at 2000-01-01T00:00:00 */
#    define MJD_AT_J2000 51544.5         /* The MJD value at 2000-01-01T12:00:00 */
#    define JD_AT_J2000 2451545          /* The JD value at 2000-01-01T12:00:00 */
#endif

                                                  




/* Notes about antenna numbering
 *
 * antennaId will typically refer to the index to the DifxInput array called
 * antenna[].  In general, this list will not be in the same order as the
 * antennas as listed in .input file TELESCOPE tables due to both sorting
 * and combining of multiple jobs.
 *
 * Some arrays take as indicies the original antenna index as found in .input
 * files.  These are:  DifxConfig.baselineFreq2IF[][][] and
 * DifxConfig.ant2dsId[][]
 */

/* keep this current with aberCorrStrings in difx_job.c */
enum AberCorr
{
    AberCorrUncorrected = 0,
    AberCorrApproximate,
    AberCorrExact,
    AberCorrNoAtmos,
	AberCorrMixed,		/* output may have more than one aberration correction applied */
    NumAberCorrOptions  /* must remain as last entry */
};

extern const char aberCorrStrings[][MAX_ABER_CORR_STRING_LENGTH];
#define DIFXIO_DEFAULT_ABER_CORR_TYPE AberCorrExact


/* keep this current with performDirectionDerivativeTypeNames in difx_job.c */
enum PerformDirectionDerivativeType
{
    PerformDirectionDerivativeNone = 0,
    PerformDirectionDerivativeUnknown,
    PerformDirectionDerivativeDefault,
    PerformDirectionDerivativeFirstDerivative,
    PerformDirectionDerivativeFirstDerivative2,
    PerformDirectionDerivativeSecondDerivative,
    PerformDirectionDerivativeSecondDerivative2,
    NumPerformDirectionDerivativeType  /* must remain as last entry */
};

extern const char performDirectionDerivativeTypeNames[][MAX_PERFORM_DIRECTION_DERIVATIVE_STRING_LENGTH];


/* keep this current with datastreamTypeNames in difx_datastream.c */
enum DataSource
{
    DataSourceNone = 0,
    DataSourceModule,
    DataSourceFile,
    DataSourceNetwork,
    DataSourceFake,
    NumDataSources      /* must remain as last entry */
};

extern const char dataSourceNames[][MAX_DATA_SOURCE_NAME_LENGTH];


/* keep this current with samplingTypeNames in difx_datastream.c */
enum SamplingType
{
    SamplingReal = 0,
    SamplingComplex,    /* "standard" complex sampling: separate quanization of real and imag */
    SamplingComplexDSB, /* Complex double sideband sampling */
    NumSamplingTypes    /* must remain as last entry */
};

extern const char samplingTypeNames[][MAX_SAMPLING_NAME_LENGTH];


/* keep this current with antennaMountTypeNames in difx_antenna.c */
/* Note that the numbering scheme is based on the FITS-IDI defs, but with XYNS added at end */
/* See AIPS memo 114 for the list of mount types */
enum AntennaMountType
{
    AntennaMountAltAz = 0,
    AntennaMountEquatorial = 1,
    AntennaMountOrbiting = 2,   /* note: uncertain calc support */
    AntennaMountXYEW = 3,       /* Hobart is the prime example */
    AntennaMountNasmythR = 4,   /* note: in calcserver, falls back to azel as is appropriate */
    AntennaMountNasmythL = 5,   /* note: in calcserver, falls back to azel as is appropriate */
    AntennaMountXYNS = 6,       /* note: no FITS-IDI/AIPS support */
    AntennaMountOther = 7,      /* set to this if different from the others */
    NumAntennaMounts        /* must remain as last entry */
};

/* keep this current with antennaSiteTypeNames in difx_antenna.c */
enum AntennaSiteType
{
    AntennaSiteFixed = 0,
    AntennaSiteEarth_Orbiting = 1,
    AntennaSiteOther = 2,
    NumAntennaSites     /* must remain as last entry */
};

/* keep this current with spacecraftTimeTypeNames in difx_spacecraft.c */
enum SpacecraftTimeType
{
    SpacecraftTimeLocal = 0,               /* Onboard clock */
    SpacecraftTimeGroundReception = 1,     /* Onboard clock performs sampling
                                              (determines effective clock rate)
                                              but the time of arrival at the
                                              ground station at some specified
                                              instant determines the clock
                                              offset
                                           */
    SpacecraftTimeGroundClock = 2,         /* The ground station sends a
                                              continuous clock signal to the
                                              spacecraft, which is used to
                                              write the absolute timestamp
                                              on the data and sets the sampling
                                              rate.
                                           */
    SpacecraftTimeGroundClockReception = 3,/* The ground station sends a
                                              continuous clock signal to the
                                              spacecraft, which is used to
                                              set the sampling rate.  The
                                              absolute clock offset is
                                              determined by the reception of
                                              the communications transmission
                                              to the ground and the arrival
                                              at some specified instant at the
                                              ground station.
                                           */
    SpacecraftTimeOther = 4,
    NumSpacecraftTimes      /* must remain as last entry */
};


enum OutputFormatType
{
    OutputFormatDIFX = 0,
    OutputFormatASCII = 1,
    NumOutputFormat         /* must remain as last entry */
};

extern const char antennaMountTypeNames[][MAX_ANTENNA_MOUNT_NAME_LENGTH];
extern const char antennaSiteTypeNames[][MAX_ANTENNA_SITE_NAME_LENGTH];
extern const char spacecraftTimeTypeNames[][MAX_SPACECRAFT_TIME_NAME_LENGTH];


/* keep this current with toneSelectionNames[] in difx_input.c */
enum ToneSelection
{
	ToneSelectionVex = 0,		/* trust the vex file	[default] */
	ToneSelectionNone,		/* Don't pass any tones along */
	ToneSelectionEnds,		/* send along two tones at edges of the band */
	ToneSelectionAll,		/* send along all tones */
	ToneSelectionSmart,		/* like Ends, but try to stay toneGuard MHz away from band edges */
	ToneSelectionMost,		/* all except those within toneGuard */
	ToneSelectionUnknown,		/* an error condition */

	NumToneSelections		/* needs to be at end of list */
};

extern const char toneSelectionNames[][MAX_TONE_SELECTION_STRING_LENGTH];



/* keep this current with delayServerTypeNames in difx_job.c */
enum DelayServerType
{
	UnknownServer = 0,  /* Default to not knowning anything */
	CALCServer,         /* CALC 9.1, original delay server */
    CALC_9_1_RA_Server, /* CALC 9.1 hacked for space VLBI */
    CALCDERIV,          /* Alternative derivative computation program */
    NumDelayServerTypes /* must remain as last entry */
};
extern const char delayServerTypeNames[][MAX_DELAY_SERVER_NAME_LENGTH];
extern const unsigned long delayServerTypeIds[];
/* keep this current with delayServerTypeNames in difx_job.c */
enum DelayServerHandlerType
{
	UnknownDelayServerHandler = 0,  /* Default to not knowning anything */
    DelayServerHandler_DiFX_Delay_Server,
	DelayServerHandler_Pipe,        /* Use multithreaded calls to piped servers */
	                                /* DiFX_Delay_Server RPC calls */
    NumDelayServerHandlerTypes      /* must remain as last entry */
};
extern const char delayServerHandlerTypeNames[][MAX_DELAY_SERVER_NAME_LENGTH];
extern const unsigned long delayServerHandlerTypeIds[];
#define DIFXIO_DEFAULT_DELAY_SERVER_TYPE CALCServer
#define DIFXIO_DEFAULT_DELAY_SERVER_VERSION 0
#define DIFXIO_DEFAULT_DELAY_SERVER_HANDLER_TYPE DelayServerHandler_DiFX_Delay_Server

/* keep this current with sourceCoordinateFrameTypeNames in difx_source.c */
/* See DiFX_Delay_Server.x for more information */
enum SourceCoordinateFrameType
{
    SourceCoordinateFrameUnknown = 0,
    SourceCoordinateFrameDefault,
    SourceCoordinateFrameJ2000,
    SourceCoordinateFrameJ2000_CMB,
    SourceCoordinateFrameJ2000_CMB_1,
    SourceCoordinateFrameJ2000_MWB,
    SourceCoordinateFrameJ2000_MWB_1,
    SourceCoordinateFrameJ2000_SSB,
    SourceCoordinateFrameJ2000_Earth,
    SourceCoordinateFrameITRF2008,
    NumSourceCoordinateFrames  /* must remain as last entry */
};
extern const char sourceCoordinateFrameTypeNames[][MAX_SOURCE_COORDINATE_FRAME_STRING_LENGTH];
#define DIFXIO_DEFAULT_STATION_COORDINATE_FRAME SourceCoordinateFrameITRF2008
#define DIFXIO_DEFAULT_SOURCE_COORDINATE_FRAME SourceCoordinateFrameJ2000


/* See AIPS Memo 117, \S 4.4 AIPS CT Calc table */
typedef struct
{
    double accelgrv;        /* acceleration of gravity at Earth's surface
                               in m s^{-2} */
    double e_flat;          /* Earth's flattening factor, unitless.
                               This is the square of the eccentricity
                               of the ellipsoid which approximates the shape
                               of the Earth.*/
    double earthrad;        /* Earth's equatorial radius, in m */
    double mmsems;          /* ratio of the mass of the Moon to the mass
                               of the Earth */
    double ephepoc;         /* coordinate equinox (usually 2000.0) */
    double gauss;           /* Gaussian gravitational constant in
                               AU^{3/2} day^{-1} M_\Sun^{-1/2} */
    double u_grv_cn;        /* constant of gravitation, in
                               m^3 kg^{-1} s^{-2} */
    double gmsun;           /* Heliocentric gravitational constant,
                               mass of the Sun times the Newtonian graviational
                               constant, in m^3 s^{-2} */
    double gmmercury;       /* Mass of Mercury times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmvenus;         /* Mass of Venus times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmearth;         /* Mass of Earth times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmmoon;          /* Lunar-centric gravitational constant,
                               mass of the Moon times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmmars;          /* Mass of Mars times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmjupiter;       /* Mass of Jupiter times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmsaturn;        /* Mass of Saturn times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmuranus;        /* Mass of Uranus times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double gmneptune;       /* Mass of Neptune times the Newtonian graviational
                               constant, in m^3 s^{-3} */
    double etidelag;        /* lag angle of Earth tides, in radians */
    double love_h;          /* Earth tides: global Love Number H */
    double love_l;          /* Earth tides: global Love Number L */
    double pre_data;        /* general precession in longitude at standard
                               equinox J2000, in arcseconds per Julian century */
    double rel_data;        /* Post-Newtonian expansion parameter */
    double tidalut1;        /* ??? */
    double au;              /* size of an astronomical unit, in m */
    double tsecau;          /* light travel time for 1 astronomical unit, in s */
    double vlight;          /* speed of light, in m s^{-1} */
} DifxCalcParamTable;


/* keep this current with eopMergeModeNames[] in difx_eop.c */
enum EOPMergeMode
{
	EOPMergeModeUnspecified = 0,
	EOPMergeModeStrict,		/* here only allow merging if EOP sets match exactly */
	EOPMergeModeRelaxed,		/* here allow non-contradictory EOP sets to be merged as long as common days have identical values */

	NumEOPMergeModes		/* must remain as last entry */
};

extern const char eopMergeModeNames[][MAX_EOP_MERGE_MODE_STRING_LENGTH];


/* keep this current with phasedArrayOutputTypeNames[] in difx_phasedarray.c */
enum PhasedArrayOutputType
{
	PhasedArrayOutputTypeFilterBank = 0,
	PhasedArrayOutputTypeTimeSeries,

	NumPhasedArrayOutputTypes	/* must remain as last entry */
};

extern const char phasedArrayOutputTypeNames[][MAX_PHASED_ARRAY_TYPE_STRING_LENGTH];


/* keep this current with phasedArrayOutputFormatNames[] in difx_phasedarray.c */
enum PhasedArrayOutputFormat
{
	PhasedArrayOutputFormatDIFX = 0,
	PhasedArrayOutputFormatVDIF,

	NumPhasedArrayOutputFormats	/* must remain as last entry */
};

extern const char phasedArrayOutputFormatNames[][MAX_PHASED_ARRAY_FORMAT_STRING_LENGTH];


/* keep this current with taperFunctionNames in difx_job.c */
enum TaperFunction
{
	TaperFunctionUniform = 0,

	NumTaperFunctions		/* must remain as last entry */
};

extern const char taperFunctionNames[][MAX_TAPER_FUNCTION_STRING_LENGTH];


/* Straight from DiFX frequency table */
typedef struct
{
    double freq;        /* (MHz) */
    double bw;          /* (MHz) */
    char sideband;      /* U or L -- net sideband */
    int nChan;
    int specAvg;        /* This is averaging within mpifxcorr  */
    int overSamp;
    int decimation;
    int nTone;          /* Number of pulse cal tones */
    int *tone;          /* Array of tone indices */
    char rxName[DIFXIO_RX_NAME_LENGTH];
} DifxFreq;

/* To become a FITS IF */
typedef struct
{
    double freq;        /* (MHz) */
    double bw;          /* (MHz) */
    char sideband;      /* U or L -- net sideband */
    int nPol;           /* 1 or 2 */
    char pol[2];        /* polarization codes (one per nPol) : L R X or Y. */
    char rxName[DIFXIO_RX_NAME_LENGTH];
} DifxIF;

typedef struct
{
    char fileName[DIFXIO_FILENAME_LENGTH];  /* filename containing polyco data */
    double dm;          /* pc/cm^3 */
    double refFreq;     /* MHz */
    double mjd;         /* center time for first polynomial */
    int nCoef;          /* number of coefficients per polynomial */
    int nBlk;           /* number of minutes spanned by each */
    double p0;          /* reference phase */
    double f0;          /* reference spin frequency */
    double *coef;
} DifxPolyco;

typedef struct
{
    char fileName[DIFXIO_FILENAME_LENGTH];  /* pulsar config filename */
    int nPolyco;        /* number of polyco structures in file */
    DifxPolyco *polyco; /* individual polyco file contents */
    int nBin;           /* number of pulsar bins */
    double *binEnd;     /* [bin] end phase [0.0, 1.0) of bin */
    double *binWeight;  /* [bin] weight to apply to bin */
    int scrunch;        /* 1 = yes, 0 = no */
} DifxPulsar;

typedef struct
{
	char fileName[DIFXIO_FILENAME_LENGTH];		/* Phased array config filename */
	enum PhasedArrayOutputType outputType;		/* FILTERBANK or TIMESERIES */
	enum PhasedArrayOutputFormat outputFormat;	/* DIFX or VDIF */
	double accTime;		/* Accumulation time in ns for phased array output */
	/* FIXME: below should be part of an enum */
	int complexOutput;	/* 1=true (complex output), 0=false (real output) */
	int quantBits;		/* Bits to re-quantise to */
} DifxPhasedArray;

/* From DiFX config table, with additional derived information */
typedef struct
{
    char name[DIFXIO_NAME_LENGTH];  /* name for configuration */
    double tInt;        /* integration time (sec) */
    int subintNS;       /* Length of a subint in nanoseconds */
    int guardNS;        /* "Guard" nanoseconds appended to the end of a send */
    int fringeRotOrder; /* 0, 1 or 2 */
    int strideLength;   /* Must be integer divisor of number of channels */
    int xmacLength;     /* Must be integer divisor of number of channels */
    int numBufferedFFTs;/* The number of FFTs to do in a row before XMAC'ing */
    int pulsarId;       /* -1 if not pulsar */
    int phasedArrayId;  /* -1 if not phased array mode */
    int nPol;           /* number of pols in datastreams (1 or 2) */
    char pol[2];        /* the polarizations */
    int polMask;        /* bit field using DIFX_POL_x from above */
    int doPolar;        /* >0 if cross hands to be correlated */
    int doAutoCorr;     /* >0 if autocorrelations are to be written to disk */
    int doMSAcalibration;   /* calculate the mount-source angle
                               (parallactic angle for on-axis
                               sources with traditional
                               telescopes) correction and apply
                               this in the FITS (delay) model components
                               (MC) table output during conversion
                               to FITS.  Defaults to 0.
                            */
    double MC_table_output_interval; /* The time interval, in seconds, at
                                        which to report the (delay) model component
                                        (MC table) values in the output FITS files.
                                        The default value of 0.0 results in
                                        the tabulated values occuring at polyInterval
                                        seconds (defualts to
                                        DIFXIO_DEFAULT_POLY_INTERVAL).  Note that in
                                        any case, the interval will be no longer than
                                        polyInterval seconds.
                                     */
    int quantBits;      /* 1 or 2 */
    int nAntenna;
    int nDatastream;    /* number of datastreams attached */
    int nBaseline;      /* number of baselines */
    int *datastreamId;  /* 0-based; datastream table indx */
                        /* -1 terminated [ds # < nDatastream]  */
    int *baselineId;    /* baseline table indicies for this config */
                        /* -1 terminated [bl # < nBaseline] */
    
    /* FIXME: Really these don't belong in here.  Someday (DiFX-3?) move these out. */
    int nIF;            /* number of FITS IFs to create */
    DifxIF *IF;         /* FITS IF definitions */
    int fitsFreqId;     /* 0-based number -- unique FITS IF[] index NOT AN INDEX TO DifxFreq[]! */
    int *freqId2IF;     /* map from freq table [0 to nFreq] index to IF [0 to nIF-1] or -1 */
                        /* a value of -1 indicates this Freq is not used */
    int *freqIdUsed;    /* Is the Freq table index used by this config? */

    int *ant2dsId;      /* map from .input file antenna# to internal
                         * DifxDatastream Id. [0..nAntenna-1]
                         * this should be used only in conjunction
                         * with .difx/ antenna numbers! */
} DifxConfig;

typedef struct
{
    DifxStringArray sourceName; /* (DiFX) name(s) of source, optional */
    DifxStringArray scanId;     /* Scan identifier(s) from vex file, optional */
    char calCode[DIFXIO_CALCODE_LENGTH];  /* calCode, optional */
    int qual;           /* Source qualifier, optional */
    double mjdStart;    /* start time, optional */
    double mjdStop;     /* stop time, optional */
    char configName[DIFXIO_NAME_LENGTH];  /* Name of the configuration to which 
                   this rule is applied */  
} DifxRule;

typedef struct
{
    int antennaId;      /* index to D->antenna */
    float tSys;         /* 0.0 for VLBA DiFX */
    char dataFormat[DIFXIO_FORMAT_LENGTH];   /* e.g., VLBA, MKIV, ... */

    enum SamplingType dataSampling; /* REAL or COMPLEX */
    int nFile;          /* number of files */
    char **file;        /* list of files to correlate (if not VSN) */
    char networkPort[DIFXIO_ETH_DEV_SIZE]; /* eVLBI port for this datastream */
    int windowSize;     /* eVLBI TCP window size */
    int quantBits;      /* quantization bits */
    int dataFrameSize;  /* (bytes) size of formatted data frame */
    enum DataSource dataSource; /* MODULE, FILE, NET, other? */

    int phaseCalIntervalMHz;/* 0 if no phase cal extraction, otherwise extract every tone */
    int tcalFrequency;  /* 0 if no switched power extraction to be done.  =80 for VLBA */
    int nRecTone;       /* number of pcal tones in the *recorded* baseband*/
    int *recToneFreq;   /* Frequency of each pcal tone in the *recorded* baseband in MHz */
    int *recToneOut;    /* bool Recorded pcal written out?*/

    double *clockOffset;    /* (us) [freq] */
    double *clockOffsetDelta; /* (us) [freq] */
    double *phaseOffset;    /* (degrees) [freq] */
    double *freqOffset; /* Freq offsets for each frequency in Hz */
    
    int nRecFreq;       /* number of freqs recorded in this datastream */
    int nRecBand;       /* number of base band channels recorded */
    int *nRecPol;       /* [recfreq] */
    int *recFreqId;     /* [recfreq] index to DifxFreq table */
    int *recBandFreqId; /* [recband] index to recFreqId[] */
    char *recBandPolName;   /* [recband] Polarization name (R, L, X or Y) */

    int nZoomFreq;      /* number of "zoom" freqs (within recorded freqs) for this datastream */
    int nZoomBand;      /* number of zoom subbands */
    int *nZoomPol;      /* [zoomfreq] */
    int *zoomFreqId;    /* [zoomfreq] index to DifxFreq table */
    int *zoomBandFreqId;    /* [zoomband] index to zoomfreqId[] */
    char *zoomBandPolName;  /* [zoomband] Polarization name (R, L, X or Y) */
} DifxDatastream;

typedef struct
{
    int dsA, dsB;       /* indices to datastream table */
    int nFreq;
    int *nPolProd;      /* [freq] */

    /* note: band in excess of nRecBand are assumed to be zoom bands */
    int **bandA;        /* [freq][productIndex] */
    int **bandB;        /* [freq][productIndex] */
} DifxBaseline;

typedef struct
{
    char name[DIFXIO_NAME_LENGTH];      /* null terminated */
    char calcname[DIFXIO_NAME_LENGTH];  /* null terminated */
                        /* Antenna name (if different) to provide to the
                           delay model software (CALC) */
    int origId;         /* antennaId before a sort */
    double clockrefmjd; /* Reference time for clock polynomial */
    int clockorder;     /* Polynomial order of the clock model */
    double clockcoeff[MAX_MODEL_ORDER+1];   /* clock polynomial coefficients (us, us/s, us/s^2... */
    enum AntennaMountType mount;
    enum AntennaSiteType sitetype;
    enum SourceCoordinateFrameType site_coord_frame;
    double offset[3];   /* axis offset, (m) */
    double X, Y, Z;     /* telescope position, (m) */
    double dX, dY, dZ;  /* telescope position derivative, (m/s) */
    int spacecraftId;   /* -1 if not a spacecraft */
    char sc_name[DIFXIO_NAME_LENGTH]; /* null terminated */
    char shelf[DIFXIO_SHELF_LENGTH];  /* shelf location of module; really this should not be here! */
} DifxAntenna;

typedef struct
{
    double ra, dec;     /* radians */
    char name[DIFXIO_NAME_LENGTH];      /* source name */
    char calCode[DIFXIO_CALCODE_LENGTH];    /* usually only 1 char long */
    enum SourceCoordinateFrameType coord_frame;
    enum PerformDirectionDerivativeType perform_uvw_deriv;
    enum PerformDirectionDerivativeType perform_lmn_deriv;
    enum PerformDirectionDerivativeType perform_xyz_deriv;
    double delta_lmn;   /* (rad) step size for calculating d\tau/dl, d\tau/dm,
                           and d\tau/dn for the LMN polynomial model
                           and (u,v) from the delay derivatives for the UVW
                           polynomial model
                        */
    double delta_xyz;   /* step size for calculating d\tau/dx, d\tau/dy, and
                           d\tau/dz for the Cartesian (x,y,z) coordinate
                           system of the source.  If positive, this variable
                           has units of meters (\Delta x = delta_xyz).
                           If negative, then the variable is a fractional value
                           to indicate the step size as a function of the
                           current radius of the source.  (So with
                           r = (x^2 + y^2 + z^2)^{1/2},
                           \Delta x = delta_xyz \times r.)
                        */
    double delta_lmn_used; /* Actual delta used for a specific instant */
    double delta_xyz_used; /* Actual delta used for a specific instant */
    int qual;           /* source qualifier */
    int spacecraftId;   /* -1 if not spacecraft */
    char sc_name[DIFXIO_NAME_LENGTH];       /* spacecraft name */
    double sc_epoch;        /* MJD at which to evaluate the spacecraft
                               position. If sc_epoch==0.0,
                               use the default evaluation as a function of
                               time */
    int numFitsSourceIds;   /* Should be equal to the number of configs */
                        /* FITS source IDs are filled in in deriveFitsSourceIds
                         */
    int *fitsSourceIds; /* 0-based FITS source id */
    double pmRA;        /* arcsec/year */
    double pmDec;       /* arcsec/year */
    double parallax;    /* arcsec */
    double pmEpoch;     /* MJD */
    double station0PropDelay; /* Estimate of the time delay between the time
                                 of emission at the source and the time of
                                 reception at station 0, in the clock system
                                 related to coord_frame.  In units of seconds.
                              */
} DifxSource;

typedef struct
{
    int mjd;            /* day of start of polynomial validity */
    int sec;            /* time (sec) of start of validity */
    int order;          /* order of polynomial -> order+1 terms! */
    int validDuration;  /* (seconds), from mjd, sec */
    double delta;       /* (rad) displacement used in calculating derivatives */
                        /* If delta == 0.0, (u,v) was provided by the
                           delay erver software, not by numerical
                           differentiation for d\tau/dl and d\tau/dm */
    double delay[MAX_MODEL_ORDER+1];    /* (us/sec^n); n=[0, order] */
    double dry[MAX_MODEL_ORDER+1];      /* (us/sec^n) */
    double wet[MAX_MODEL_ORDER+1];      /* (us/sec^n) */
    double iono[MAX_MODEL_ORDER+1];     /* (us/sec^n) at a frequency of 1 GHz*/
    double az[MAX_MODEL_ORDER+1];       /* azimuth (deg) */
    double elcorr[MAX_MODEL_ORDER+1];   /* el (corrected for refraction; i.e., the one used for pointing) (deg) */
    double elgeom[MAX_MODEL_ORDER+1];   /* el (uncorrected for refraction) (deg) */
    double parangle[MAX_MODEL_ORDER+1]; /* parallactic angle (deg) */
    double msa[MAX_MODEL_ORDER+1];      /* (rad/sec^n), mount-source angle */
    double sc_gs_delay[MAX_MODEL_ORDER+1];     /* (us/sec^n) delay between
                                                  reception of the celestial
                                                  signal at the spacecraft
                                                  and reception of the data
                                                  signal at the ground station
                                                */ 
    double gs_sc_delay[MAX_MODEL_ORDER+1];     /* (us/sec^n) delay between
                                                  transmission of the clock
                                                  signal by the ground station
                                                  and the use of the clock
                                                  signal in the spacecraft
                                                  electronics for sampling
                                                  and time stamping
                                               */
    double gs_clock_delay[MAX_MODEL_ORDER+1];  /* (us/sec^n) ground station
                                                  clock offset
                                                */
    /* see CALCServer.h for the msa specification */
    double u[MAX_MODEL_ORDER+1];        /* (m/sec^n) */
    double v[MAX_MODEL_ORDER+1];        /* (m/sec^n) */
    double w[MAX_MODEL_ORDER+1];        /* (m/sec^n) */
} DifxPolyModel;

typedef struct
{
    /* essentially a matrix of partial derivatives with respect to source l, m, and n */
    /* of the full delay (including atmosphere) */
    double delta;       /* (rad) displacement used in calculating derivatives */
    double dDelay_dl[MAX_MODEL_ORDER+1];    /* (us/sec^n/rad); n=[0, order] (should be equiv to U but in time units) */
    double dDelay_dm[MAX_MODEL_ORDER+1];    /* (us/sec^n/rad); n=[0, order] (should be equiv to V but in time units) */
    double dDelay_dn[MAX_MODEL_ORDER+1];    /* (us/sec^n); n=[0, order] (should be equiv to W but in time units) */
    /* Note that whereas l and m derivatives are made by rotating the source
       direction by some delta angle in radians, the n numerical derivative
       is made by moving the soruce radially a fraction of the distance.  So
       if the radius from the coordinate system origin to the source is r, then
       the derivative is made by [delay(at r*{1.0+delta}) - delay(at r)]/delta.
    */
    double d2Delay_dldl[MAX_MODEL_ORDER+1]; /* (us/sec^n/rad^2); n=[0, order] */
    double d2Delay_dldm[MAX_MODEL_ORDER+1]; /* (us/sec^n/rad^2); n=[0, order] */
    double d2Delay_dldn[MAX_MODEL_ORDER+1]; /* (us/sec^n/rad^2); n=[0, order] */
    double d2Delay_dmdm[MAX_MODEL_ORDER+1]; /* (us/sec^n/rad^2); n=[0, order] */
    double d2Delay_dmdn[MAX_MODEL_ORDER+1]; /* (us/sec^n/rad^2); n=[0, order] */
    double d2Delay_dndn[MAX_MODEL_ORDER+1]; /* (us/sec^n/rad^2); n=[0, order] */
} DifxPolyModelLMNExtension;

typedef struct
{
    /* essentially a matrix of partial derivatives with respect to source XYZ */
    /* of the full delay (including atmosphere) */
    double delta;       /* (m) displacement used in calculating derivatives */
    double dDelay_dX[MAX_MODEL_ORDER+1];    /* (us/sec^n/m); n=[0, order] */
    double dDelay_dY[MAX_MODEL_ORDER+1];    /* (us/sec^n/m); n=[0, order] */
    double dDelay_dZ[MAX_MODEL_ORDER+1];    /* (us/sec^n/m); n=[0, order] */
    double d2Delay_dXdX[MAX_MODEL_ORDER+1]; /* (us/sec^n/m^2); n=[0, order] */
    double d2Delay_dXdY[MAX_MODEL_ORDER+1]; /* (us/sec^n/m^2); n=[0, order] */
    double d2Delay_dXdZ[MAX_MODEL_ORDER+1]; /* (us/sec^n/m^2); n=[0, order] */
    double d2Delay_dYdY[MAX_MODEL_ORDER+1]; /* (us/sec^n/m^2); n=[0, order] */
    double d2Delay_dYdZ[MAX_MODEL_ORDER+1]; /* (us/sec^n/m^2); n=[0, order] */
    double d2Delay_dZdZ[MAX_MODEL_ORDER+1]; /* (us/sec^n/m^2); n=[0, order] */
} DifxPolyModelXYZExtension;

typedef struct
{
    double mjdStart;    /* (day) */
    double mjdEnd;      /* (day) */
    int startSeconds;   /* Since model reference (top of calc file) */
    int durSeconds;     /* Duration of the scan */
    char identifier[DIFXIO_NAME_LENGTH];    /* Usually a zero-based number */
    char obsModeName[DIFXIO_NAME_LENGTH];   /* Identifying the "mode" of observation */
    int maxNSBetweenUVShifts;   /* Maximum interval until data must be shifted/averaged */
    int maxNSBetweenACAvg;      /* Maximum interval until autocorrelations are sent/averaged */
    int pointingCentreSrc;      /* index to source array */
    int nPhaseCentres;          /* Number of correlation centres */
    int phsCentreSrcs[MAX_PHS_CENTRES];     /* indices to source array */
    int orgjobPhsCentreSrcs[MAX_PHS_CENTRES];/* indices to the source array from the original (pre-merged) job */
    int jobId;          /* 0, 1, ... nJob-1 */
    int configId;       /* to determine freqId */
    int nAntenna;
    int nPoly;
    DifxPolyModel ***im;/* indexed by [ant][src][poly] */
                        /* ant is index of antenna in .input file */
                        /*   src ranges over [0...nPhaseCentres] */
                        /*   poly ranges over [0 .. nPoly-1] */
                        /* NOTE : im[ant] can be zero -> no data */
    DifxPolyModelLMNExtension ***imLMN; /* Experimental feature; not usually used */
    DifxPolyModelXYZExtension ***imXYZ; /* Experimental feature; not usually used */
} DifxScan;

typedef struct
{
    int mjd;            /* (day) */
    int tai_utc;        /* (sec) */
    double ut1_utc;     /* (sec) */
    double xPole, yPole;/* (arcsec) */
} DifxEOP;

typedef struct
{
    double X, Y, Z;     /* (m, m/s, m s^{-2}, ...) */
} simple3Vector;

typedef struct
{
    int mjd;
    double fracDay;
    long double X, Y, Z;    /* (m) */
    long double dX, dY, dZ; /* (m/sec) */
                                /* Warning! evaluateDifxSpacecraftSource
                                   used to return units of m/day for
                                   velocities.
                                */
} sixVector;

typedef struct
{
    int mjd;
    double fracDay;
    double X, Y, Z;         /* (m) */
    double dX, dY, dZ;      /* (m/s) */
    double ddX, ddY, ddZ;   /* (m/s/s) */
} nineVector;

typedef struct
{
    double Delta_t;             /* the difference in time between the
                                   spacecraft and TT time frames, in seconds,
                                   such that the spacecraft clock reads
                                   TT + \Delta t seconds at TT MJD time
                                   mjd.fracDay.
                                   T_{SC} = TT + \Delta t
                                   (s) */
    double dtdtau;              /* The rate of \Delta t
                                   (s/s) */
} spacecraftTimeFrameOffset;

typedef struct
{
    double X[3];                /* unit vector for X axis (usually up or North)*/
    double Y[3];                /* unit vector for Y axis */
    double Z[3];                /* unit vector for Z axis (toward source) */
} spacecraftAxisVectors;

typedef struct
{
    char name[DIFXIO_NAME_LENGTH];  /* name of spacecraft */
    int is_antenna;     /* spacecraft can be antennas or sources */
    /* sources have different light travel time */
    /* corrections in position calculations */
    /* spacecraft ground station (GS) information */
    int GS_exists;          /* Is there a ground station for this antenna? */
    char GS_Name[DIFXIO_NAME_LENGTH];     /* Ground station name */
    char GS_calcName[DIFXIO_NAME_LENGTH]; /* Ground station name (if different) to provide to the */
    /*     delay model software (CALC) */
    enum SpacecraftTimeType spacecraft_time_type;
    enum SourceCoordinateFrameType position_coord_frame;
    enum SourceCoordinateFrameType pointing_coord_frame;
    int GS_mjd_sync;        /* TT MJD at which the spacecraft and
                               ground station clocks are synced */
    double GS_dayfraction_sync;/* TT time at which the spacecraft and
                                  ground station clocks are synced */
    double GS_clock_break_fudge_sec;/* Extra offset term for
                                       ground station recording offsets, in units
                                       of seconds, from what it should
                                       nominally be.  This has the same sign
                                       as GS_clockcoeff[0] (so it is the amount
                                       of time that the clock is late).  This
                                       term arrises because of a bug in the
                                       Pushchino ground station for RadioAstron
                                       that sometimes starts recording some
                                       milliseconds off from the time it
                                       writes to the header as the start time.
                                       Note that GS_dayfraction_sync should
                                       already be corrected for this term
                                       when an instance of this struct is
                                       generated.
                                    */
    double SC_recording_delay; /* This is the time between reception of the
                                  wavefront by the antenna and the
                                  marking of the data with a specific timestamp
                                  inside of the spacecraft recording
                                  electronics, in s.
                                  If the SpacecraftTimeType is local
                                  (timestamp from its own local clock),
                                  then this is not used.  The regular
                                  clock offset should be used instead. */
    double SC_Comm_Rec_to_Elec;/* This is the time between the arrival of a
                                  ground signal at the communications antenna
                                  and the time that the signal reaches the
                                  recording electronics on the spacecraft
                                  to be used to mark a timestamp or control
                                  the sampling rate, in s.
                               */
    double SC_Elec_to_Comm;    /* This is the time between the marking of the
                                  celestial signal with a timestamp (or
                                  sampling the celestial signal) in the
                                  recording electronics, and the propagation
                                  through the electronics to the communications
                                  antenna for transmission to the ground, in s.
                               */
    /* The following 6 variables are time variable and will be updated
       repeatedly within the delay calculation software.
    */
    double GS_recording_delay; /* for the SpacecraftTimeGroundReception
                                  and SpacecraftTimeGroundClockReception
                                  time types, this gives the delay between
                                  transmission of signal from the spacecraft
                                  communications antenna
                                  and recording at the ground station [s]
                                  at the instant specified by GS_mjd_sync
                                  and GS_dayfraction_sync
                               */
    double GS_transmission_delay;
                               /* For the SpacecraftTimeGroundClock and
                                  SpacecraftTimeGroundClockReception
                                  time types, this is the continuously
                                  variable time delay between the
                                  transmission of the clock signal by
                                  the ground station and the reception
                                  of the signal at the spacecraft
                                  communications antenna. [s]
                                */
    double GS_transmission_delay_sync;
                               /* For the SpacecraftTimeGroundClockReception
                                  time type, this is the delay between the
                                  transmission of the clock signal from the
                                  ground station to the spacecraft that was
                                  in effect when the ground station marked
                                  the recording time offset at the
                                  instant specified by GS_mjd_sync
                                  and GS_dayfraction_sync. [s]
                               */
    double SC_elec_delay;      /* For all time types, this is the extra delay
                                  to be added (positive means earlier before
                                  the center of Earth receives the signal)
                                  to account for the electronics delays
                                  within the spacecraft [s].  This will
                                  be computed within calcif2 from available
                                  information above.
                               */
    double GS_clock_delay;     /* Internal variable to store the ground
                                  station clock offset used for a specific
                                  time evaluation [s].
                               */
    double GS_clock_delay_sync;/* Internal variable to store the ground
                                  station clock offset used for the time
                                  at which the ground station syncs the 
                                  timestamp [s].
                               */
    enum AntennaMountType GS_mount;
    double GS_offset[3];    /* axis offset, (m) */
    double GS_X, GS_Y, GS_Z;/* telescope position, (m) */
    double GS_clockrefmjd;  /* Reference time for GS clock polynomial */
    int GS_clockorder;      /* Polynomial order of the GS clock model */
    double GS_clockcoeff[MAX_MODEL_ORDER+1]; /* GS clock polynomial 
                               coefficients (us, us/s, us/s^2... */
    /* spacecraft J2000 Earth center position information */
    int nPoint;             /* number of entries in ephemeris */
    sixVector *pos;         /* array of positions and velocities */
    spacecraftTimeFrameOffset* TFrameOffset; /* array of time frame offsets*/
    spacecraftAxisVectors* SCAxisVectors; /* array of axis vectors */
    int SC_pos_offset_refmjd; /* Reference MJD for the spacecraft
                                 position offset information */
    double SC_pos_offset_reffracDay; /* Reference MJD fractional day
                                        for the spacecraft
                                        position offset information */
    int SC_pos_offsetorder; /* Order of SC pos offset poly */
    simple3Vector SC_pos_offset[MAX_MODEL_ORDER+1]; /* spacecraft
                                                       position offset polynomial information, as
                                                       m, m/s, m/s^2, ... */
	int SC_clock_offset_order; /* polynomial order of SC_clock_offset,
								  with value -1 indicating no offsets are
								  to be applied */
    double SC_clock_offset[MAX_MODEL_ORDER+1];
                        /* clock polynomial offset (fudge) coefficients
						   (us, us/s, us/s^2... */
    int calculate_own_retarded_position;
                        /* If 0, let the spacecraft software calculate its
                           own time retardation for spacecraft.
                           Otherwise, use our own predicted delay to the
                           center of the Earth to calculate the retarded
                           positions.
                         */
} DifxSpacecraft;

typedef struct
{
    double mjd1, mjd2;  /* (day) */
    int antennaId;      /* antenna number (index to D->antenna) */
} DifxAntennaFlag;


/* DifxJob contains information relevant for a particular job.
 * In some cases, multiple jobs will be concatennated and some
 * information about the individual jobs will be needed.
 * In particular, the DifxAntennaFlag contains flags that are
 * generated by .input/.calc generating programs (such as vex2difx)
 * which are job dependent and are to be applied when building the
 * output FITS files.
 */

typedef struct
{
    char difxVersion[DIFXIO_VERSION_LENGTH];  /* Name of difx version in .calc file */
    char difxLabel[DIFXIO_VERSION_LENGTH];    /* Name of difx label in .calc file */
    double jobStart;    /* cjobgen job start time (mjd) */
    double jobStop;     /* cjobgen job start time (mjd) */
    double mjdStart;    /* subjob start time (mjd) */
    double duration;    /* subjob observe duration (sec) */
    int jobId;          /* correlator job number */
    int subjobId;       /* difx specific sub-job id */
    int subarrayId;     /* sub array number of the specified sub-job */
    char obsCode[DIFXIO_OBSCODE_LENGTH];     /* project name */
    char obsSession[DIFXIO_SESSION_LENGTH];  /* project session (e.g., A, B, C1) */
	enum TaperFunction taperFunction;	 /* currently only "UNIFORM" is supported */
    char delayServerHost[DIFXIO_HOSTNAME_LENGTH]; /* name of delay server */
    enum DelayServerType delayServerType; /* type of delay server */
    enum DelayServerHandlerType delayServerHandlerType; /* type of delay server handler */
    unsigned long delayVersion;  /* version number of delay server */
    unsigned long delayProgram;  /* RPC program id of delay server */
    unsigned long delayHandler;  /* RPC program id of the delay server handler*/
    unsigned long delayProgramDetailedVersion;
                                 /* The detailed version number of the
                                    delay server program
                                  */
    int activeDatastreams;
    int activeBaselines;
    int polyOrder;      /* polynomial model order */
    int polyInterval;   /* (sec) length of valid polynomial */
    float delayModelPrecision; /* suggested error maximum for delay
                                  calculation uncertainties or errors
                                  for numerical computations, in seconds
                               */
    enum PerformDirectionDerivativeType perform_uvw_deriv;
    enum PerformDirectionDerivativeType perform_lmn_deriv;
    enum PerformDirectionDerivativeType perform_xyz_deriv;
    double delta_lmn;   /* (rad) step size for calculating d\tau/dl, d\tau/dm,
                           and d\tau/dn for the LMN polynomial model
                           and (u,v) from the delay derivatives for the UVW
                           polynomial model
                        */
    double delta_xyz;   /* step size for calculating d\tau/dx, d\tau/dy, and
                           d\tau/dz for the Cartesian (x,y,z) coordinate
                           system of the source.  If positive, this variable
                           has units of meters (\Delta x = delta_xyz).
                           If negative, then the variable is a fractional value
                           to indicate the step size as a function of the
                           current radius of the source.  (So with
                           r = (x^2 + y^2 + z^2)^{1/2},
                           \Delta x = delta_xyz \times r.)
                        */
    enum AberCorr aberCorr; /* level of correction for aberration */
    int calculate_own_retarded_position;
                        /* If 0, let the spacecraft software calculate its
                           own time retardation for spacecraft.
                           Otherwise, use our own predicted delay to the
                           center of the Earth to calculate the retarded
                           positions.
                         */
    double dutyCycle;   /* fraction of time in scans */

    int nFlag;
    DifxAntennaFlag *flag;  /* flags to be applied at FITS building time */
	DifxCalcParamTable* calcParamTable; /* delay server parameters */

    /* Filenames */
    char vexFile[DIFXIO_FILENAME_LENGTH];
    char inputFile[DIFXIO_FILENAME_LENGTH];
    char calcFile[DIFXIO_FILENAME_LENGTH];
    char imFile[DIFXIO_FILENAME_LENGTH];
    char flagFile[DIFXIO_FILENAME_LENGTH];
    char threadsFile[DIFXIO_FILENAME_LENGTH];
    char outputFile[DIFXIO_FILENAME_LENGTH];

    /* Remappings.  These are null arrays unless some renumbering from original values occurred */
    int *jobIdRemap;    /* confusingly, not the same jobId as that in this structure, but rather index to DifxJob */
    int *freqIdRemap;
    int *antennaIdRemap;
    int *datastreamIdRemap;
    int *baselineIdRemap;
    int *pulsarIdRemap;
    int *configIdRemap;
    int *sourceIdRemap;
    int *spacecraftIdRemap;
} DifxJob;

typedef struct
{
    int fracSecondStartTime;/* allow writing of fractional second start time? */
    double mjdStart;    /* start of combined dataset */
    double mjdStop;     /* end of combined dataset */
    double refFreq;     /* some sort of reference frequency, (MHz) */
    int startChan;      /* first (unaveraged) channel to write, only set for difx2fits */
    int specAvg;        /* number of channels to average post corr. */
    int nInChan;        /* number of correlated channels, only set for difx2fits */
    int nOutChan;       /* number of channels to write to FITS, only set for difx2fits */
                        /* Statchan, nInChan and nOutChan are all set haphazardly, and
                           will certainly have odd things if not all freqs are the same */
    int visBufferLength;/* number of visibility buffers in mpifxcorr */

    int nIF;            /* maximum num IF across configs */
    int nPol;           /* maximum num pol across configs */
    int doPolar;        /* 0 if not, 1 if so */
    int nPolar;         /* nPol*(doPolar+1) */
                        /* 1 for single pol obs */
                        /* 2 for dual pol, parallel hands only */
                        /* 4 for full pol */
    double chanBW;      /* MHz common channel bandwidth. 0 if differ */
    int quantBits;      /* 0 if different in configs; or 1 or 2 */
    char polPair[4];    /* "  " if different in configs */
    int dataBufferFactor;
    int nDataSegments;
    enum OutputFormatType outputFormat;
	enum EOPMergeMode eopMergeMode;
    int nCore;          /* from the .threads file, or zero if no file */
    int *nThread;       /* [coreId]: how many threads to use on each core */

    int nAntenna, nConfig, nRule, nFreq, nScan, nSource, nEOP, nFlag;
    int nDatastream, nBaseline, nSpacecraft, nPulsar, nPhasedArray, nJob;
    DifxJob     *job;
    DifxConfig  *config;
    DifxRule    *rule;
    DifxFreq    *freq;
    DifxAntenna *antenna;
    DifxScan    *scan;  /* assumed in time order */
    DifxSource  *source;
    DifxEOP     *eop;   /* assumed one per day, optional */
    DifxDatastream  *datastream;
    DifxBaseline    *baseline;
    DifxSpacecraft  *spacecraft;    /* optional table */
    DifxPulsar      *pulsar;        /* optional table */
    DifxPhasedArray *phasedarray;   /* optional table */
} DifxInput;

/* DifxJob functions */
enum AberCorr stringToAberCorr(const char* str);
enum PerformDirectionDerivativeType stringToPerformDirectionDerivativeType(const char *str);
enum DelayServerType stringToDelayServerType(const char *str);
enum DelayServerHandlerType stringToDelayServerHandlerType(const char *str);
enum TaperFunction stringToTaperFunction(const char *str);
DifxJob *newDifxJobArray(int nJob);
void deleteDifxJobArray(DifxJob *dj, int nJob);
void printDifxJob(const DifxJob *dj);
void fprintDifxJob(FILE *fp, const DifxJob *dj);
void copyDifxJob(DifxJob *dest, const DifxJob *src, int *antennaIdRemap);
void generateDifxJobFileBase(DifxJob *dj, char *fileBase);
DifxJob *mergeDifxJobArrays(const DifxJob *dj1, int ndj1, const DifxJob *dj2, int ndj2, int *jobIdRemap, int *antennaIdRemap, int *ndj);

/* DifxFreq functions */
DifxFreq *newDifxFreqArray(int nFreq);
void DifxFreqAllocTones(DifxFreq *df, int nTone);
void deleteDifxFreqInternals(DifxFreq *df);
void deleteDifxFreqArray(DifxFreq *df, int nFreq);
void printDifxFreq(const DifxFreq *df);
void fprintDifxFreq(FILE *fp, const DifxFreq *df);
int isSameDifxFreqToneSet(const DifxFreq *df1, const DifxFreq *df2);
int isSameDifxFreq(const DifxFreq *df1, const DifxFreq *df2);
int isDifxIFInsideDifxFreq(const DifxIF *di, const DifxFreq *df);
void copyDifxFreq(DifxFreq *dest, const DifxFreq *src);
int simplifyDifxFreqs(DifxInput *D);
DifxFreq *mergeDifxFreqArrays(const DifxFreq *df1, int ndf1, const DifxFreq *df2, int ndf2, int *freqIdRemap, int *ndf);
int writeDifxFreqArray(FILE *out, int nFreq, const DifxFreq *df);

/* DifxAntenna functions */
enum AntennaMountType stringToMountType(const char *str);
enum AntennaSiteType stringToSiteType(const char *str);
DifxAntenna *newDifxAntennaArray(int nAntenna);
void deleteDifxAntennaArray(DifxAntenna *da, int nAntenna);
void printDifxAntenna(const DifxAntenna *da);
void fprintDifxAntenna(FILE *fp, const DifxAntenna *da);
void fprintDifxAntennaSummary(FILE *fp, const DifxAntenna *da);
int isSameDifxAntenna(const DifxAntenna *da1, const DifxAntenna *da2);
int isSameDifxAntennaClock(const DifxAntenna *da1, const DifxAntenna *da2);
int getDifxAntennaShiftedClock(const DifxAntenna *da, double dt, int outputClockSize, double *clockOut);
double evaluateDifxAntennaClock(const DifxAntenna *da, double mjd);
void copyDifxAntenna(DifxAntenna *dest, const DifxAntenna *src);
DifxAntenna *mergeDifxAntennaArrays(const DifxAntenna *da1, int nda1, const DifxAntenna *da2, int nda2, int *antennaIdRemap, int *nda);
int writeDifxAntennaArray(FILE *out, int nAntenna, const DifxAntenna *da, int doMount, int doOffset, int doCoords, int doClock, int doShelf, int doSpacecraftID);

/* DifxDatastream functions */
enum DataSource stringToDataSource(const char *str);
enum SamplingType stringToSamplingType(const char *str);
DifxDatastream *newDifxDatastreamArray(int nDatastream);
void DifxDatastreamAllocFiles(DifxDatastream *ds, int nFile);
void DifxDatastreamAllocFreqs(DifxDatastream *dd, int nReqFreq);
void DifxDatastreamAllocBands(DifxDatastream *dd, int nRecBand);
void DifxDatastreamAllocZoomFreqs(DifxDatastream *dd, int nZoomFreq);
void DifxDatastreamAllocZoomBands(DifxDatastream *dd, int nZoomBand);
void DifxDatastreamAllocPhasecalTones(DifxDatastream *dd, int nTones);
void DifxDatastreamCalculatePhasecalTones(DifxDatastream *dd, const DifxFreq *df);
int DifxDatastreamGetPhasecalTones(double *toneFreq, const DifxDatastream *dd, const DifxFreq *df, int maxCount);
void deleteDifxDatastreamInternals(DifxDatastream *dd);
void deleteDifxDatastreamArray(DifxDatastream *dd, int nDatastream);
void fprintDifxDatastream(FILE *fp, const DifxDatastream *dd);
void printDifxDatastream(const DifxDatastream *dd);
int isSameDifxDatastream(const DifxDatastream *dd1, const DifxDatastream *dd2, const int *freqIdRemap, const int *antennaIdRemap);
void copyDifxDatastream(DifxDatastream *dest, const DifxDatastream *src, const int *freqIdRemap, const int *antennaIdRemap);
void moveDifxDatastream(DifxDatastream *dest, DifxDatastream *src);
int simplifyDifxDatastreams(DifxInput *D);
DifxDatastream *mergeDifxDatastreamArrays(const DifxDatastream *dd1, int ndd1, const DifxDatastream *dd2, int ndd2, int *datastreamIdRemap, const int *freqIdRemap, const int *antennaIdRemap, int *ndd);
int writeDifxDatastream(FILE *out, const DifxDatastream *dd);
int DifxDatastreamGetRecBands(DifxDatastream *dd, int freqId, char *pols, int *recBands);
int DifxDatastreamGetZoomBands(DifxDatastream *dd, int freqId, char *pols, int *zoomBands);
char getDifxDatastreamBandPol(const DifxDatastream *dd, int band);

/* DifxBaseline functions */
DifxBaseline *newDifxBaselineArray(int nBaseline);
void DifxBaselineAllocFreqs(DifxBaseline *b, int nFreq);
void DifxBaselineAllocPolProds(DifxBaseline *b, int freq, int nPol);
void deleteDifxBaselineInternals(DifxBaseline *db);
void deleteDifxBaselineArray(DifxBaseline *db, int nBaseline);
void fprintDifxBaseline(FILE *fp, const DifxBaseline *db);
void printDifxBaseline(const DifxBaseline *db);
int isSameDifxBaseline(const DifxBaseline *db1, const DifxBaseline *db2, const int *datastreamIdRemap);
void copyDifxBaseline(DifxBaseline *dest, const DifxBaseline *src, const int *datastreamIdRemap);
void moveDifxBaseline(DifxBaseline *dest, DifxBaseline *src);
int simplifyDifxBaselines(DifxInput *D);
DifxBaseline *mergeDifxBaselineArrays(const DifxBaseline *db1, int ndb1, const DifxBaseline *db2, int ndb2, int *baselineIdRemap, const int *datastreamIdRemap, int *ndb);
int writeDifxBaselineArray(FILE *out, int nBaseline, const DifxBaseline *db);

/* DifxPolyco functions */
DifxPolyco *newDifxPolycoArray(int nPolyco);
void deleteDifxPolycoInternals(DifxPolyco *dp);
void deleteDifxPolycoArray(DifxPolyco *dp, int nPolyco);
void printDifxPolycoArray(const DifxPolyco *dp, int nPolyco);
void fprintDifxPolycoArray(FILE *fp, const DifxPolyco *dp, int nPolyco);
void copyDifxPolyco(DifxPolyco *dest, const DifxPolyco *src);
DifxPolyco *dupDifxPolycoArray(const DifxPolyco *src, int nPolyco);
int loadPulsarPolycoFile(DifxPolyco **dpArray, int *nPoly, const char *filename);
int loadPulsarConfigFile(DifxInput *D, const char *fileName);
int DifxPolycoArrayGetMaxPolyOrder(const DifxPolyco *dp, int nPolyco);

/* DifxPhasedArray functions */
enum PhasedArrayOutputType stringToPhasedArrayOutputType(const char *str);
enum PhasedArrayOutputFormat stringToPhasedArrayOutputFormat(const char *str);
DifxPhasedArray *newDifxPhasedarrayArray(int nPhasedArray);
DifxPhasedArray *growDifxPhasedarrayArray(DifxPhasedArray *dpa, int origSize);
void deleteDifxPhasedarrayArray(DifxPhasedArray *dpa, int nPhasedArray);
void fprintDifxPhasedArray(FILE *fp, const DifxPhasedArray *dpa);
void printDifxPhasedArray(const DifxPhasedArray *dpa);
int isSameDifxPhasedArray(const DifxPhasedArray *dpa1, const DifxPhasedArray *dpa2);
DifxPhasedArray *dupDifxPhasedarrayArray(const DifxPhasedArray *src, int nPhasedArray);
DifxPhasedArray *mergeDifxPhasedarrayArrays(const DifxPhasedArray *dpa1, 
	int ndpa1, const DifxPhasedArray *dpa2, int ndpa2, 
	int *phasedArrayIdRemap, int *ndpa);

/* DifxPulsar functions */
DifxPulsar *newDifxPulsarArray(int nPulsar);
DifxPulsar *growDifxPulsarArray(DifxPulsar *dp, int origSize);
void deleteDifxPulsarInternals(DifxPulsar *dp);
void deleteDifxPulsarArray(DifxPulsar *dp, int nPulsar);
void fprintDifxPulsar(FILE *fp, const DifxPulsar *dp);
void printDifxPulsar(const DifxPulsar *dp);
int isSameDifxPulsar(const DifxPulsar *dp1, const DifxPulsar *dp2);
DifxPulsar *dupDifxPulsarArray(const DifxPulsar *src, int nPulsar);
DifxPulsar *mergeDifxPulsarArrays(const DifxPulsar *dp1, int ndp1, const DifxPulsar *dp2, int ndp2, int *pulsarIdRemap, int *ndp);
int DifxPulsarArrayGetMaxPolyOrder(const DifxPulsar *dp, int nPulsar);

/* DifxConfig functions */
DifxConfig *newDifxConfigArray(int nConfig);
void DifxConfigAllocDatastreamIds(DifxConfig *dc, int nDatastream, int start);
void DifxConfigAllocBaselineIds(DifxConfig *dc, int nBaseline, int start);
void deleteDifxConfigInternals(DifxConfig *dc);
void deleteDifxConfigArray(DifxConfig *dc, int nConfig);
void fprintDifxConfig(FILE *fp, const DifxConfig *dc);
void printDifxConfig(const DifxConfig *dc);
void fprintDifxConfigSummary(FILE *fp, const DifxConfig *dc);
void printDifxConfigSummary(const DifxConfig *dc);
int isSameDifxConfig(const DifxConfig *dc1, const DifxConfig *dc2);
void copyDifxConfig(DifxConfig *dest, const DifxConfig *src, const int *baselineIdRemap, const int *datastreamIdRemap, const int *pulsarIdRemap);
void moveDifxConfig(DifxConfig *dest, DifxConfig *src);
int simplifyDifxConfigs(DifxInput *D);
DifxConfig *mergeDifxConfigArrays(const DifxConfig *dc1, int ndc1, const DifxConfig *dc2, int ndc2, int *configIdRemap, const int *baselineIdRemap, const int *datastreamIdRemap, const int *pulsarIdRemap, int *ndc);
int DifxConfigCalculateDoPolar(DifxConfig *dc, DifxBaseline *db);
int DifxConfigGetPolId(const DifxConfig *dc, char polName);
int DifxConfigRecBand2FreqPol(const DifxInput *D, int configId,
    int antennaId, int recBand, int *freqId, int *polId);
int writeDifxConfigArray(FILE *out, int nConfig, const DifxConfig *dc, const DifxPulsar *pulsar, const DifxPhasedArray *phasedarray);

/* DifxRule functions */
DifxRule *newDifxRuleArray(int nRule);
void deleteDifxRuleArray(DifxRule *dr, int nRule);
void fprintDifxRule(FILE *fp, const DifxRule *dc);
void printDifxRule(const DifxRule *dr);
int writeDifxRuleArray(FILE *out, const DifxInput *D);
void copyDifxRule(DifxRule * dest, DifxRule * src);
int simplifyDifxRules(DifxInput *D);
int ruleAppliesToScanSource(const DifxRule * dr, const DifxScan * ds, const DifxSource * src);

/* DifxPolyModel functions */
DifxPolyModel ***newDifxPolyModelArray(int nAntenna, int nSrcs, int nPoly);
DifxPolyModel *dupDifxPolyModelColumn(const DifxPolyModel *src, int nPoly);
void deleteDifxPolyModelArray(DifxPolyModel ***dpm, int nAntenna, int nSrcs);
void printDifxPolyModel(const DifxPolyModel *dpm, int antennaId, int sourceId, int polyId);
void fprintDifxPolyModel(FILE *fp, const DifxPolyModel *dpm, int antennaId, int sourceId, int polyId);

DifxPolyModelLMNExtension ***newDifxPolyModelLMNExtensionArray(int nAntenna, int nSrcs, int nPoly);
DifxPolyModelLMNExtension *dupDifxPolyModelLMNExtensionColumn(const DifxPolyModelLMNExtension *src, int nPoly);
void deleteDifxPolyModelLMNExtensionArray(DifxPolyModelLMNExtension ***lmne, int nAntenna, int nSrcs);

DifxPolyModelXYZExtension ***newDifxPolyModelXYZExtensionArray(int nAntenna, int nSrcs, int nPoly);
DifxPolyModelXYZExtension *dupDifxPolyModelXYZExtensionColumn(const DifxPolyModelXYZExtension *src, int nPoly);
void deleteDifxPolyModelXYZExtensionArray(DifxPolyModelXYZExtension ***xyze, int nAntenna, int nSrcs);

/* DifxScan functions */
DifxScan *newDifxScanArray(int nScan);
void deleteDifxScanInternals(DifxScan *ds);
void deleteDifxScanArray(DifxScan *ds, int nScan);
void fprintDifxScan(FILE *fp, const DifxScan *ds);
void printDifxScan(const DifxScan *ds);
void fprintDifxScanSummary(FILE *fp, const DifxScan *ds);
void printDifxScanSummary(const DifxScan *ds);
void copyDifxScan(DifxScan *dest, const DifxScan *src, const int *sourceIdRemap, const int *jobIdRemap, const int *configIdRemap, const int *antennaIdRemap);
DifxScan *mergeDifxScanArrays(const DifxScan *ds1, int nds1, const DifxScan *ds2, int nds2, const int *sourceIdRemap, const int *jobIdRemap, const int *configIdRemap, const int *antennaIdRemap, int *nds);
int getDifxScanIMIndex(const DifxScan *ds, double mjd, double iat, double *dt);
int writeDifxScan(FILE *out, const DifxScan *ds, int scanId, const DifxConfig *dc);
int writeDifxScanArray(FILE *out, int nScan, const DifxScan *ds, const DifxConfig *dc);
int padDifxScans(DifxInput *D);

/* DifxEOP functions */
enum EOPMergeMode stringToEOPMergeMode(const char *str);
DifxEOP *newDifxEOPArray(int nEOP);
void deleteDifxEOPArray(DifxEOP *de);
void printDifxEOP(const DifxEOP *de);
void fprintDifxEOP(FILE *fp, const DifxEOP *de);
void printDifxEOPSummary(const DifxEOP *de);
void fprintDifxEOPSummary(FILE *fp, const DifxEOP *de);
void copyDifxEOP(DifxEOP *dest, const DifxEOP *src);
int isSameDifxEOP(const DifxEOP *de1, const DifxEOP *de2);
DifxEOP *mergeDifxEOPArrays(const DifxEOP *de1, int nde1, const DifxEOP *de2, int nde2, int *nde);
int areDifxEOPsCompatible(const DifxEOP *de1, int nde1, const DifxEOP *de2, int nde2, enum EOPMergeMode eopMergeMode);
int writeDifxEOPArray(FILE *out, int nEOP, const DifxEOP *de);

/* DifxSpacecraft functions */
enum SpacecraftTimeType stringToSpacecraftTimeType(const char *str);
DifxSpacecraft *newDifxSpacecraftArray(int nSpacecraft);
DifxSpacecraft *dupDifxSpacecraftArray(const DifxSpacecraft *src, int n);
DifxSpacecraft *mergeDifxSpacecraft(const DifxSpacecraft *ds1, int nds1, const DifxSpacecraft *ds2, int nds2, int *spacecraftIdRemap, int *nds);
void deleteDifxSpacecraftInternals(DifxSpacecraft *ds);
void deleteDifxSpacecraftArray(DifxSpacecraft *ds, int nSpacecraft);
void printDifxSpacecraft(const DifxSpacecraft *ds);
void fprintDifxSpacecraft(FILE *fp, const DifxSpacecraft *ds);
int shiftSpacecraftClockPolys(DifxInput *D);
int computeDifxSpacecraftSourceEphemeris(DifxSpacecraft *ds, double sc_epoch_mjd, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemStellarAber, double ephemClockError);
int computeDifxSpacecraftSourceEphemerisFromXYZ(DifxSpacecraft *ds, double sc_epoch_mjd, double mjd0, double deltat, int nPoint, double X, double Y, double Z, const char *ephemType, const char *naifFile, const char* orientationFile, double ephemClockError);
int computeDifxSpacecraftAntennaEphemeris(DifxSpacecraft *ds, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemClockError);
int computeDifxSpacecraftTimeFrameOffset(DifxSpacecraft *ds, const char* JPLplanetaryephem);
int computeDifxSpacecraftEphemerisOffsets(DifxSpacecraft *ds);
int DiFX_model_scpacecraft_time_delay_qromb(const DifxSpacecraft* const spacecraft, 
                                            const int mjd0, const double frac0,
                                            const int mjd1, const double frac1,
                                            const double fractional_error,
                                            const double absolute_error,
                                            double* delay,
                                                      /* cumulative differential
                                                         delay, in seconds */
                                            double* delta_delay,
                                                      /* uncertainty in delay */
                                            double* t1_rate
                                                      /* differential rate at
                                                         time 1, in s/s */
                                            );
int DiFX_model_spacecraft_time_frame_delay_rate(const DifxSpacecraft* const spacecraft, const int MJD_TT, const double frac_TT, double* const rate);
int evaluateDifxSpacecraftSource(const DifxSpacecraft *sc, int mjd, double fracMjd, sixVector *interpolatedPosition);
int evaluateDifxSpacecraftAntenna(const DifxSpacecraft *sc, int mjd, double fracMjd, nineVector *interpolatedPosition);
int evaluateDifxSpacecraftAntennaTimeFrameOffset(const DifxSpacecraft *sc,int mjd, double fracMjd,spacecraftTimeFrameOffset *interpolated_timeoffset);
int evaluateDifxSpacecraftAntennaAxisVectors(const DifxSpacecraft *sc, int mjd, double fracMjd, spacecraftAxisVectors* direction, spacecraftAxisVectors* velocity);
int evaluateDifxSpacecraftAntennaOffset(const DifxSpacecraft *sc, int mjd, double fracMjd, nineVector *interpolatedOffset);
int writeDifxSpacecraftArray(FILE *out, int nSpacecraft, DifxSpacecraft *ds);
void get_next_Russian_scf_line(char * const line, const int MAX_LEN, FILE *fp);
int read_Russian_scf_file(const char * const filename,const char * const spacecraftname, const double MJD_start, const double MJD_end, const double MJD_delta, const double ephemClockError, DifxSpacecraft * const ds);
int read_Russian_scf_axes_file(const char * const filename, const double MJD_start, const double MJD_end, const double ephemClockError, DifxSpacecraft * const ds);

void sixVectorSetTime(sixVector *v, int mjd, double sec);
int populateSpiceLeapSecondsFromEOP(const DifxEOP *eop, int nEOP);

/* DifxSource functions */
enum SourceCoordinateFrameType stringToSourceCoordinateFrameType(const char* str);
DifxSource *newDifxSourceArray(int nSource);
void deleteDifxSourceArray(DifxSource *ds, int nSource);
void printDifxSource(const DifxSource *ds);
void fprintDifxSource(FILE *fp, const DifxSource *ds);
void printDifxSourceSummary(const DifxSource *ds);
void fprintDifxSourceSummary(FILE *fp, const DifxSource *ds);
int writeDifxSourceArray(FILE *out, int nSource, const DifxSource *ds, int doCalcode, int doQual, int doSpacecraftID);
int isSameDifxSourceBasic(const DifxSource *ds1, const DifxSource *ds2);
int isSameDifxSource(const DifxSource *ds1, const DifxSource *ds2);
void copyDifxSource(DifxSource *dest, const DifxSource *src);
DifxSource *mergeDifxSourceArrays(const DifxSource *ds1, int nds1, const DifxSource *ds2, int nds2, int *sourceIdRemap, int *nds);

/* DifxIF functions */
DifxIF *newDifxIFArray(int nIF);
void deleteDifxIFArray(DifxIF *di);
void printDifxIF(const DifxIF *di);
void fprintDifxIF(FILE *fp, const DifxIF *di);
void printDifxIFSummary(const DifxIF *di);
void fprintDifxIFSummary(FILE *fp, const DifxIF *di);
int isSameDifxIF(const DifxIF *di1, const DifxIF *di2);

/* DifxAntennaFlag functions */
DifxAntennaFlag *newDifxAntennaFlagArray(int nFlag);
void deleteDifxAntennaFlagArray(DifxAntennaFlag *df);
void printDifxAntennaFlagArray(const DifxAntennaFlag *df, int nf);
void fprintDifxAntennaFlagArray(FILE *fp, const DifxAntennaFlag *df, int nf);
void copyDifxAntennaFlag(DifxAntennaFlag *dest, const DifxAntennaFlag *src, const int *antennaIdRemap);
DifxAntennaFlag *mergeDifxAntennaFlagArrays(const DifxAntennaFlag *df1, int ndf1, const DifxAntennaFlag *df2, int ndf2, const int *antennaIdRemap, int *ndf);

/* DifxInput functions */
enum ToneSelection stringToToneSelection(const char *str);
DifxInput *newDifxInput();
void deleteDifxInput(DifxInput *D);
void printDifxInput(const DifxInput *D);
void fprintDifxInput(FILE *fp, const DifxInput *D);
void printDifxInputSummary(const DifxInput *D);
void fprintDifxInputSummary(FILE *fp, const DifxInput *D);
void DifxConfigMapAntennas(DifxConfig *dc, const DifxDatastream *ds);
DifxInput *loadDifxInput(const char *filePrefix);
DifxInput *loadDifxCalc(const char *filePrefix);
//DifxInput *deriveSourceTable(DifxInput *D);
DifxInput *allocateSourceTable(DifxInput *D, int length);
DifxInput *updateDifxInput(DifxInput *D);
int areDifxInputsMergable(const DifxInput *D1, const DifxInput *D2);
int areDifxInputsCompatible(const DifxInput *D1, const DifxInput *D2);
DifxInput *mergeDifxInputs(const DifxInput *D1, const DifxInput *D2, int verbose);
int isAntennaFlagged(const DifxJob *J, double mjd, int antennaId);
int DifxInputGetPointingSourceIdByJobId(const DifxInput *D, double mjd, int jobId);
int DifxInputGetPointingSourceIdByAntennaId(const DifxInput *D, double mjd, int antennaId);
const DifxSource *DifxInputGetSource(const DifxInput *D, const char *sourceName);
int DifxInputGetScanIdByJobId(const DifxInput *D, double mjd, int jobId);
int DifxInputGetScanIdByJobIdVis(const DifxInput *D, double mjd, int jobId);
int DifxInputGetScanIdByAntennaId(const DifxInput *D, double mjd, int antennaId);
int DifxInputGetAntennaId(const DifxInput *D, const char *antennaName);
int DifxInputGetDatastreamIdsByAntennaId(int *dsIds, const DifxInput *D, int antennaId, int maxCount);
int DifxInputGetOriginalDatastreamIdsByAntennaIdJobId(int *dsIds, const DifxInput *D, int antennaId, int jobId, int maxCount);
int DifxInputGetMaxTones(const DifxInput *D);
int DifxInputGetMaxPhaseCentres(const DifxInput *D);
int DifxInputGetFreqIdByBaselineFreq(const DifxInput *D, int baselineId, int baselineFreq);
int DifxInputGetDatastreamId(const DifxInput *D, int jobId, int antId);
int DifxInputSortAntennas(DifxInput *D, int verbose);
int DifxInputSimFXCORR(DifxInput *D);
int DifxInputGetPointingCentreSource(const DifxInput *D, int sourceId);
void DifxInputAllocThreads(DifxInput *D, int nCore);
void DifxInputSetThreads(DifxInput *D, int nThread);
int DifxInputLoadThreads(DifxInput *D);
int DifxInputWriteThreads(const DifxInput *D);
int polMaskValue(char polName);

/* Writing functions */
int writeDifxIM(const DifxInput *D);
int writeDifxCalc(const DifxInput *D);
int writeDifxInput(const DifxInput *D);
int writeDifxPulsarFiles(const DifxInput *D);

/* Remap functions */
void fprintRemap(FILE *out, const char *name, const int *Remap);
void printRemap(const char *name, const int *Remap);
int *newRemap(int nItem);
void deleteRemap(int *Remap);
int *dupRemap(const int *Remap);
int sizeofRemap(const int *Remap);
int reverseRemap(const int *Remap, int y);  /* find index corresponding to y */

char* difx_strlcpy(char *dest, const char *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
