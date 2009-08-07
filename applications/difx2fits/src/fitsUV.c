#include "fits.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <complex.h>
#include "config.h"
#include "fitsUV.h"
#ifdef HAVE_FFTW
#include "sniffer.h"
#endif
#define DEBUG

/*Error Codes*/
#define NEXT_FILE            1
#define HEADER_READ_ERROR   -1
#define NEXT_FILE_ERROR     -2
#define DATA_READ_ERROR     -3
#define SKIPPED_RECORD      -4
#define CONFIG_CHANGE_ERROR	-5
#define BAND_ID_ERROR       -6
#define POL_ID_ERROR        -7
#define NFLOAT_ERROR        -8
#define UNDEFINED_ERROR     -9

/*changed codes*/
#define NEW_BIN			0
#define NEW_POL			0
#define NEW_BAND		1
#define NEW_BL			2
#define NEW_MJD			3
#define NEW_TINT		4
#define NEW_CONFIG	5
#define NEW_JOB			6
#define JOBS_DONE   7

const double TWO_PI=2*M_PI;
const double cLight=2.99792458e8;	/* speed of light in m/s */

int baseline2index(int a1, int a2, int nAnt)
{
	/*0-index in, 0-index out*/
	if(a1 < a2)
	{
		return a1*(2*(nAnt-1) - (a1-1))/2 + (a2-a1-1);
	}
	if(a1 > a2)
	{
		return a2*(2*(nAnt-1) - (a2-1))/2 + (a1-a2-1);
	}
	if(a1 == a2)
	{
		return nAnt*(nAnt-1)/2 + a1;
	}
	return -1;
}

int getFrequencies(VisBuffer *vb)
{
	/*FIXME put ref_pixel in algorithm */
	int i, j;
	double chanBW, bandFreq;
	//float ref_pixel = 1.0;

#ifdef DEBUG
			printf("band\tchan\tfrequency\n");
#endif
/* n.b. for uvshifting purposes, it is important that we calculate the
 * *incoming* data frequencies. I.e. before spectral averaging
 */
	for(i = 0; i < vb->nBand; i++) 
	{
		bandFreq = vb->config->IF[i].freq;
		chanBW = (vb->config->IF[i].bw/vb->D->nInChan);/*in MHz*/
		for(j = 0; j < vb->D->nInChan; j++)
		{
			if(vb->config->IF[i].sideband == 'U')
			{
				vb->freq[i][j] = bandFreq + j * chanBW;
			}
			else
			{
				vb->freq[i][j] = bandFreq - j * chanBW;
			}
#ifdef DEBUG
			printf("%d\t%d\t%f\n", i, j, vb->freq[i][j]);
#endif
			

		}
	}
	return 0;
}

/* Use Cramer's rule to evaluate polynomial */
double evalPoly(const double *p, int n, double x)
{
	double y;
	int i;

	if(n == 1)
	{
		return p[0];
	}

	y = p[n-1];

	for(i = n-2; i >= 0; i--)
	{
		y = x*y + p[i];
	}

	return y;
}

static int DifxVisInitData(VisBuffer *vb, DifxVis *dv)
{
	int i, j, k;
	int n;
	float ****datamap;
	float *data0, *weight0;

	/* Allocate tEff*/
	dv->tEff = (double **)calloc(vb->nBand, sizeof(double *));
	if(!dv->tEff)
	{
		fprintf(stderr, "Error: DifxVisInitData: dv->tEff=calloc failed, size=%d\n",
			vb->nBand * sizeof(double *));
		return 0;
	}
	for(i = 0; i < vb->nBand; i++)
		{
			dv->tEff[i] = (double *)calloc(vb->nPol, sizeof(double));
		}

	/* Allocate data */
	vb->nData = vb->nBand*vb->nPol*vb->D->nOutChan*vb->nComplex;
	n = vb->nData + vb->nBand * vb->nPol;/*weights and visibilities*/
	dv->record = calloc(n * sizeof(float) + sizeof(struct UVrow), 1);
	if(!dv->record)
	{
		fprintf(stderr, "Error: DifxVisInitData: dv->record=calloc failed, size=%d\n",
			n * sizeof(float) + sizeof(struct UVrow));
		return 0;
	}

	/* Set up data array pointing to record->data*/
	weight0 = dv->record->data;
	data0 = weight0 + (vb->nBand*vb->nPol);

	datamap = (float ****)calloc(vb->nBand, sizeof(float ***));
	if(!datamap)
	{
		fprintf(stderr, "Error: DifxVisInitData: dv->data=calloc failed, size=%d\n",
			n * sizeof(float) + sizeof(struct UVrow));
		return 0;
	}
	for(i = 0; i < vb->nBand; i++)
	{
		datamap[i] = (float ***)calloc(vb->nPol, sizeof(float **));
		if(!datamap[i])
		{
			fprintf(stderr, "Error: DifxVisInitData: dv->data[%d]=calloc failed, size=%d\n",
				i, sizeof(float)*vb->nPol);
			return 0;
		}
		for(j = 0; j < vb->nPol; j++)
		{
			datamap[i][j] = (float **)calloc(vb->nOutChan, sizeof(float *));
			if(!datamap[i][j])
			{
				fprintf(stderr, "Error: DifxVisInitData: dv->data[%d][%d]=calloc failed, size=%d\n",
					i, j, sizeof(float)*vb->nOutChan);
				return 0;
			}
			for(k = 0; k < vb->nOutChan; k++)
			{
				datamap[i][j][k] = data0 +
							 i*vb->nPol*vb->D->nOutChan*vb->nComplex +
							 j*vb->D->nOutChan*vb->nComplex +
							 k*vb->nComplex;
			}
		}
	}
	dv->data = datamap;

	/* Set up weight arrays pointing to record->data*/
	dv->weightSum = (float **)calloc(vb->nBand, sizeof(float *));
	for(i = 0; i < vb->nBand; i++)
	{
		dv->weightSum[i] = weight0 + i*vb->nPol;
	}
	//dv->n = 0;

	return 0;
}

static void startGlob(VisBuffer *vb)
{
	char *globstr;
	const char suffix[] = ".difx/DIFX*";

	globstr = calloc(strlen(vb->CorrModel->job[vb->jobId].fileBase)+
		strlen(suffix)+8, 1);

	sprintf(globstr, "%s%s", vb->CorrModel->job[vb->jobId].fileBase, suffix);

	glob(globstr, 0, 0, vb->globbuf);
	vb->nFile = vb->globbuf->gl_pathc;
	if(vb->nFile == 0)
	{
		fprintf(stderr, "Error: No files found for job %d %s\n",
				vb->jobId, vb->CorrModel->job[vb->jobId].fileBase);
	}
	vb->globbuf->gl_offs = 0;

	free(globstr);
}

int nextFile(VisBuffer *vb)
{
	/*find next file, moving to the next job if necessary*/
	/*in the case of a new job, jobId, globbuff, are updated*/
	printf("    JobId %d File %d/%d finished\n", 
		vb->jobId,
		vb->curFile+1, vb->nFile);
	if(vb->in)
	{
		fclose(vb->in);
		vb->in = 0;
	}
	vb->curFile++;
	if(vb->jobId < 0 || vb->curFile >= vb->nFile)
	{
		vb->changed = NEW_JOB;
		if(vb->jobId > 0) 
		{
			printf("    JobId %d done.\n", vb->jobId);
		}
		vb->jobId = updateJob(vb);
		if(vb->jobId == -1)
		{
			return JOBS_DONE;
		}
		vb->curFile = 0;
	}
	printf("    Opening JobId %d File %d/%d : %s\n", 
		vb->jobId,
		vb->curFile+1, vb->nFile,
		vb->globbuf->gl_pathv[vb->curFile]);
	vb->in = fopen(vb->globbuf->gl_pathv[vb->curFile], "r");
	if(!vb->in)
	{
		fprintf(stderr, "Error opening file : %s\n", 
			vb->globbuf->gl_pathv[vb->curFile]);
		return -1;
	}
	return 0;
}

