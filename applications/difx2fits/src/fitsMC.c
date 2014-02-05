/***************************************************************************
 *   Copyright (C) 2008-2012, 2014 by Walter Brisken & Adam Deller               *
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
#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#include "config.h"
#include "difx2fits.h"

const DifxInput *DifxInput2FitsMC(const DifxInput *D,
	struct fits_keywords *p_fits_keys, struct fitsPrivate *out, int phaseCentre)
{
	char bandFormFloat[8];

	struct fitsBinTableColumn columns[] =
	{
		{"TIME", "1D", "Time of center of interval", "DAYS"},
		{"SOURCE_ID", "1J", "source id from sources tbl", 0},
		{"ANTENNA_NO", "1J", "antenna id from antennas tbl", 0},
		{"ARRAY", "1J", "array id number", 0},
		{"FREQID", "1J", "freq id from frequency tbl", 0},
		{"ATMOS", "1D", "atmospheric group delay", "SECONDS"},
		{"DATMOS", "1D", "atmospheric group delay rate", "SEC/SEC"},
		{"GDELAY", "1D", "CALC geometric delay", "SECONDS"},
		{"GRATE", "1D", "CALC geometric delay rate", "SEC/SEC"},
		{"CLOCK_1", "1D", "electronic delay", "SECONDS"},
		{"DCLOCK_1", "1D", "electronic delay rate", "SEC/SEC"},
		{"LO_OFFSET_1", bandFormFloat, "station lo_offset for polar. 1", "HZ"},
		{"DLO_OFFSET_1", bandFormFloat, "station lo_offset rate for polar. 1", "HZ/SEC"},
		{"DISP_1", "1E", "dispersive delay", "SECONDS"},
		{"DDISP_1", "1E", "dispersive delay rate", "SEC/SEC"},
		{"CLOCK_2", "1D", "electronic delay", "SECONDS"},
		{"DCLOCK_2", "1D", "electronic delay rate", "SEC/SEC"},
		{"LO_OFFSET_2", bandFormFloat, "station lo_offset for polar. 2", "HZ"},
		{"DLO_OFFSET_2", bandFormFloat, "station lo_offset rate for polar. 2", "HZ/SEC"},
		{"DISP_2", "1E", "dispersive delay for polar 2", "SECONDS"},
		{"DDISP_2", "1E", "dispersive delay rate for polar 2", "SEC/SEC"}
	};
	struct fitsBinTableColumn columns_with_spacecraft[] =
	{
		{"TIME", "1D", "Time of center of interval", "DAYS"},
		{"SOURCE_ID", "1J", "source id from sources tbl", 0},
		{"ANTENNA_NO", "1J", "antenna id from antennas tbl", 0},
		{"ARRAY", "1J", "array id number", 0},
		{"FREQID", "1J", "freq id from frequency tbl", 0},
		{"ATMOS", "1D", "atmospheric group delay", "SECONDS"},
		{"DATMOS", "1D", "atmospheric group delay rate", "SEC/SEC"},
		{"GDELAY", "1D", "CALC geometric delay", "SECONDS"},
		{"GRATE", "1D", "CALC geometric delay rate", "SEC/SEC"},
		{"SCGSDELAY", "1D", "spacecraft to ground station delay", "SECONDS"},
		{"DSCGSDELAY", "1D", "spacecraft to ground station delay rate", "SEC/SEC"},
		{"GSCLOCK", "1D", "ground station electronic delay", "SECONDS"},
		{"DGSCLOCK", "1D", "ground station electronic delay rate", "SEC/SEC"},
		{"MSANGLE", "1D", "mount source angle", "DEGREES"},
		{"DMSANGLE", "1D", "mount source angle rate", "DEG/SEC"},
		{"CLOCK_1", "1D", "electronic delay", "SECONDS"},
		{"DCLOCK_1", "1D", "electronic delay rate", "SEC/SEC"},
		{"LO_OFFSET_1", bandFormFloat, "station lo_offset for polar. 1", "HZ"},
		{"DLO_OFFSET_1", bandFormFloat, "station lo_offset rate for polar. 1", "HZ/SEC"},
		{"DISP_1", "1E", "dispersive delay", "SECONDS"},
		{"DDISP_1", "1E", "dispersive delay rate", "SEC/SEC"},
		{"CLOCK_2", "1D", "electronic delay", "SECONDS"},
		{"DCLOCK_2", "1D", "electronic delay rate", "SEC/SEC"},
		{"LO_OFFSET_2", bandFormFloat, "station lo_offset for polar. 2", "HZ"},
		{"DLO_OFFSET_2", bandFormFloat, "station lo_offset rate for polar. 2", "HZ/SEC"},
		{"DISP_2", "1E", "dispersive delay for polar 2", "SECONDS"},
		{"DDISP_2", "1E", "dispersive delay rate for polar 2", "SEC/SEC"}
	};

	int nColumn;
 	int nRowBytes;
	char *p_fitsbuf, *fitsbuf;
	int nBand, nPol;
	int b, j, s, p, np, a;
        int found_MC_interval, MC_interval;
	float LOOffset[array_MAX_BANDS];
	float LORate[array_MAX_BANDS];
	float dispDelay;
	float dispDelayRate;
	const DifxConfig *config;
	const DifxScan *scan;
	const DifxPolyModel *P;
	double time, deltat, deltat2, deltatn;
        double x, x_j, x_j_m1;
	double delay, delayRate;
	double atmosDelay, atmosRate;
	double clock, clockRate, c1, c2;
        double sc_gs_delay, sc_gs_rate;
        double gs_clock_delay, gs_clock_rate;
        double msa, msa_rate;
	int configId, dsId, antId;
	int *skip;
	int skipped=0;
	int printed=0;
        int has_no_spacecraft=1;
	/* 1-based indices for FITS file */
	int32_t antId1, arrayId1, sourceId1, freqId1;

	if(D == 0)
	{
		return 0;
	}

        /* Find out whether or not there are spacecraft antennas. */
        /* Because of the way that AIPS loads in FITS files, if */
        /* any job of an experiment has a spacecraft antenna, then */
        /* all jobs of the experiment must write out information */
        /* for the spacecraft antenna mode. */
	for(s = 0; s < D->nSpacecraft; s++)
	{
            if((D->spacecraft[s].is_antenna))
            {
                has_no_spacecraft = 0;
                break;
            }
        }

	nBand = p_fits_keys->no_band;
	sprintf (bandFormFloat, "%1dE", nBand);  
  
	nPol = D->nPol;
	if(nPol == 2)
	{
            if((has_no_spacecraft))
            {
		nColumn = NELEMENTS(columns);
            }
            else
            {
                nColumn = NELEMENTS(columns_with_spacecraft);
            }
	}
	else	/* don't populate last 6 columns if not full polar */
	{
            if((has_no_spacecraft))
            {
		nColumn = NELEMENTS(columns) - 6;
            }
            else
            {
                nColumn = NELEMENTS(columns_with_spacecraft) - 6;
            }
	}
        if((has_no_spacecraft))
        {
            nRowBytes = FitsBinTableSize(columns, nColumn);
        }
        else
        {
            nRowBytes = FitsBinTableSize(columns_with_spacecraft, nColumn);
        }

	/* calloc space for storing table in FITS order */
	fitsbuf = (char *)calloc(nRowBytes, 1);
	if(fitsbuf == 0)
	{
		return 0;
	}
  
	if((has_no_spacecraft))
        {
            fitsWriteBinTable(out, nColumn, columns, nRowBytes, "MODEL_COMPS");
        }
        else
        {
            fitsWriteBinTable(out, nColumn, columns_with_spacecraft, nRowBytes, "MODEL_COMPS");
        }
	arrayWriteKeys(p_fits_keys, out);
	fitsWriteInteger(out, "NO_POL", nPol, "");
	fitsWriteInteger(out, "FFT_SIZE", D->nInChan*2, "");
	fitsWriteInteger(out, "OVERSAMP", 0, "");
	fitsWriteInteger(out, "ZERO_PAD", 0, "");
	fitsWriteInteger(out, "FFT_TWID", 1, "Version of FFT twiddle table used");
	fitsWriteString(out, "TAPER_FN", D->job->taperFunction, "");
	fitsWriteInteger(out, "TABREV", 1, "");
