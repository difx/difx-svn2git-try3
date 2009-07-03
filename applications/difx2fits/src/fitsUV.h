#ifndef __FITS_UV_H__
#define __FITS_UV_H__

#include "fits.h"
#include <glob.h>
#include <sys/types.h>
#include "difxio/parsedifx.h"
#include "difx2fits.h"

struct __attribute__((packed)) UVrow
{
	float U, V, W;
	double jd, iat;
	int32_t baseline, filter;/*base256 baseline number*/
	int32_t sourceId1, freqId1;	/* 1-based FITS indices */
	float intTime;
	/* FIXME -- no pulsar gate id! */
	float data[0];	/* this takes no room in the "sizeof" operation */
};

/*maybe this should go in difxio (along with getpolprodID?*/
typedef struct
{
	DifxParameters *dp;
	/*same parameters as header except pol pair is converted to polId
	 * and mjd and seconds are combined*/
	int baseline_num;
	double mjd;
	int config_index;
	int source_index;
	int freq_index;
	int polId;
	int pulsar_bin;
	double data_weight;
	/*double U, V, W*/
} DifxHeader;

/* Information useful for tracking properies of a single visibility record */
/* Information common to more than one visibility record goes in VisBuffer */
typedef struct
{
	float ****data;  /* data[nBand][nPol][nChannel][nComplex]*/
	float *fitsData; /* this will point to floats inside UVrow
	                  * null until writeout */
	int n;/*incremented every time a record is added to data*/
	int difxbl;
	int bl;
	float **weightSum; /* weightSum[nBand][nPol]sum of weights
	                    * points at WEIGHT random parameter  in record*/
	double recweight; 
	double **tEff;/*[nBand][nPol] Effective integration time sum(tIntIn * weight)*/
	//double U, V, W;
	struct UVrow *record;
} DifxVis;

/* VisBuffer:
 * In here go all of the attributes which are common to all of the visibilities
 * within a time integration. buffer is reallocated if necessary for every config 
 * so the number of channels etc. can go in as well.
 */
typedef struct
{
	const DifxInput *D, *CorrModel;
	int jobId;
	int nJob;
	int jobsCompleted;
	int first;	/*indicates buffer is empty*/
	int changed;

	DifxHeader *header;
	DifxConfig *config;

	FILE *in;
	int curFile, nFile;
	glob_t globbuf[1];

	int freqId;/*0 unless more than one frequency setup*/
	int configId;
	int scanId;
	int sourceId;
	int difxbl;

	double mjdStart, mjdCentroid, mjd; /*mjd at start and centre of current integration and currently read mjd*/
	int timeAvg;/*straight from command-line input*/
	int verbose;
	float tIntIn; /*from DifxScan*/
	float tIntOut;/*calculated from timeAvg*/
	double user_scale; /*from command line*/
	double scale;

	/*antenna and baseline indices*/
	int bl;/*base 256 baseline index*/
	int aa1, aa2;/*antenna indices difx order*/
	int a1, a2;/*antenna indices fits order*/

	/*The following are the dimensions of dvs and dv.data*/
	int nBand;
	int nPol;
	int nBl;/*nBaseline + nAntenna (baselines and autocorrs)*/
	int nBinOut;/*number of pulsar bins to buffer*/
	int nOutChan;
	int nComplex;/*number of floats per channel in FITS file*/

	int *binsOut;	/* binsOut[nBinOut]*/
	int **baseline2blId;

	/*The following are used to index  dvs*/
	int blId;	/* generated from baseline2index(a1, a2)*/
	int bandId;	/* translated using baselineFreq2IF */
	int binId;
	int polId;

	/*The following refer to the data coming in*/
	int nFloat; /*number of floats per channel in .difx file */
	int nChanIn;
	float *spectrum;/*buffer to hold the data when it's read from file*/

	/*model indices*/
	int n, nCorr;
	double dt, dtCorr;

	double shiftDelay;
	double **freq;/*freq[nIF][nChanOut]*/

	int flagTransition;
	int nData;/*size of dv's internal data*/
	DifxVis *dv; /*dv currently being written to set by getDifxVis*/

	DifxVis ***dvs;/*dvs[nBl][nBinOut]*/

#ifdef HAVE_FFTW
	Sniffer *S = 0;
#endif

	/*FITS information */
	int polStart;

	/*number of records stored in dvs*/
	int nBuffer; 

	/*counters of records written to FITS file*/
	int nInvalid, nFlagged, nZero, nTrans, nWritten;
} VisBuffer;


DifxVis *newDifxVis(VisBuffer *vb);
void deleteDifxVis(DifxVis *dv);
int DifxVisNextFile(VisBuffer *vb);
int DifxVisNewUVData(VisBuffer *oldvb, VisBuffer *vb);
int DifxVisCollectRandomParams(const VisBuffer *vb);
int addPhase(DifxVis *dv, float phase);
int updateJob(VisBuffer *vb);
int shiftPhase(VisBuffer *vb);

#endif