double getScaleFactor(const DifxInput *D, double s, int timeAvg, int verbose)
{
	double scale;

	if(D->inputFileVersion == 0) /* Perth merge and after */
	{
		scale = 4.576*D->nOutChan/(D->nInChan*timeAvg);
	}
	else
	{
		scale = 7.15/(D->chanBW*6.25e6*D->config[0].tInt*D->specAvg);
	}
	if(D->quantBits == 2)
	{
		scale /= (3.3359*3.3359);
	}

	if(s > 0.0)
	{
		if(verbose > 0)
		{
			printf("      Overriding scale factor %e with %e\n",
				scale, s);
		}
		scale = s;
	}

	return scale;
}

int updateBand(VisBuffer *vb)
{
	vb->bandId = vb->config->baselineFreq2IF[vb->aa1][vb->aa2][vb->header->freq_index];
	if(vb->bandId <  0 || vb->bandId >= vb->D->nFreq)
	{
		fprintf(stderr, "Parameter problem: bandId should be in "
				"[0, %d), was %d\n", 
				vb->D->nFreq, vb->bandId);
		
		return BAND_ID_ERROR;
	}
	else
	{
		return 0;
	}
}

int updateBaseline(VisBuffer *vb)
{
	int a1, a2, d1, d2, bl;
	vb->aa1 = a1 = (vb->header->baseline_num/256) - 1;
	vb->aa2 = a2 = (vb->header->baseline_num%256) - 1;


	if(a1 < 0 || a1 >= vb->config->nAntenna ||
	   a2 < 0 || a2 >= vb->config->nAntenna)
	{
		printf("Error: illegal baseline %d -> %d-%d\n", bl, a1, a2);
		return -8;
	}

	/* translate from .input file antId to index for 
	 * D->antenna via dsId */
	d1 = vb->config->ant2dsId[a1];
	d2 = vb->config->ant2dsId[a2];
	if(d1 < 0 || d1 >= vb->D->nDatastream || 
	   d2 < 0 || d2 >= vb->D->nDatastream)
	{
		printf("Error: baseline %d -> datastreams %d-%d\n",
			bl, d1, d2);
		return -9;
	}
	vb->a1 = vb->D->datastream[d1].antennaId;
	vb->a2 = vb->D->datastream[d2].antennaId;

	/*used to index dvs*/
	vb->blId = vb->baseline2blId[vb->aa1][vb->aa2];
#ifdef DEBUG
	printf("\t\t\taa1=%d aa2=%d blid=%d\n", vb->aa1, vb->aa2, vb->blId);
	printf("\t\t\t a1=%d  a2=%d\n",vb->a1, vb->a2);
#endif

	vb->dv = vb->dvs[vb->blId][0];
	/*this is the one actually written out to fits*/
	//vb->dv = vb->dvs[vb->blId][vb->binId];
	vb->bl = (vb->a1+1)*256 + (vb->a2+1);
	vb->difxbl = vb->header->baseline_num;
	return 0;
}

int updateShiftDelay(VisBuffer *vb)
{
	if (vb->a1 == vb->a2 || vb->D == vb->CorrModel)
	{
		/*skip for autocorrs or if no shifting*/
		vb->shiftDelay = 0;
		return 0;
	}
	/*1 and 2 are the two datastreams*/
	/*Corr refers to the model used for correlation*/
	double delay1, delayCorr1;
	double delay2, delayCorr2;
	double dt, dtCorr;
	const DifxPolyModel *im1, *im2;
	const DifxPolyModel *imCorr1, *imCorr2;
	int terms1, terms2;
	int termsCorr1, termsCorr2;
	int n, nCorr;

	n = getDifxScanIMIndex(vb->D->scan + vb->scanId, vb->mjdCentroid, &dt);
	nCorr = getDifxScanIMIndex(vb->D->scan + vb->scanId, vb->mjdCentroid, &dtCorr);

	im1 = vb->D->scan[vb->scanId].im[vb->aa1];
	im2 = vb->D->scan[vb->scanId].im[vb->aa2];
	imCorr1 = vb->CorrModel->scan[vb->scanId].im[vb->aa1];
	imCorr2 = vb->CorrModel->scan[vb->scanId].im[vb->aa2];
	terms1 = im1->order + 1;
	terms2 = im2->order + 1;
	termsCorr1 = imCorr1->order + 1;
	termsCorr2 = imCorr2->order + 1;
	delay1    = evalPoly(im1[n].delay, terms1, dt);
	delay2    = evalPoly(im2[n].delay, terms2, dt);
	delayCorr1 = evalPoly(imCorr1[nCorr].delay, termsCorr1, dtCorr);
	delayCorr2 = evalPoly(imCorr2[nCorr].delay, termsCorr2, dtCorr);
	vb->shiftDelay =  (delay2 - delay1) - (delayCorr2 - delayCorr1);

	return 0;
}

int updateInt(VisBuffer *vb)
{
	int scanId, scanId2;
	vb->mjdStart = vb->header->mjd - fmod(vb->header->mjd - vb->D->job[vb->jobId].mjdStart, (vb->tIntOut/86400.0));
	vb->mjdCentroid = vb->mjdStart + (vb->tIntOut/(2*86400.0));

	scanId = DifxInputGetScanIdByJobId(vb->D, vb->mjdStart, vb->jobId);
	scanId2 = DifxInputGetScanIdByJobId(vb->D, vb->mjdStart+vb->tIntOut/86400.0, vb->jobId);
	if(scanId != scanId2)
	{
		fprintf(stderr, "Error: scan change in middle of integration: Id %d changes to %d\n", scanId, scanId2);
	}
	return 0;
}