#warning "populate the new keyword below"
	//fitsWriteDouble(out, "DELTAT", , "DAYS");
	
	fitsWriteEnd(out);

	arrayId1 = 1;

	/* some values that are always zero */
	for(b = 0; b < nBand; ++b)
	{
		LOOffset[b] = 0.0;
		LORate[b] = 0.0;
	}

	dispDelay = 0.0;
	dispDelayRate = 0.0;

	skip = (int *)calloc(D->nAntenna, sizeof(int));

	for(s = 0; s < D->nScan; ++s)
	{
                scan = D->scan + s;
                configId = scan->configId;
                if(configId < 0)
                {
                        continue;
                }
                if(phaseCentre >= scan->nPhaseCentres)
                {
                        continue;
                }
                config = D->config + configId;
                freqId1 = config->fitsFreqId + 1;
                sourceId1 = D->source[scan->phsCentreSrcs[phaseCentre]].fitsSourceIds[configId] + 1;

                if(scan->im)
                {
                        np = scan->nPoly;
                }
                else
                {
                        fprintf(stderr, "No IM table available for scan %d; aborting MC file creation\n", s);
                        continue;
                }

                for(p = 0; p < np; ++p)
                {
                        found_MC_interval = 1;
                        MC_interval = -1;
                        while((found_MC_interval))
                        {
                                MC_interval++;
                                /* set loop condition to false --- it will be set true as
                                   needed later on */
                                found_MC_interval = 0;
                                x = MC_interval * config->MC_table_output_interval;
                                /* loop over original .input file antenna list */
                                for(a = 0; a < config->nAntenna; ++a)
                                {
                                        dsId = config->ant2dsId[a];
                                        if(dsId < 0 || dsId >= D->nDatastream)
                                        {
                                                continue;
                                        }
                                        /* convert to D->antenna[] index ... */
                                        antId = D->datastream[dsId].antennaId;
                                        
                                        if(antId < 0 || antId >= scan->nAntenna)
                                        {
                                                continue;
                                        }
                                        
                                        /* ... and to FITS antennaId */
                                        antId1 = antId + 1;
                                        
                                        if(scan->im[antId] == 0)
                                        {
                                                if(skip[antId] == 0)
                                                {
                                                        printf("\n    Polynomial model error : skipping antId %d = %s", 
                                                               antId, D->antenna[antId].name);
                                                        ++skip[antId];
                                                        ++printed;
                                                        ++skipped;
                                                }
                                                continue;
                                        }

                                        P = scan->im[antId][phaseCentre] + p;
                                        if((x >= P->validDuration) && (MC_interval > 0))
                                        {
                                                continue;
                                        }
                                        else {
                                                found_MC_interval = 1;
                                        }
                                        
                                        time = P->mjd - (int)(D->mjdStart) + (P->sec +x)/86400.0;
                                        deltat = (P->mjd - D->antenna[antId].clockrefmjd)*86400.0 + P->sec +x;
                                        
                                        /* evaluate the model polynomials and their first
                                           derivatives for time offset x */
                                        x_j = 1.0;
                                        x_j_m1 = 0.0;
                                        atmosDelay = atmosRate = delay = delayRate = 0.0;
                                        sc_gs_delay = sc_gs_rate = gs_clock_delay = gs_clock_rate = 0.0;
                                        msa = msa_rate = 0.0;
                                        for(j=0; j <= P->order; j++)
                                        {
                                                atmosDelay += (P->dry[j] + P->wet[j]) * x_j;
                                                atmosRate  += (P->dry[j] + P->wet[j]) * (j * x_j_m1);
                                                /* here correct the sign of delay,
                                                   and remove atmospheric portion of it. */
                                                delay      += (-P->delay[j] - (P->dry[j] + P->wet[j])) * x_j;
                                                delayRate  += (-P->delay[j] - (P->dry[j] + P->wet[j])) * (j * x_j_m1);
                                                if(!has_no_spacecraft)
                                                {
                                                        sc_gs_delay += P->sc_gs_delay[j] * x_j;
                                                        sc_gs_rate  += P->sc_gs_delay[j] * (j * x_j_m1);
                                                        gs_clock_delay += P->gs_clock_delay[j] * x_j;
                                                        gs_clock_rate  += P->gs_clock_delay[j] * (j * x_j_m1);
                                                        msa      += P->msa[j] * x_j;
                                                        msa_rate += P->msa[j] * (j * x_j_m1);
                                                }
                                                /* update x_j values for the next order */
                                                x_j_m1 = x_j;
                                                x_j *= x;
                                        }

                                        /* evaluate the clock polynomials and their first
                                           derivatives for time offset x */
                                        x_j = 1.0;
                                        x_j_m1 = 0.0;
                                        clock = clockRate = 0.0;
                                        for(j = 0; j <= D->antenna[antId].clockorder; j++)
                                        {
                                                clock     += D->antenna[antId].clockcoeff[j] * x_j;
                                                clockRate += D->antenna[antId].clockcoeff[j] * (j * x_j_m1);
                                                /* update x_j values for the next order */
                                                x_j_m1 = x_j;
                                                x_j *= deltat;
                                        }
                                        
                                        /* in general, convert from (us) to (sec) */
                                        atmosDelay     *= 1.0e-6;
                                        atmosRate      *= 1.0e-6;
                                        delay          *= 1.0e-6;
                                        delayRate      *= 1.0e-6;
                                        sc_gs_delay    *= 1.0E-6;
                                        sc_gs_rate     *= 1.0E-6;
                                        gs_clock_delay *= 1.0E-6;
                                        gs_clock_rate  *= 1.0E-6;
                                        clock          *= 1.0E-6;
                                        clockRate      *= 1.0E-6;
                                        /* convert from radians to degrees */
                                        msa            *= 180.0/M_PI;
                                        msa_rate       *= 180.0/M_PI;
                                        
                                        /* get MSA in range -180 to +180 degrees */
                                        msa = fmod(msa,360.0);
                                        if(msa > 180.0)
                                        {
                                                msa -= 360.0;
                                        }
                                        else if(msa <= -180.0)
                                        {
                                                msa += 360.0;
                                        }
                                        if(!has_no_spacecraft)
                                        {
                                                if(!config->doMSAcalibration)
                                                {
                                                        msa=0.0;
                                                        msa_rate=0.0;
                                                }
                                        }
                                        
                                        p_fitsbuf = fitsbuf;

                                        FITS_WRITE_ITEM (time, p_fitsbuf);
                                        FITS_WRITE_ITEM (sourceId1, p_fitsbuf);
                                        FITS_WRITE_ITEM (antId1, p_fitsbuf);
                                        FITS_WRITE_ITEM (arrayId1, p_fitsbuf);
                                        FITS_WRITE_ITEM (freqId1, p_fitsbuf);
                                        FITS_WRITE_ITEM (atmosDelay, p_fitsbuf);
                                        FITS_WRITE_ITEM (atmosRate, p_fitsbuf);
                                        FITS_WRITE_ITEM (delay, p_fitsbuf);
                                        FITS_WRITE_ITEM (delayRate, p_fitsbuf);
                                        if(!has_no_spacecraft)
                                        {
                                                FITS_WRITE_ITEM (sc_gs_delay, p_fitsbuf);
                                                FITS_WRITE_ITEM (sc_gs_rate, p_fitsbuf);
                                                FITS_WRITE_ITEM (gs_clock_delay, p_fitsbuf);
                                                FITS_WRITE_ITEM (gs_clock_rate, p_fitsbuf);
                                                FITS_WRITE_ITEM (msa, p_fitsbuf);
                                                FITS_WRITE_ITEM (msa_rate, p_fitsbuf);
                                        }
                                        
                                        for(j = 0; j < nPol; ++j)
                                        {
                                                FITS_WRITE_ITEM (clock, p_fitsbuf);
                                                FITS_WRITE_ITEM (clockRate, p_fitsbuf);
                                                FITS_WRITE_ARRAY(LOOffset, p_fitsbuf, nBand);
                                                FITS_WRITE_ARRAY(LORate, p_fitsbuf, nBand);
                                                FITS_WRITE_ITEM (dispDelay, p_fitsbuf);
                                                FITS_WRITE_ITEM (dispDelayRate, p_fitsbuf);
                                        } /* Polar loop */

                                        testFitsBufBytes(p_fitsbuf - fitsbuf, nRowBytes, "MC");
                                        
#ifndef WORDS_BIGENDIAN
                                        if((has_no_spacecraft))
                                        {
                                                FitsBinRowByteSwap(columns, nColumn, fitsbuf);
                                        }
                                        else
                                        {
                                                FitsBinRowByteSwap(columns_with_spacecraft, nColumn, fitsbuf);
                                        }
#endif
                                        fitsWriteBinRow(out, fitsbuf);
                                } /* Antenna loop */
                                if(config->MC_table_output_interval == 0.0)
                                {
                                        found_MC_interval = 0;
                                }
                        } /* found_MC_interval loop */
                } /* Intervals in scan loop */
	} /* Scan loop */
 
	if(printed)
	{
		printf("\n                            ");
	}
  
	/* release buffer space */
	free(fitsbuf);
	free(skip);

	return D;
}