int updateScan(VisBuffer*vb)
{
	int scanId;
	DifxScan *scan;

	scanId = DifxInputGetScanIdByJobId(vb->D, vb->header->mjd, vb->jobId);
	if(scanId != vb->scanId )
	{
#ifdef DEBUG
		printf("newscan\n");
#endif
		vb->scanId = scanId;
		if(!vb->first && vb->scanId < 0)
		{
			fprintf(stderr, "Error: scan outside range on  mjd=%12.6f, jobId=%d\n", vb->header->mjd, vb->jobId);
		}
		scan = vb->D->scan + vb->scanId;
		vb->sourceId = scan->sourceId;
		vb->configId = scan->configId;
		vb->config = vb->D->config + vb->configId;
	}
	return(0);
}
int updateConfig(VisBuffer *oldvb, VisBuffer *vb)
{
	int **map;
	int a1, a2;
	int i;
	DifxScan *scan, *CorrScan;
	/* n.b Most of the if tests are meaningless for now (D never changes) */

	if(vb->verbose >= 1)
	{
		printf("        MJD=%11.5f jobId=%d scanId=%d Source=%s  FITS SourceId=%d\n", 
			vb->mjdCentroid, vb->jobId, vb->scanId, vb->D->scan[vb->scanId].name, vb->D->source[vb->sourceId].fitsSourceId+1);
	}

	vb->freqId = vb->config->freqId;
	vb->tIntIn = vb->config->tInt;
	vb->tIntOut = vb->tIntIn * vb->timeAvg;
	if(oldvb->first)
	{
		oldvb->freqId = vb->freqId;
		oldvb->tIntOut = vb->tIntOut;
	}

	/* allocate baseline index. see difxio/difx_if.c*/
	if(vb->first || vb->D->nAntenna != oldvb->D->nAntenna)
	{
		map = (int **)calloc(vb->D->nAntenna+1, sizeof(int *));
		for(a1 = 0; a1 < vb->D->nAntenna; a1++)
		{
			map[a1] = (int *)calloc(vb->D->nAntenna+1, sizeof(int));
			for(a2 = 0; a2 < vb->D->nAntenna; a2++)
			{
				map[a1][a2] = baseline2index(a1, a2, vb->D->nAntenna);
			}
		}
		vb->baseline2blId = map;
		if(oldvb->first)
		{
			oldvb->baseline2blId = map;
		}
	}
	/* allocate spectrum */
	if(vb->first || vb->D->nInChan != oldvb->D->nInChan)
	{
		vb->spectrum = (float *)calloc(3*vb->D->nInChan, sizeof(float));
		if(oldvb->first)
		{
			oldvb->spectrum = vb->spectrum;
		}
	}

	if(vb->first || vb->nBand != oldvb->nBand ||
		vb->D->nOutChan != oldvb->D->nInChan)
	{
		/*allocate freq*/
		vb->freq = (double **)calloc(vb->nBand, sizeof(double *));
		for(i = 0; i < vb->nBand; i++)
		{
			vb->freq[i] = (double *)calloc(vb->D->nInChan, sizeof(double));
		}
		/*recalculate frequencies*/
		getFrequencies(vb);
		if(oldvb->first)
		{
			oldvb->freq = vb->freq;
		}
	}
	if(vb->first || vb->nBl != oldvb->nBl ||
		vb->nBinOut != oldvb->nBinOut)
	{
		vb->dvs = (DifxVis ***)calloc(vb->nBl, sizeof(DifxVis **));
		for(i = 0; i < vb->nBl; i++)
		{
			vb->dvs[i] = (DifxVis **)calloc(vb->nBinOut, sizeof(DifxVis *));
		}
		if(oldvb->first)
		{
			oldvb->dvs = vb->dvs;
		}
	}

	CorrScan = vb->CorrModel->scan + vb->scanId;
	scan = vb->D->scan + vb->scanId;
	
	/*
	vb->n = getDifxScanIMIndex(scan, vb->header->mjd, &vb->dt);
	if(vb->n < 0)
	{
		fprintf(stderr, "Error: updateConfig: interferometer model index out of range: scanId=%d mjd=%12.6f %d-%d \n",
		vb->scanId, vb->header->mjd,
		vb->aa1, vb->aa2);
	}
	vb->nCorr = getDifxScanIMIndex(CorrScan, vb->header->mjd, &vb->dtCorr);
	if(vb->nCorr < 0)
	{
		fprintf(stderr, "Error: updateConfig: interferometer model index out of range: scanId=%d mjd=%12.6f %d-%d \n",
		vb->scanId, vb->header->mjd,
		vb->aa1, vb->aa2);
	}
	*/

	vb->scale = getScaleFactor(vb->D, vb->user_scale, vb->timeAvg, vb->verbose);
	vb->nOutChan = vb->D->nOutChan;
	if(oldvb->first)
	{
		/*
		oldvb->n = vb->n;
		oldvb->nCorr = vb->nCorr;
		*/

		oldvb->scale = vb->scale;
		oldvb->nOutChan = vb->nOutChan;
	}
	return 0;
}

int updateJob(VisBuffer *vb)
{
	int j;
	double mjd;
	int bestmjd = 1.0e9;
	int bestj = -1;
	vb->jobsCompleted++;
	if(vb->jobsCompleted >= vb->nJob)
	{
		vb->changed = JOBS_DONE;
		return -1;
	}
	/*FIXME go back to old algorithm*/
	for(j = 0; j < vb->nJob; j++)
	{
		mjd = vb->D->job[j].mjdStart;
		if(mjd > vb->mjdCentroid && mjd < bestmjd)
		{
			bestmjd = mjd;
			bestj = j;
		}
	}
	if(bestj == -1)
	{
		vb->changed = JOBS_DONE;
		return bestj;
	}
	printf("    Starting JobId %d mjdStart %5.5f \n",
		       bestj, vb->D->job[bestj].mjdStart);
	return bestj;
}
	
int updateVisBuffer(VisBuffer *oldvb, VisBuffer *vb)
{
	DifxHeader *h, *oldh;
	int newBand = 0; int newBl = 0; int newMjd = 0; int newInt = 0; int newScan = 0; int newConfig = 0; 
	h = vb->header; oldh = oldvb->header;

	if(vb->first || h->config_index != oldh->config_index)
	{
		vb->changed = NEW_CONFIG;
		newBand = newBl = newMjd = newInt = newScan = newConfig = 1;
	}
	else
	{
		vb->changed = NEW_POL;
		if(h->freq_index != oldh->freq_index)
		{
			vb->changed = NEW_BAND;
			newBand = 1;
		}
		if(h->baseline_num != oldh->baseline_num)
		{
			newBl = 1;
			vb->changed = NEW_BL;
#ifdef DEBUG
			printf("\t\t\tnewbl %d-%d\n", h->baseline_num/256, h->baseline_num%256);
#endif
		}		
		if(fabs(h->mjd - oldh->mjd) > 1.0/86400000.0)
		{
			updateScan(vb);
			newMjd = 1;
			vb->changed = NEW_MJD;
#ifdef DEBUG
			printf("\t\tnewmjd: old=%d %06.2f new=%d %06.2f\n",
				(int)oldh->mjd, fmod(oldh->mjd, 1.0) * 86400.0,
				(int)h->mjd, fmod(h->mjd, 1.0) * 86400.0);
#endif
			if(h->mjd - oldvb->mjdStart - oldvb->tIntOut/86400.0 > 0)
			{
				newInt = 1;
				vb->changed = NEW_TINT;
				updateInt(vb);
#ifdef DEBUG
				printf("\tnewtint mjdStart=%d %06.2f mjdCentroid=%d %06.2f\n", 
						(int)vb->mjdStart, fmod(vb->mjdStart, 1.0) * 86400.0,
						(int)vb->mjdCentroid, fmod(vb->mjdCentroid, 1.0) * 86400.0);
#endif
				if(oldvb->first)
				{
					oldvb->scanId = vb->scanId;
				}
				if(vb->configId != oldvb->configId)
				{
					newConfig = 1;
					vb->changed = NEW_CONFIG;
#ifdef DEBUG
					printf("newconfig\n");
#endif
				}
			}
		}		
	}

	if(newScan)
	{
		updateScan(vb);
		if(oldvb->first)
		{
			oldvb->configId = vb->configId;
			oldvb->config = vb->config;
			oldvb->sourceId = vb->sourceId;
		}
	}
	if(newConfig)
	{
		updateConfig(oldvb, vb);
		vb->changed = NEW_CONFIG;
	}
	if(newInt)
	{
		updateInt(vb);
	}

	/*check header source and config are what we expect*/
	if(vb->header->config_index != vb->configId)
	{
		//can't continue -- we don't know which scan we're on
		//so we don't have correct model information
		fprintf(stderr, "Error: configId in .difx doesn't match expected. .difx id %d instead of %d\n",
				vb->header->config_index, vb->configId);
		return SKIPPED_RECORD;
	}
	if(newBl)
	{
		updateBaseline(vb);
		updateShiftDelay(vb);
	}
	if(newMjd)
	{
		updateShiftDelay(vb);
	}
	if(newBand)
	{
		updateBand(vb);
	}
	vb->polId = h->polId;
	vb->binId = h->pulsar_bin;
#ifdef DEBUG
	printf("\n\t\t\t\tnewband/pol/bin band=%d pol=%d bin=%d\n", vb->bandId, vb->polId, vb->binId);
#endif
	vb->first = 0;
	oldvb->first = 0;
	return 0;
}

int getPolStart(const DifxInput *D)
{
	int i, c;
	int polMask = 0;
        for(c = 0; c < D->nConfig; c++)
        {
                for(i = 0; i < D->nIF; i++)
                {
                        if(D->config[c].IF[i].pol[0] == 'R' ||
                           D->config[c].IF[i].pol[1] == 'R')
                        {
                                polMask |= 0x01;
                        }
                        if(D->config[c].IF[i].pol[0] == 'L' ||
                           D->config[c].IF[i].pol[1] == 'L')
                        {
                                polMask |= 0x02;
                        }
                        if(D->config[c].IF[i].pol[0] == 'X' ||
                           D->config[c].IF[i].pol[1] == 'X')
                        {
                                polMask |= 0x10;
                        }
                        if(D->config[c].IF[i].pol[0] == 'Y' ||
                           D->config[c].IF[i].pol[1] == 'Y')
                        {
                                polMask |= 0x20;
                        }
                }
        }

	/* check for polarization confusion */
	if( ((polMask & 0x0F) > 0 && (polMask & 0xF0) > 0) || polMask == 0 )
	{
		fprintf(stderr, "Error: bad polarization combinations : 0x%x\n",
			polMask);
		return 0;
	}

	if(polMask & 0x01)
	{
		return -1;
	}
	else if(polMask & 0x02)
	{
		return -2;
	}
	else if(polMask & 0x10)
	{
		return -5;
	}
	else /* must be YY only! who would do that? */
	{
		return -6;
	}
}

VisBuffer *newVisBuffer(const DifxInput *D, const DifxInput *CorrModel, int nComplex, int timeAvg, float scale, int pulsarBin, int verbose)
{
	/* This should set *everything* that is the same for all jobs
	 * Anything else is set in updateVisBuffer etc. */

	VisBuffer *vb;
	vb = (VisBuffer *)calloc(1, sizeof(VisBuffer));
	if(!vb)
	{
		fprintf(stderr, "Error: newVisBuffer: vb=calloc failed, size=%d\n",
			sizeof(VisBuffer));
		return 0;
	}

	vb->D = D;
	vb->CorrModel = CorrModel;
	vb->jobId = -1;
	vb->nJob = D->nJob;
	vb->jobsCompleted = -1;
	vb->first = 1;
	vb->changed = UNDEFINED_ERROR;
	vb->difxbl = 0;

	vb->header = (DifxHeader *)calloc(1, sizeof(DifxHeader));
	if(!vb)
	{
		fprintf(stderr, "Error: newVisBuffer: vb->header=calloc failed, size=%d\n",
			sizeof(DifxHeader));
		return 0;
	}
	vb->header->dp = newDifxParameters();
	vb->config = 0;

	vb->curFile = -1;

	vb->freqId = -1;
	vb->configId = -1;
	vb->scanId = -1;

	vb->timeAvg = timeAvg;
	vb->user_scale = scale;
	vb->scale = getScaleFactor(D, scale, timeAvg, verbose);
	vb->polStart = getPolStart(D);

	vb->nBl = D->nBaseline + D->nAntenna;
	vb->nBand = D->nIF;
	vb->nPol = D->nPol;
	vb->nBinOut = 1;
	vb->nComplex = 2;

	vb->binsOut = (int*) calloc(1, sizeof(int));
	vb->binsOut[0] = pulsarBin;

	vb->baseline2blId = 0;
	vb->freq = 0;

	vb->binId = vb->binsOut[0];

	vb->nFloat = 2;

	vb->shiftDelay = 0;

	return vb;
}

int moveVisBuffer(VisBuffer *vb, VisBuffer *oldvb)
{
	int i, j;
	int **map;
	oldvb->jobId = vb->jobId;
	oldvb->scanId = vb->scanId;
	oldvb->sourceId = vb->sourceId;
	oldvb->configId = vb->configId;
	oldvb->blId = vb->blId;
	oldvb->bandId = vb->bandId;
	oldvb->polId = vb->polId;
	oldvb->binId = vb->binId;
	oldvb->mjdCentroid = vb->mjdCentroid;
	oldvb->mjdStart = vb->mjdStart;
	/*
	oldvb->n = vb->n;
	oldvb->nCorr = vb->nCorr;
	*/
	oldvb->tIntIn = vb->tIntIn;
	oldvb->tIntOut = vb->tIntOut;
	oldvb->shiftDelay = vb->shiftDelay;

	/*first delete all arrays in oldvb which aren't used by vb*/
	//assume that D is always the same
	/*
	if(oldvb->D != vb->D)
	{
		deleteDifxInput(oldvb->D)
	}
	if(oldvb->CorrModel != vb->CorrModel)
	{
		deleteDifxInput(oldvb->CorrModel)
	}
	*/
	//resetDifxParameters(oldvb->header->dp);
		//deleteDifxParameters(oldvb->header->dp);
		//free(oldvb->header);
	/*
	if(oldvb->config != vb->config)
	{
		deleteDifxConfig(oldvb->config);
	}
	*/
	if(oldvb->in != vb->in)
	{
		if(oldvb->in)
		{
			fclose(oldvb->in);
		}
		else
		{
			oldvb->in = vb->in;
		}
	}
	if(oldvb->globbuf != vb->globbuf)
	{
		globfree(oldvb->globbuf);
	}

	if(oldvb->binsOut[0] != vb->binsOut[0])
	{
		free(oldvb->binsOut);
	}
	
	if(oldvb->baseline2blId != vb->baseline2blId)
	{
		map = oldvb->baseline2blId;
		if(map)
		{
			for(i = 0; map[i]; i++)
			{
				free(map[i]);
			}
			free(map);
		}
	}
	if(oldvb->spectrum != vb->spectrum)
	{
		free(oldvb->spectrum);
	}
	if(oldvb->freq != vb->freq)
	{
		for(i = 0; i < vb->nBand; i++)
		{
			free(vb->freq[i]);
		}
	}
	if(oldvb->dvs != vb->dvs)
	{
		for(i = 0; i < vb->nBl; i++)
		{
			for(j=0; j < vb->nBinOut; j++)
			{
				if(vb->dvs[i][j])
					{
						deleteDifxVis(vb->dvs[i][j]);
					}
			}
			free(vb->dvs[i]);
		}
		free(vb->dvs);
	}

#ifdef HAVE_FFTW
	if(vb->S != oldvb->S)
	{
		deleteSniffer(vb->S);
	}
#endif
	
	return 0;
}

int deleteVisBuffer(VisBuffer *vb)
{
	int i, j;
	int **map;

	if(!vb)
	{
		return 0;
	}

	if(vb->in)
	{
		fclose(vb->in);
	}
	free(vb);

	if(vb->header)
	{
		if(vb->header->dp)
		{
			deleteDifxParameters(vb->header->dp);
		}
		free(vb->header);
	}

	globfree(vb->globbuf);

	map = vb->baseline2blId;
        if(map)
        {
                for(i = 0; map[i]; i++)
                {
                        free(map[i]);
                }
                free(map);
        }

	/*delete dvs*/
	for(i = 0; i < vb->nBl; i++)
	{
		for(j=0; j < vb->nBinOut; j++)
		{
			if(vb->dvs[i][j])
				{
					deleteDifxVis(vb->dvs[i][j]);
				}
		}
		free(vb->dvs[i]);
	}
	free(vb->dvs);

	if(vb->freq)
	{
		for(i = 0; i < vb->nBand; i++)
		{
			free(vb->freq[i]);
		}
	}

	if(vb->header)
	{
		free(vb->header);
	}
	return 0;
}

DifxVis *newDifxVis(VisBuffer *vb)
{
	DifxVis *dv;

	dv = (DifxVis *)calloc(1, sizeof(DifxVis));
	if(!dv)
	{
		fprintf(stderr, "Error: newDifxVis: dv=calloc failed, size=%d\n",
			sizeof(DifxVis));
		return 0;
	}
	DifxVisInitData(vb, dv);

	return dv;
}

void deleteDifxVis(DifxVis *dv)
{
	if(!dv)
	{
		return;
	}

	if(dv->record)
	{
		free(dv->record);
	}
	
	if(dv->data)
	{
		free(dv->data);
	}
	free(dv);
}


int getPolProdId(const VisBuffer *vb, const char *polPair)
{
	const char polSeq[8][4] = 
		{"RR", "LL", "RL", "LR", "XX", "YY", "XY", "YX"};
	int p;

	for(p = 0; p < 8; p++)
	{
		if(strncmp(polPair, polSeq[p], 2) == 0)
		{
			p = (p+1) + vb->polStart;
			
			if(p < 0 || p >= vb->D->nPolar)
			{
				return -1;
			}
			else
			{
				return p;
			}
		}
	}
	return -2;
}

static int storevis(VisBuffer *vb)
{
	const DifxInput *D;
	int isLSB;
	int startChan;
	int stopChan;
	int i, j, k;
	double weight;
	DifxVis * dv;

	D = vb->D;
	weight = vb->header->data_weight;

	if(!vb->dvs[vb->blId][0])
	{

#ifdef DEBUG
		printf("newDifxVis\n");
#endif
		vb->dvs[vb->blId][0] = newDifxVis(vb);
		vb->dvs[vb->blId][0]->difxbl = vb->difxbl;
		vb->dvs[vb->blId][0]->bl = vb->bl;
	}

#ifdef DEBUG
	printf("blId=%d\n", vb->blId);
#endif
	dv = vb->dvs[vb->blId][0];
	/* scale data by weight */
	for(i = 0; i < vb->D->nInChan; i++)
	{
		vb->spectrum[i*vb->nComplex] *= weight;
		vb->spectrum[i*vb->nComplex+1] *= weight;
	}

	isLSB = vb->config->IF[vb->bandId].sideband == 'L';
	startChan = D->startChan;
	stopChan = startChan + D->nOutChan*D->specAvg;

	//dv->weightSum[D->nPolar*vb->bandId + vb->polId] =
	//	vb->recweight;
	dv->tEff[vb->bandId][vb->polId] += vb->tIntIn * weight;
	dv->weightSum[vb->bandId][vb->polId] += weight;
	shiftPhase(vb);
	
	for(i = startChan; i < stopChan; i++)
	{
		if(isLSB)
		{
			j = (stopChan - 1 - i) / vb->D->specAvg;
		}
		else
		{
			j = i / vb->D->specAvg;
		}

		for(k = 0; k < vb->nComplex; k++)
		{
			/* swap phase/uvw for FITS-IDI conformance */
			if(k % 3 == 1 && !isLSB)
			{
				dv->data[vb->bandId][vb->polId][j][k]-=
					vb->scale*
					vb->spectrum[vb->nComplex*i+k];
			}
			else
			{
				dv->data[vb->bandId][vb->polId][j][k]+= 
					vb->scale*
					vb->spectrum[vb->nComplex*i+k];
			}
		}
	}
	dv->n += 1;
	return 0;
}

int readDifxHeader(VisBuffer *vb)
{
	const char difxKeys[][MAX_DIFX_KEY_LEN] = 
	{
		"BASELINE NUM",
		"MJD",
		"SECONDS",
		"CONFIG INDEX",
		"SOURCE INDEX",
		"FREQ INDEX",
		"POLARISATION PAIR",
		"PULSAR BIN",
		"DATA WEIGHT",
		"U (METRES)",
		"V (METRES)",
		"W (METRES)"
	};

	const char difxKeysOrig[][MAX_DIFX_KEY_LEN] = 
	{
		"BASELINE NUM",
		"MJD",
		"SECONDS",
		"CONFIG INDEX",
		"SOURCE INDEX",
		"FREQ INDEX",
		"POLARISATION PAIR",
		"PULSAR BIN",
		"WEIGHTS WRITTEN",
		"U (METRES)",
		"V (METRES)",
		"W (METRES)"
	};
	const int N_DIFX_ROWS = sizeof(difxKeys)/sizeof(difxKeys[0]);
	int rows[N_DIFX_ROWS];
	int i, N;
	int v = 1;
	char line[100];
	DifxHeader *h;

	h = vb->header;
	resetDifxParameters(h->dp);

	if(!vb->in)
	{
#ifdef DEBUG
		printf("newfile\n");
#endif
		v = nextFile(vb);
	}
	for(i = 0; i < 13; i++)
	{
		fgets(line, 99, vb->in);
		if(feof(vb->in))
		{
			/* EOF should not happen in middle of text */
			if(i != 0)
			{
				fprintf(stderr, "Error reading header. EOF in line %d\n", i);
				fprintf(stderr, "\t%s\n", line);
				vb->changed = HEADER_READ_ERROR;
				return -1;
			}
			v = nextFile(vb);
			if(v != 0)
			{
				if(vb->changed == JOBS_DONE)
				{
					return JOBS_DONE;
				}
				vb->changed = NEXT_FILE_ERROR;
				return NEXT_FILE_ERROR;
			}
			fgets(line, 99, vb->in);
		}
		DifxParametersaddrow(h->dp, line);
	}

	/* parse the text header */
	if(vb->D->inputFileVersion == 0)
	{
		N = DifxParametersbatchfind(h->dp, 0, difxKeys, 
			N_DIFX_ROWS, rows);
	}
	else
	{
		N = DifxParametersbatchfind(h->dp, 0, difxKeysOrig, 
			N_DIFX_ROWS, rows);
	}
	if(N < N_DIFX_ROWS)
	{
		printf("ERROR: N=%d < N_DIFX_ROWS=%d\n", N, N_DIFX_ROWS);
		vb->changed = HEADER_READ_ERROR;
		return HEADER_READ_ERROR;
	}

	h->baseline_num = atoi(DifxParametersvalue(h->dp, rows[0]));
	h->mjd          = atoi(DifxParametersvalue(h->dp, rows[1])) +
	                atof(DifxParametersvalue(h->dp, rows[2]))/86400.0;
	h->config_index = atoi(DifxParametersvalue(h->dp, rows[3]));
	h->source_index = atoi(DifxParametersvalue(h->dp, rows[4]));
	h->freq_index   = atoi(DifxParametersvalue(h->dp, rows[5]));
	h->polId        = getPolProdId(vb, DifxParametersvalue(h->dp, rows[6]));
	h->pulsar_bin   = atoi(DifxParametersvalue(h->dp, rows[7]));
	//h->U            = atof(DifxParametersvalue(h->dp, rows[9]));
	//h->V            = atof(DifxParametersvalue(h->dp, rows[10]));
	//h->W            = atof(DifxParametersvalue(h->dp, rows[11]));

	if(vb->D->inputFileVersion == 0)
	{
		h->data_weight = atof(DifxParametersvalue(h->dp, rows[8]));
	}
	else
	{
		h->data_weight = 1.0;
	}
	//printf("baseline_num %d\n", h->baseline_num);
	//printf("mjd          %f\n", h->mjd);
	//printf("config_index %d\n", h->config_index);
	//printf("source_index %d\n", h->source_index);
	//printf("freq_index   %d\n", h->freq_index);
	//printf("polId        %d\n", h->polId);
	//printf("pulsar_bin   %d\n", h->pulsar_bin);
	//printf("data_weight  %f\n", h->data_weight);
	return 0;
}

int DifxVisNewUVData(VisBuffer *oldvb, VisBuffer *vb)
{

	int i, i1, v;
	int readSize;
	int error = 0;

	error = readDifxHeader(vb);
	if(error)
	{
		return error;
	}

	/*change parameters of new vb to match header*/
	updateVisBuffer(oldvb, vb);

	/*read data*/
	/* if chan weights are written the data volume is 3/2 as large */
	/* for now, force nFloat = 2 (one weight for entire vis record) */
	vb->nFloat = 2;
	readSize = vb->nFloat * vb->D->nInChan;
	v = fread(vb->spectrum, sizeof(float), readSize, vb->in);
	if(v < readSize)
	{
		vb->changed = DATA_READ_ERROR;
		return DATA_READ_ERROR;
	}

	if(vb->nFloat > vb->nComplex)
	{
		fprintf(stderr, "nFloat > vb->nComplex\n");
		vb->changed = NFLOAT_ERROR;
		return NFLOAT_ERROR;
	}

	if(vb->header->polId  <  0 || vb->header->polId  >= vb->D->nPolar)
	{
		fprintf(stderr, "Parameter problem: polId should be in "
				"[0, %d), was %d\n",
				vb->D->nPolar, vb->header->polId);
		vb->changed = POL_ID_ERROR;
		return POL_ID_ERROR;
	}
	/*skip if wrong pulsar bin */
	if(vb->header->pulsar_bin != vb->binsOut[0])
	{
#ifdef DEBUG
		printf("skipped: wrong bin %d instead of %d\n", vb->header->pulsar_bin, vb->binsOut[0]);
#endif
		vb->changed = SKIPPED_RECORD;
		return SKIPPED_RECORD;
	}

	/* reorder data and set weights if weights not provided */
	if(vb->nFloat < vb->nComplex)
	{
		for(i = 3*vb->D->nInChan-3; i > 0; i -= 3)
		{
			i1 = i*2/3;
			vb->spectrum[i+2] = 1.0;                 /* weight */
			vb->spectrum[i+1] = vb->spectrum[i1+1];  /* imag */
			vb->spectrum[i]   = vb->spectrum[i1];    /* real */
		}
		/* The first element needs no reordering, but needs weight */
		vb->spectrum[2] = 1.0;
	}
	return 0;
}

int DifxVisCollectRandomParams(const VisBuffer *vb)
{
	DifxVis *dv = vb->dv;
	const DifxPolyModel *im1, *im2;
	int terms1, terms2, n;
	double U, V, W, dt;
	/*calculate UVW*/
	im1 = vb->D->scan[vb->scanId].im[dv->difxbl/256 - 1];
	im2 = vb->D->scan[vb->scanId].im[dv->difxbl%256 - 1];
	if(im1 != im2)
	{
		//FIXME shouldn't need to redo this
		n = getDifxScanIMIndex(vb->D->scan + vb->scanId, vb->mjdCentroid, &dt);
		if(n < 0)
		{
			fprintf(stderr, "Error: interferometer model index out of range: scanId=%d mjd=%12.6f %d-%d \n",
			vb->scanId, vb->mjdCentroid,
			dv->bl/256, dv->bl%256);
		}
		else
		{
			terms1 = im1->order + 1;
			terms2 = im2->order + 1;
			U = evalPoly(im2[n].u, terms2, dt) 
			   -evalPoly(im1[n].u, terms1, dt);
			V = evalPoly(im2[n].v, terms2, dt) 
			   -evalPoly(im1[n].v, terms1, dt);
			W = evalPoly(im2[n].w, terms2, dt) 
			   -evalPoly(im1[n].w, terms1, dt);
		}
	}
	else
	{
		U = 0.0;
		V = 0.0;
		W = 0.0;
	}
	dv->record->U		= U/cLight;
	dv->record->V		= V/cLight;
	dv->record->W		= W/cLight;

	dv->record->jd		= 2400000.5 + (int)vb->mjdCentroid;
	dv->record->iat		= vb->mjdCentroid - (int)vb->mjdCentroid;

	/* reminder: antennaIds, sourceId, freqId are 1-based in FITS */
	dv->record->baseline	= dv->bl;
	dv->record->filter	= 0;
	dv->record->sourceId1	= vb->D->source[vb->sourceId].fitsSourceId + 1;
	dv->record->freqId1	= vb->freqId + 1;
	dv->record->intTime	= vb->tIntOut;
#ifdef DEBUG
	printf("U=%e\n", dv->record->U);
	printf("V=%e\n", dv->record->V);
	printf("W=%e\n", dv->record->W);
	printf("jd=%f\n", dv->record->jd);
	printf("iat=%f\n", dv->record->iat);
	printf("baseline=%d\n", dv->record->baseline);
	printf("filter=%d\n", dv->record->filter);
	printf("sourceId1=%d\n", dv->record->sourceId1);
	printf("freqId1=%d\n", dv->record->freqId1);
	printf("intTime=%f\n", dv->record->intTime);
#endif
	return 0;
}

static int resetData(const VisBuffer *vb)
{
	int i, j, k, l;
	float ****d;

	d = vb->dv->data;

	for(i=0; i<vb->nBand; i++)
	{
		for(j=0; j<vb->nPol; j++)
		{
			for(k=0; k<vb->nOutChan; k++)
			{
				for(l=0; l<vb->nComplex; l++)
				{
					d[i][j][k][l] = 0.0;
				}
			}
			vb->dv->tEff[i][j] = 0.0;
			vb->dv->weightSum[i][j] = 0.0;
		}
	}
	vb->dv->n = 0;
	return 0;
}

static int RecordIsInvalid(const VisBuffer *vb)
{
	int i, j, k, l;
	float ****d;

	d = vb->dv->data;

	for(i=0; i<vb->nBand; i++)
	{
		for(j=0; j<vb->nPol; j++)
		{
			for(k=0; k<vb->nOutChan; k++)
			{
				for(l=0; l<vb->nComplex; l++)
				{
					if(isnan(d[i][j][k][l]) || isinf(d[i][j][k][l]))
					{

						printf("Warning -- droppin record with !finite value: ");
						printf("a1=%d a2=%d mjd=%13.7f band=%d pol=%d chan=%d cplx=%d\n",
							(vb->dv->record->baseline/256) - 1,
							(vb->dv->record->baseline%256) - 1,
							vb->mjdCentroid,
							i,j,k,l);
						return 1;
					}
					if(d[i][j][k][l] > 1.0e10 || d[i][j][k][l] < -1.0e10)
					{
						printf("Warning -- dropping record with extreme value: ");
						printf("a1=%d a2=%d mjd=%13.7f band=%d pol=%d chan=%d cplx=%d value=%e\n",
							(vb->dv->record->baseline/256) - 1,
							(vb->dv->record->baseline%256) - 1,
							vb->mjdCentroid,
							i,j,k,l,
							d[i][j][k][l]);
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

static int RecordIsZero(const VisBuffer *vb)
{
	int i, j, k, l;
	int invalid=1, weightinvalid = 1;
	float ****d;

	d = vb->dv->data;

	for(i=0; i<vb->nBand; i++)
	{
		for(j=0; j<vb->nPol; j++)
		{
			for(k=0; k<vb->nOutChan; k++)
			{
				for(l=0; l<vb->nComplex; l++)
				{
					/* don't look at weight in deciding whether data is valid */
					if((d[i][j][k][l] != 0.0) && (l != 2))
					{
						invalid = 0;
					}
				}
			}
		}
	}
	if(vb->nComplex == 2)
	{
		for(i=0; i<vb->nBand; i++)
		{
			for(j=0; j<vb->nPol; j++)
			{
				if(vb->dv->weightSum[i][j] != 0.0)
				{
					weightinvalid = 0;
				}
			}
		}
	}

	return invalid || weightinvalid;
}

static int RecordIsTransitioning(const VisBuffer *vb)
{
	return 0;
	//return vb->flagTransition;
}

static int RecordIsFlagged(const VisBuffer *vb)
{
	double mjd;
	int a1, a2;
	int i;

	DifxVis *dv;

	dv = vb->dv;
		
	if(vb->D->nFlag <= 0)
	{
		return 0;
	}

	a1  = (dv->record->baseline/256) - 1;
	a2  = (dv->record->baseline%256) - 1;
	mjd = vb->mjdCentroid;

	for(i = 0; i < vb->D->nFlag; i++)
	{
		if(vb->D->flag[i].mjd1 <= mjd &&
		   vb->D->flag[i].mjd2 >= mjd)
		{
			if(vb->D->flag[i].antennaId == a1 ||
			   vb->D->flag[i].antennaId == a2)
			{
				return 1;
			}
		}
	}

	return 0;
}


int shiftPhase(VisBuffer *vb)
{
	if(vb->shiftDelay == 0)
	{
		return(0);
	}
	/*shift phase for a single spectrum (subband)*/
	int i, index;
	float real, imag, preal, pimag; 
	float *d;
	double shift;

	d = vb->spectrum;
	
	for(i = 0; i < vb->D->nInChan; i++)
	{
		index = i*2;
		shift = TWO_PI * fmod(vb->freq[vb->bandId][i] * vb->shiftDelay, 1.0);/* us and MHz cancel*/
		preal = cos(shift); 
		pimag = sin(shift);
		real = d[index] * preal - d[index+1] * pimag;
		imag = d[index] * pimag + d[index+1] * preal;
		d[index] = real;
		d[index+1] = imag;
	}
	return(0);
}

static int readvisrecord(VisBuffer *oldvb, VisBuffer *vb)
{
	VisBuffer *oldervb;
	vb->changed = 0;
	while(vb->changed < NEW_TINT || 
	      vb->changed == SKIPPED_RECORD)
	{
		if(!vb->first && vb->changed >= 0)
		{
			storevis(vb);
#ifdef DEBUG
			printf("\t\t\t\tstored, ");
#endif
			//printf("vb      %f %d-%d %d %d\n", vb->header->mjd, vb->header->baseline_num/256, vb->header->baseline_num%256, vb->header->freq_index, vb->header->polId);
			//printf("oldvb   %f %d-%d %d %d\n", oldvb->header->mjd, oldvb->header->baseline_num/256, oldvb->header->baseline_num%256, oldvb->header->freq_index, oldvb->header->polId);
			moveVisBuffer(vb, oldvb);
#ifdef DEBUG
			//printf("swapping\n");
#endif
			oldervb = oldvb;
			oldvb = vb;
			vb = oldervb;
#ifdef DEBUG
			//printf("vb      %f %d-%d %d %d\n", vb->header->mjd, vb->header->baseline_num/256, vb->header->baseline_num%256, vb->header->freq_index, vb->header->polId);
			//printf("oldvb   %f %d-%d %d %d\n", oldvb->header->mjd, oldvb->header->baseline_num/256, oldvb->header->baseline_num%256, oldvb->header->freq_index, oldvb->header->polId);
#endif
		}
#ifdef DEBUG
		printf("reading, ");
#endif
		DifxVisNewUVData(oldvb, vb);
#ifdef DEBUG
		printf("\t\t\t\tchanged = %d, ", vb->changed);
#endif
	}
#ifdef DEBUG
	printf("breaking out\n");
#endif
	return vb->changed;
}

static int DifxVisConvert(const DifxInput *D, const DifxInput *CorrModel,
	struct fits_keywords *p_fits_keys, struct fitsPrivate *out, 
	double s, int verbose, int timeAvg, double sniffTime, int pulsarBin)
{
	int i, j, k, l, m;
	float visScale = 1.0;
	char fileBase[200];
	char dateStr[12];
	char fluxFormFloat[8];
	char gateFormInt[8];
	char weightFormFloat[8];
	int changed;
	int nRowBytes;
	int nColumn;
	int nWeight;
	int nData;
	int nFreq;
	int nJob;
	int polStart;
	int nComplex = 2;
	int nInvalid = 0;
	int nFlagged = 0;
	int nZero = 0;
	int nTrans = 0;
	int nWritten = 0;
	double scale;
	VisBuffer *vb, *oldvb;
	DifxVis* dv;
#ifdef HAVE_FFTW
	Sniffer *S = 0;
#endif

	/* define the columns in the UV data FITS Table */
	struct fitsBinTableColumn columns[] =
	{
		{"UU--SIN", "1E", "u", "SECONDS"},
		{"VV--SIN", "1E", "v", "SECONDS"},
		{"WW--SIN", "1E", "w", "SECONDS"},
		{"DATE", "1D", "Julian day at 0 hr current day", "DAYS"},
		{"TIME", "1D", "IAT time", "DAYS"},
		{"BASELINE", "1J", "baseline: ant1*256 + ant2", 0},
		{"FILTER", "1J", "filter id number", 0},
		{"SOURCE", "1J", "source id number from source tbl", 0},
		{"FREQID", "1J", "freq id number from frequency tbl", 0},
		{"INTTIM", "1E", "time span of datum", "SECONDS"},
		{"WEIGHT", weightFormFloat, "weights proportional to time", 0},
		{"GATEID", gateFormInt, "gate id from gate model table", 0},
		{"FLUX", fluxFormFloat, "data matrix", "UNCALIB"}
	};



	/* Start up sniffer */
	if(sniffTime > 0.0)
	{
		strcpy(fileBase, out->filename);
		l = strlen(fileBase);
		for(i = l-1; i > 0; i--)
		{
			if(fileBase[i] == '.')
			{
				fileBase[i] = 0;
				break;
			}
		}
#ifdef HAVE_FFTW
		S = newSniffer(D, nComplex, fileBase, sniffTime);
#endif
	}

	nWeight = D->nIF*D->nPolar;
	nData = 2*D->nFreq*D->nPolar*D->nOutChan;
	nFreq = D->nFreq;
	polStart = getPolStart(D);

	/* set the number of weight and flux values*/
	sprintf(weightFormFloat, "%dE", nWeight);
	sprintf(gateFormInt, "%dJ", 0);
	sprintf(fluxFormFloat, "%dE", nData);

	nColumn = NELEMENTS(columns);
	nRowBytes = FitsBinTableSize(columns, nColumn);

	fitsWriteBinTable(out, nColumn, columns, nRowBytes, "UV_DATA");
	fitsWriteInteger(out, "NMATRIX", 1, "");

	/* get the job ref_date from the fits_keyword struct, convert it into
	   a FITS string and save it in the FITS header */
	mjd2fits((int)D->mjdStart, dateStr);
	fitsWriteString(out, "DATE-OBS", dateStr, "");

	fitsWriteString(out, "TELESCOP", "VLBA", "");
	fitsWriteString(out, "OBSERVER", "PLUTO", "");
	
	arrayWriteKeys(p_fits_keys, out);
	
	fitsWriteInteger(out, "TABREV", 2, "ARRAY changed to FILTER");
	fitsWriteFloat(out, "VIS_SCAL", visScale, "");

	fitsWriteString(out, "SORT", "T*", "");

	/* define the data matrix columns */
	fitsWriteInteger(out, "MAXIS", 6, "");
	fitsWriteInteger(out, "MAXIS1", nComplex, "");

	fitsWriteString(out, "CTYPE1", "COMPLEX", "");
	fitsWriteFloat(out, "CDELT1", 1.0, "");
	fitsWriteFloat(out, "CRPIX1", 1.0, "");
	fitsWriteFloat(out, "CRVAL1", 1.0, "");
	fitsWriteInteger(out, "MAXIS2", D->nPolar, "");
	fitsWriteString(out, "CTYPE2", "STOKES", "");
	fitsWriteFloat(out, "CDELT2", -1.0, "");
	fitsWriteFloat(out, "CRPIX2", 1.0, "");
	fitsWriteFloat(out, "CRVAL2", (float)polStart, "");
	fitsWriteInteger(out, "MAXIS3", D->nOutChan, "");
	fitsWriteString(out, "CTYPE3", "FREQ", "");
	fitsWriteFloat(out, "CDELT3", 
		D->chanBW*D->specAvg*1.0e6/D->nInChan, "");
	fitsWriteFloat(out, "CRPIX3", p_fits_keys->ref_pixel, "");
	fitsWriteFloat(out, "CRVAL3", D->refFreq*1000000.0, "");
	fitsWriteInteger(out, "MAXIS4", nFreq, "");
	fitsWriteString(out, "CTYPE4", "BAND", "");
	fitsWriteFloat(out, "CDELT4", 1.0, "");
	fitsWriteFloat(out, "CRPIX4", 1.0, "");
	fitsWriteFloat(out, "CRVAL4", 1.0, "");
	fitsWriteInteger(out, "MAXIS5", 1, "");
	fitsWriteString(out, "CTYPE5", "RA", "");
	fitsWriteFloat(out, "CDELT5", 0.0, "");
	fitsWriteFloat(out, "CRPIX5", 1.0, "");
	fitsWriteFloat(out, "CRVAL5", 0.0, "");
	fitsWriteInteger(out, "MAXIS6", 1, "");
	fitsWriteString(out, "CTYPE6", "DEC", "");
	fitsWriteFloat(out, "CDELT6", 0.0, "");
	fitsWriteFloat(out, "CRPIX6", 1.0, "");
	fitsWriteFloat(out, "CRVAL6", 0.0, "");
	fitsWriteLogical(out, "TMATX11", 1, "");
	
	fitsWriteEnd(out);

	nJob = D->nJob;

	oldvb = newVisBuffer(D, CorrModel, nComplex, timeAvg, scale, verbose, pulsarBin);
	vb = newVisBuffer(D, CorrModel, nComplex, timeAvg, scale, verbose, pulsarBin);

	/*point vb at first job*/
	vb->jobId = updateJob(vb);
	startGlob(vb);
	
	while(1)
	{
		changed = readvisrecord(oldvb, vb);
#ifdef DEBUG
		printf("record read\n");
#endif

		for(i=0; i < vb->nBl; i++)
		{
			oldvb->dv = oldvb->dvs[i][0];
			dv = oldvb->dv;
			if(dv == 0)/*no data for this baseline*/
			{
#ifdef DEBUG
			printf("Warning -- empty baseline ");
			printf("blId %d mjd=%13.7f\n",
				i, vb->mjdCentroid);
#endif
				continue;
			}

			DifxVisCollectRandomParams(oldvb);
			if(RecordIsInvalid(oldvb))
			{
				nInvalid++;
			}
			else if(RecordIsFlagged(oldvb))
			{
				nFlagged++;
			}
			else if(RecordIsZero(oldvb))
			{
				nZero++;
			}
			else if(RecordIsTransitioning(oldvb))
			{
				nTrans++;
			}
			else
			{
				/*do weight averaging*/
				for(j=0; j<oldvb->nBand; j++)
				{
					for(k=0; k<oldvb->nPol; k++)
					{
						for(l=0; l<oldvb->nOutChan; l++)
						{
							for(m=0; m<oldvb->nComplex; m++)
							{
								/*this gives results consistent with old difx2fits*/
								dv->data[j][k][l][m] /= 1;
									//dv->weightSum[j][k]; 
							}
						}
						dv->weightSum[j][k] = dv->tEff[j][k]/oldvb->tIntOut;
					}
				}
#ifdef HAVE_FFTW
				feedSnifferFITS(S, dv->record);
#endif
#ifndef WORDS_BIGENDIAN
				FitsBinRowByteSwap(columns, nColumn, 
					dv->record);
#endif
				fitsWriteBinRow(out, (char *)dv->record);
				nWritten++;
				printf("record written\n");
			}
			resetData(oldvb);
		}
		oldvb->nBuffer = 0;
		if(changed >= NEW_JOB)
		{
			nJob--;
		}
		if(changed >= JOBS_DONE)
		{
			break;
		}
	}

	printf("      %d invalid records dropped\n", nInvalid);
	printf("      %d flagged records dropped\n", nFlagged);
	printf("      %d all zero records dropped\n", nZero);
	printf("      %d scan boundary records dropped\n", nTrans);
	printf("      %d records written\n", nWritten);
	if(verbose > 1)
	{
		printf("        Note : A record is all data from 1 baseline\n");
		printf("        for 1 output time integration\n");
	}

#ifdef HAVE_FFTW
	deleteSniffer(S);
#endif

	return 0;
}

const DifxInput *DifxInput2FitsUV(const DifxInput *D, const DifxInput *CorrModel,
	struct fits_keywords *p_fits_keys,
	struct fitsPrivate *out, double scale, int timeAvg,
	int verbose, double sniffTime, int pulsarBin)
{
	if(D == 0)
	{
		return 0;
	}

	DifxVisConvert(D, CorrModel, p_fits_keys, out, scale, timeAvg, verbose, sniffTime, pulsarBin);

	return D;
}
