/***************************************************************************
 *	 Copyright (C) 2008-2015 by Walter Brisken							   *
 *				   2012-2015 by James M Anderson						  *
 *																		   *
 *	 This program is free software; you can redistribute it and/or modify  *
 *	 it under the terms of the GNU General Public License as published by  *
 *	 the Free Software Foundation; either version 3 of the License, or	   *
 *	 (at your option) any later version.								   *
 *																		   *
 *	 This program is distributed in the hope that it will be useful,	   *
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of		   *
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		   *
 *	 GNU General Public License for more details.						   *
 *																		   *
 *	 You should have received a copy of the GNU General Public License	   *
 *	 along with this program; if not, write to the						   *
 *	 Free Software Foundation, Inc.,									   *
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.			   *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "config.h"
#include "difxio/difx_write.h"

/* include spice files for spacecraft navigation if libraries are present */
#if HAVE_SPICE
#include "SpiceCK.h"
#include "SpiceZpr.h"
#include "SpiceZfc.h"
#endif
#include "hermite.h"


static const double SECONDS_PER_DAY = SEC_DAY_DBL;


const char spacecraftTimeTypeNames[][MAX_SPACECRAFT_TIME_NAME_LENGTH] =
	{
		"Local",
		"GroundReception",
		"GroundClock",
		"GroundClockReception",
		"OTHER"
	};

enum SpacecraftTimeType stringToSpacecraftTimeType(const char *str)
{
	if(strcasecmp(str, "Local") == 0)
	{
		return SpacecraftTimeLocal;
	}
	if(strcasecmp(str, "GroundReception") == 0)
	{
		return SpacecraftTimeGroundReception;
	}
	if(strcasecmp(str, "GroundClock") == 0)
	{
		return SpacecraftTimeGroundClock;
	}
	if(strcasecmp(str, "GroundClockReception") == 0)
	{
		return SpacecraftTimeGroundClockReception;
	}
	return SpacecraftTimeOther;
}





DifxSpacecraft *newDifxSpacecraftArray(int nSpacecraft)
{
	DifxSpacecraft *ds;
	int s;
	
	if(nSpacecraft == 0)
	{
		return 0;
	}
	
	ds = (DifxSpacecraft *)calloc(nSpacecraft, sizeof(DifxSpacecraft));

	for(s = 0; s < nSpacecraft; ++s)
	{
		ds[s].spacecraft_time_type = SpacecraftTimeOther;
		ds[s].position_coord_frame = SourceCoordinateFrameUnknown;
		ds[s].pointing_coord_frame = SourceCoordinateFrameUnknown;
		ds[s].GS_recording_delay = NAN;
		ds[s].GS_transmission_delay = NAN;
		ds[s].GS_transmission_delay_sync = NAN;
		ds[s].GS_mount = AntennaMountOther;
		ds[s].GS_clockorder = -1;
		ds[s].SC_pos_offsetorder = -1;
	}

		
	return ds;
}

DifxSpacecraft *dupDifxSpacecraftArray(const DifxSpacecraft *src, int n)
{
	DifxSpacecraft *dest;
	int s;

	dest = newDifxSpacecraftArray(n);

	for(s = 0; s < n; ++s)
	{
		/* Take care of the regular member variables */
		dest[s] = src[s];
		/* Now deal with the pointers */
		dest[s].pos = (sixVector *)malloc(dest[s].nPoint* sizeof(sixVector));
		memcpy(dest[s].pos, src[s].pos, dest[s].nPoint*sizeof(sixVector));
		if(src[s].TFrameOffset)
		{
			dest[s].TFrameOffset = (spacecraftTimeFrameOffset *)malloc(dest[s].nPoint* sizeof(spacecraftTimeFrameOffset));
			memcpy(dest[s].TFrameOffset, src[s].TFrameOffset, dest[s].nPoint*sizeof(spacecraftTimeFrameOffset));
		}
		if(src[s].SCAxisVectors)
		{
			dest[s].SCAxisVectors = (spacecraftAxisVectors *)malloc(dest[s].nPoint* sizeof(spacecraftAxisVectors));
			memcpy(dest[s].SCAxisVectors, src[s].SCAxisVectors,  dest[s].nPoint*sizeof(spacecraftAxisVectors));
		}
	}

	return dest;
}

void deleteDifxSpacecraftInternals(DifxSpacecraft *ds)
{
	free(ds->pos); ds->pos = 0;
	free(ds->TFrameOffset); ds->TFrameOffset = 0;
	free(ds->SCAxisVectors); ds->SCAxisVectors = 0;
	return;
}

void deleteDifxSpacecraftArray(DifxSpacecraft *ds, int nSpacecraft)
{
	int s;

	if(ds)
	{
		for(s = 0; s < nSpacecraft; ++s)
		{
			deleteDifxSpacecraftInternals(ds + s);
		}
		free(ds);
	}
}

void fprintDifxSpacecraft(FILE *fp, const DifxSpacecraft *ds)
{
	int i;
	fprintf(fp, "  DifxSpacecraft : %p\n", ds);
	if(!ds)
	{
		return;
	}
	fprintf(fp, "	 Name = %s\n", ds->name);
	fprintf(fp, "	 Is Antenna = %c\n", (ds->is_antenna) ? 'T' : 'F');
	fprintf(fp, "	 Num points = %d\n", ds->nPoint);
	fprintf(fp, "	 Has ground station = %c\n", (ds->GS_exists) ? 'T':'F');
	fprintf(fp, "	 spacecraft_time_type = %s\n", spacecraftTimeTypeNames[ds->spacecraft_time_type]);
	fprintf(fp, "	 position_coord_frame = %s\n", sourceCoordinateFrameTypeNames[ds->position_coord_frame]);
	fprintf(fp, "	 pointing_coord_frame = %s\n", sourceCoordinateFrameTypeNames[ds->pointing_coord_frame]);
	fprintf(fp, "	 SC_recording_delay			= %.16E\n", ds->SC_recording_delay);
	fprintf(fp, "	 SC_Comm_Rec_to_Elec		= %.16E\n", ds->SC_Comm_Rec_to_Elec);
	fprintf(fp, "	 SC_Elec_to_Comm			= %.16E\n", ds->SC_Elec_to_Comm);
	fprintf(fp, "	 GS_recording_delay			= %.16E\n", ds->GS_recording_delay);
	fprintf(fp, "	 GS_transmission_delay		= %.16E\n", ds->GS_transmission_delay);
	fprintf(fp, "	 GS_transmission_delay_sync = %.16E\n", ds->GS_transmission_delay_sync);
	fprintf(fp, "	 SC_elec_delay				= %.16E\n", ds->SC_elec_delay);
	fprintf(fp, "	 GS_clock_delay				= %.16E\n", ds->GS_clock_delay);
	fprintf(fp, "	 GS_clock_delay_sync		= %.16E\n", ds->GS_clock_delay_sync);
	if(ds->GS_exists) {
		fprintf(fp, "	 GS_Name = %s\n", ds->GS_Name);
		fprintf(fp, "	 GS_calcName = %s\n", ds->GS_calcName);
		fprintf(fp, "	 GS_mjd_sync		   = %d\n", ds->GS_mjd_sync);
		fprintf(fp, "	 GS_dayfraction_sync   = %.16f\n", ds->GS_dayfraction_sync);
		fprintf(fp, "	 GS_clock_break_fudge_sec	= %.16E\n", ds->GS_clock_break_fudge_sec);
		fprintf(fp, "	 GS_mount = %s\n", antennaMountTypeNames[ds->GS_mount]);
		fprintf(fp, "	 GS_offfset[0] = %.6f\n", ds->GS_offset[0]);
		fprintf(fp, "	 GS_offfset[1] = %.6f\n", ds->GS_offset[1]);
		fprintf(fp, "	 GS_offfset[2] = %.6f\n", ds->GS_offset[2]);
		fprintf(fp, "	 GS_X = %.5f\n", ds->GS_X);
		fprintf(fp, "	 GS_Y = %.5f\n", ds->GS_Y);
		fprintf(fp, "	 GS_Z = %.5f\n", ds->GS_Z);
		fprintf(fp, "	 GS_clockrefmjd = %.16f\n", ds->GS_clockrefmjd);
		fprintf(fp, "	 GS_clockorder = %d\n", ds->GS_clockorder);
		for(i=0; i < ds->GS_clockorder+1; ++i) {
			fprintf(fp, "	 GS_clockcoeff[%d] = %24.16E\n", i, ds->GS_clockcoeff[i]);
		}
	}
	fprintf(fp, "	 SC_pos_offset_refmjd = %d\n", ds->SC_pos_offset_refmjd);
	fprintf(fp, "	 SC_pos_offset_reffracDay = %.16f\n", ds->SC_pos_offset_reffracDay);
	fprintf(fp, "	 SC_pos_offsetorder = %d\n", ds->SC_pos_offsetorder);
	fprintf(fp, "	 CalcOwnRetardation = %d\n", ds->calculate_own_retarded_position);
}

void printDifxSpacecraft(const DifxSpacecraft *ds)
{
	fprintDifxSpacecraft(stdout, ds);
}

int computeDifxSpacecraftTimeFrameOffset(DifxSpacecraft *ds, const char* JPLplanetaryephem)
{
	int nPoint = ds->nPoint;
	int mjd_sync = ds->GS_mjd_sync;
	double dayfraction_sync = ds->GS_dayfraction_sync;
	int START_AFTER_TIME_INDEX;
	int i;
	int last_block_mjd;
	double last_block_dayfraction;
	double last_blockpoint_delay;
	const double block_time_length = 0.05; /* in days.	The delay modeling is
											  done in blocks that are hopefully
											  not larger than a single orbital
											  time, so that the underlying
											  numerical integration functions
											  can get a
											  reasonable approximation
											  to the orbit, with points not
											  jumping back and forth in
											  a "random" manner. */
	const double max_fractional_error = 1E-11; /* Keep error below 1E-15 s
												  over 24 hours of orbit */
	const double max_absolute_error = 1E-15;   /* Keep absolute undertainty
												  below 1E-15 s */
	int return_code;

	/*fprintf(stderr, "INFO: computeDifxSpacecraftTimeFrameOffset starting for spacecraft %s.\n", ds->name);*/

	if(JPLplanetaryephem[0] != 0) {
		fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: DiFX software for JPL planetary ephemeris effects on spacecraft time frame not yet written.  Effects including the gravitational potential of the Moon and Sun will not be incorporated.\n");
	}

	free(ds->TFrameOffset);
	ds->TFrameOffset = (spacecraftTimeFrameOffset *)calloc(nPoint, sizeof(spacecraftTimeFrameOffset));

	/* verify sane sync time */
	if(((mjd_sync+dayfraction_sync) < (ds->pos[0].mjd+ds->pos[0].fracDay)-0.01)
	  || ((mjd_sync+dayfraction_sync) > (ds->pos[nPoint-1].mjd+ds->pos[nPoint-1].fracDay)+0.01)) {
		fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: spacecraft sync time is outside of data window.  Forcing new sync time.\n");
		fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: Input sync time %d %f.\n", mjd_sync, dayfraction_sync);
		fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: Start time %d %f.\n", ds->pos[0].mjd, ds->pos[0].fracDay);
		fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: Stop time %d %f.\n", ds->pos[nPoint-1].mjd, ds->pos[nPoint-1].fracDay);
		mjd_sync = ds->pos[0].mjd;
		dayfraction_sync = ds->pos[0].fracDay;
	}
	START_AFTER_TIME_INDEX = nPoint;
	for(i=0; i < nPoint; ++i) {
		if((mjd_sync+dayfraction_sync) <= (ds->pos[i].mjd+ds->pos[i].fracDay)) {
			START_AFTER_TIME_INDEX = i;
			break;
		}
	}
	/* If the total time block over which this routine is calculating the
	   relative frame time offsets is substantially longer than a signle orbital
	   period, then the numerical integration routines can get messed up.
	   So break the full integration from the sync time to the table row time
	   into subblocks as necessary. */
	
	/* do the times below the sync time */
	last_block_mjd = mjd_sync;
	last_block_dayfraction = dayfraction_sync;
	last_blockpoint_delay = 0.0;
	for(i=START_AFTER_TIME_INDEX-1; i >= 0; --i) {
		double subblock_delay;
		double subblock_delay_uncert;
		double rate;
		int new_mjd = ds->pos[i].mjd;
		double new_dayfraction = ds->pos[i].fracDay;
		double offset;
		do {
			offset = (last_block_mjd-new_mjd)
					 + (last_block_dayfraction-new_dayfraction);
			if(offset >= block_time_length) {
				int mjd_offset;
				int new_block_mjd;
				double new_block_dayfraction = last_block_dayfraction - block_time_length;
				mjd_offset = (int)(floor(new_block_dayfraction));
				new_block_mjd = last_block_mjd + mjd_offset;
				new_block_dayfraction -= mjd_offset;
				return_code =
					DiFX_model_scpacecraft_time_delay_qromb(ds,
															last_block_mjd,
															last_block_dayfraction,
															new_block_mjd,
															new_block_dayfraction,
															max_fractional_error,
															max_absolute_error,
														   &subblock_delay,
														   &subblock_delay_uncert,
														   &rate);
				if(return_code < 0) {
					fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: Failure %d from DiFX_model_scpacecraft_time_delay_qromb.\n", return_code);
					return -1;
				}
				last_blockpoint_delay += subblock_delay;
				last_block_mjd = new_block_mjd;
				last_block_dayfraction = new_block_dayfraction;
			}
		} while(offset >= block_time_length);
		return_code =
			DiFX_model_scpacecraft_time_delay_qromb(ds,
													last_block_mjd,
													last_block_dayfraction,
													new_mjd,
													new_dayfraction,
													max_fractional_error,
													max_absolute_error,
												   &subblock_delay,
												   &subblock_delay_uncert,
												   &rate);
		ds->TFrameOffset[i].Delta_t = last_blockpoint_delay + subblock_delay;
		ds->TFrameOffset[i].dtdtau = rate;
	}
	/* do the times above the sync time */
	last_block_mjd = mjd_sync;
	last_block_dayfraction = dayfraction_sync;
	last_blockpoint_delay = 0.0;
	for(i=START_AFTER_TIME_INDEX; i < nPoint; i++) {
		double subblock_delay;
		double subblock_delay_uncert;
		double rate;
		int new_mjd = ds->pos[i].mjd;
		double new_dayfraction = ds->pos[i].fracDay;
		double offset;
		do {
			offset = (new_mjd-last_block_mjd)
					 + (new_dayfraction-last_block_dayfraction);
			if(offset >= block_time_length) {
				int mjd_offset;
				int new_block_mjd;
				double new_block_dayfraction = last_block_dayfraction + block_time_length;
				mjd_offset = (int)(floor(new_block_dayfraction));
				new_block_mjd = last_block_mjd + mjd_offset;
				new_block_dayfraction -= mjd_offset;
				return_code =
					DiFX_model_scpacecraft_time_delay_qromb(ds,
															last_block_mjd,
															last_block_dayfraction,
															new_block_mjd,
															new_block_dayfraction,
															max_fractional_error,
															max_absolute_error,
														   &subblock_delay,
														   &subblock_delay_uncert,
														   &rate);
				if(return_code < 0) {
					fprintf(stderr, "Error: computeDifxSpacecraftTimeFrameOffset: Failure %d from DiFX_model_scpacecraft_time_delay_qromb.\n", return_code);
					return -1;
				}
				last_blockpoint_delay += subblock_delay;
				last_block_mjd = new_block_mjd;
				last_block_dayfraction = new_block_dayfraction;
			}
		} while(offset >= block_time_length);
		return_code =
			DiFX_model_scpacecraft_time_delay_qromb(ds,
													last_block_mjd,
													last_block_dayfraction,
													new_mjd,
													new_dayfraction,
													max_fractional_error,
													max_absolute_error,
												   &subblock_delay,
												   &subblock_delay_uncert,
												   &rate);
		ds->TFrameOffset[i].Delta_t = last_blockpoint_delay + subblock_delay;
		ds->TFrameOffset[i].dtdtau = rate;
	}
	/*fprintf(stderr, "INFO: computeDifxSpacecraftTimeFrameOffset done.\n");*/
	return return_code;
}


#if HAVE_SPICE
static void ECI2J2000(doublereal et, doublereal state[6])
{
	doublereal precm[36];
	doublereal invprecm[36];
	doublereal tmpstate[6];
	int six = 6;
	int i;

	/* Rotate from ECI to J2000 frame */
	/* Get rotation matrix from TEME @ET (sec past J2000 epoch) to J2000 */
	/* PRECM is 6x6, goes from J2000 -> TEME */
	zzteme_(&et, precm);
	/* Invert state transformation matrix to go from TEME -> J2000 */
	invstm_(precm, invprecm);
	/* Do transformation of state from EV2LIN's TEME to J2000 */
	mxvg_(invprecm, state, &six, &six, tmpstate);
	for(i = 0; i < 6; ++i)
	{
		state[i] = tmpstate[i];
	}
}
#endif

int computeDifxSpacecraftSourceEphemerisFromXYZ(DifxSpacecraft *ds, double sc_epoch_mjd, double mjd0, double deltat, int nPoint, double X, double Y, double Z, const char *ephemType, const char *naifFile, const char* orientationFile, double ephemClockError)
{
	if((ds->calculate_own_retarded_position))
	{
		fprintf(stderr, "Error: computing non-retarded spacecraft positions is not yet implemented.\n");
		return -1;
	}
	if(strcmp(ephemType, "SPICE")==0)
	{
#if HAVE_SPICE
		doublereal state[6];
		int p;

		ds->nPoint = nPoint;
		ds->pos = (sixVector *)calloc(nPoint, sizeof(sixVector));

		state[0] = X/1000.0;
		state[1] = Y/1000.0;
		state[2] = Z/1000.0;
		state[3] = state[4] = state[5] = 0.0;

		ldpool_c(naifFile);

		if(sc_epoch_mjd == 0.0)
		{
			for(p = 0; p < nPoint; ++p)
			{
				long double mjd, jd;
				char jdstr[24];
				doublereal et;

				mjd = mjd0 + p*deltat;
				jd = mjd + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
				sprintf(jdstr, "JD %18.12Lf", jd);
				str2et_c(jdstr, &et);

				ECI2J2000(et, state);

				ds->pos[p].mjd = mjd;
				ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
				ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
				ds->pos[p].Y = state[1]*1000.0;
				ds->pos[p].Z = state[2]*1000.0;
				ds->pos[p].dX = state[3]*1000.0;
				ds->pos[p].dY = state[4]*1000.0;
				ds->pos[p].dZ = state[5]*1000.0;
			}
		}
		else
		{
			long double mjd, jd;
			char jdstr[24];
			doublereal et;

			jd = (long double)(sc_epoch_mjd) + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
			sprintf(jdstr, "JD %18.12Lf", jd);
			str2et_c(jdstr, &et);

			ECI2J2000(et, state);
			for(p = 0; p < nPoint; ++p)
			{
				mjd = mjd0 + p*deltat;

				ds->pos[p].mjd = mjd;
				ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
				ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
				ds->pos[p].Y = state[1]*1000.0;
				ds->pos[p].Z = state[2]*1000.0;
				ds->pos[p].dX = 0.0;
				ds->pos[p].dY = 0.0;
				ds->pos[p].dZ = 0.0;
			}
		}	
		clpool_c();

		return 0;
#else
		fprintf(stderr, "Error: computeDifxSpacecraftEphemerisFromXYZ: spice not compiled into difxio.\n");
		
		return -2;
#endif
	}
	else
	{
		fprintf(stderr, "Error: computeDifxSpacecraftEphemerisFromXYZ: Unknown ephemType '%s'.\n", ephemType);
	}
	return -1;
	
}







static int computeDifxSpacecraftSourceEphemeris_bsp(DifxSpacecraft *ds, double sc_epoch_mjd, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemStellarAber, double ephemClockError)
{
	ds->is_antenna = 0;
	if(strcmp(ephemType, "SPICE")==0)
	{
#if HAVE_SPICE
		int spiceHandle;
		int p;
		int retcode;

		ldpool_c(naifFile);
		spklef_c(ephemFile, &spiceHandle);

		p = snprintf(ds->name, DIFXIO_NAME_LENGTH, "%s", objectName);
		if(p >= DIFXIO_NAME_LENGTH)
		{
			fprintf(stderr, "Warning: computeDifxSpacecraftSourceEphemeris_bsp: spacecraft name %s is too long %d > %d\n", objectName, p, DIFXIO_NAME_LENGTH-1);
		}
		ds->nPoint = nPoint;
		ds->pos = (sixVector *)calloc(nPoint, sizeof(sixVector));
		if(sc_epoch_mjd == 0.0)
		{
			for(p = 0; p < nPoint; ++p)
			{
				double state[6], range;
				long double mjd, jd;
				char jdstr[24];
				double et;
		
				mjd = mjd0 + p*deltat;
				jd = mjd + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
				sprintf(jdstr, "JD %18.12Lf", jd);
				str2et_c(jdstr, &et);
				if(ephemStellarAber == 0.0)
				{
					spkezr_c(objectName, et, "J2000", "LT", "399", state, &range);	/* 399 is the earth geocenter */
				}
				else if(ephemStellarAber == 1.0)
				{
					spkezr_c(objectName, et, "J2000", "LT+S", "399", state, &range);
				}
				else
				{
					double state2[6];
					int q;

					spkezr_c(objectName, et, "J2000", "LT+S", "399", state2, &range);
					spkezr_c(objectName, et, "J2000", "LT", "399", state, &range);

					for(q = 0; q < 6; ++q)
					{
						state[q] += ephemStellarAber*(state2[q] - state[q]);
					}
				}

				ds->pos[p].mjd = mjd;
				ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
				ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
				ds->pos[p].Y = state[1]*1000.0;
				ds->pos[p].Z = state[2]*1000.0;
				ds->pos[p].dX = state[3]*1000.0;
				ds->pos[p].dY = state[4]*1000.0;
				ds->pos[p].dZ = state[5]*1000.0;
			}
		}
		else
		{
			double state[6], range;
			long double mjd, jd;
			char jdstr[24];
			double et;
		
			jd = (long double)(sc_epoch_mjd) + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
			sprintf(jdstr, "JD %18.12Lf", jd);
			str2et_c(jdstr, &et);
			if(ephemStellarAber == 0.0)
			{
				spkezr_c(objectName, et, "J2000", "LT", "399", state, &range);	/* 399 is the earth geocenter */
			}
			else if(ephemStellarAber == 1.0)
			{
				spkezr_c(objectName, et, "J2000", "LT+S", "399", state, &range);
			}
			else
			{
				double state2[6];
				int q;

				spkezr_c(objectName, et, "J2000", "LT+S", "399", state2, &range);
				spkezr_c(objectName, et, "J2000", "LT", "399", state, &range);

				for(q = 0; q < 6; ++q)
				{
					state[q] += ephemStellarAber*(state2[q] - state[q]);
				}
			}
			for(p = 0; p < nPoint; ++p)
			{
				mjd = mjd0 + p*deltat;

				ds->pos[p].mjd = mjd;
				ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
				ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
				ds->pos[p].Y = state[1]*1000.0;
				ds->pos[p].Z = state[2]*1000.0;
				ds->pos[p].dX = 0.0;
				ds->pos[p].dY = 0.0;
				ds->pos[p].dZ = 0.0;
			}
		}
		spkuef_c(spiceHandle);
		clpool_c();


		/* Get the orientation parameters */
		free(ds->SCAxisVectors);
		ds->SCAxisVectors = (spacecraftAxisVectors *)calloc(nPoint, sizeof(spacecraftAxisVectors));
		if(((orientationFile)) && ((orientationFile[0]))) {
			fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris_bsp: SPICE orientation calculations not yet implemented in difxio.\n");
			return -3;
		}
		else
		{
			/* fprintf(stderr, "Warning: computeDifxSpacecraftSourceEphemeris_bsp: blank orientationFile --- orientation set to NULL.\n"); */
			for(p = 0; p < nPoint; p++)
			{
				ds->SCAxisVectors[p].X[0] = 0.0;
				ds->SCAxisVectors[p].X[1] = 0.0;
				ds->SCAxisVectors[p].X[2] = 0.0;
				ds->SCAxisVectors[p].Y[0] = 0.0;
				ds->SCAxisVectors[p].Y[1] = 0.0;
				ds->SCAxisVectors[p].Y[2] = 0.0;
				ds->SCAxisVectors[p].Z[0] = 0.0;
				ds->SCAxisVectors[p].Z[1] = 0.0;
				ds->SCAxisVectors[p].Z[2] = 0.0;
			}
		}

		retcode = computeDifxSpacecraftTimeFrameOffset(ds, JPLplanetaryephem);
		return (retcode < 0) ? -2 : 0;
#else
		fprintf(stderr, "Error: computeDifxSpacecraftEphemeris_bsp: spice not compiled into difxio.\n");
	
		return -1;
#endif
	}
	else
	{
		fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris_bsp: Unknown ephemType '%s'.\n", ephemType);
	}
	return -1;
}

#if HAVE_SPICE
static int findBestSet(double e, SpiceDouble *epochs, int nEpoch, double *f)
{
	int i;

	if(e <= epochs[0])
	{
		*f = 1.0;

		return 0;
	}
	if(e >= epochs[nEpoch - 1])
	{
		*f = 1.0;

		return nEpoch - 1;
	}

	for(i = 1; i < nEpoch; ++i)
	{
		if(epochs[i] >= e)
		{
			*f = (epochs[i]-e)/(epochs[i]-epochs[i-1]);

			return i - 1;
		}
	}

	return -1;
}

void evaluateTLE(doublereal et, doublereal *elems, doublereal *state)
{
	double R;
	doublereal geophysConsts[] =	/* values from http://naif.jpl.nasa.gov/pub/naif/toolkit_docs/FORTRAN/spicelib/ev2lin.html */
		{
			1.082616e-3,		/* J2 gravitational harmonic for earth */
			-2.53881e-6,		/* J3 gravitational harmonic for earth */
			-1.65597e-6,		/* J4 gravitational harmonic for earth */
			7.43669161e-2,		/* KE: Square root of the GM for earth where GM is expressed in earth radii cubed per minutes squared. */
			120.0,			/* QO: Low altitude bound for atmospheric model in km. */
			78.0,			/* SO: High altitude bound for atmospheric model in km. */
			6378.135,		/* RE: Equatorial radius of the earth in km. */
			1.0			/* AE: Distance units/earth radius (normally 1) */
		};

	if(elems[8] >= 2.0*M_PI/225.0)
	{
		ev2lin_(&et, geophysConsts, elems, state);

		R = sqrt(state[0]*state[0] + state[1]*state[1] + state[2]*state[2]);

		/* Adjust for light travel time from Earth Center to object */
		/* R comes out in km, et is in seconds */
		et -= R/299792.458;

		ev2lin_(&et, geophysConsts, elems, state);
	}
	else
	{
		dpspce_(&et, geophysConsts, elems, state);

		R = sqrt(state[0]*state[0] + state[1]*state[1] + state[2]*state[2]);

		/* Adjust for light travel time from Earth Center to object */
		/* R comes out in km, et is in seconds */
		et -= R/299792.458;

		dpspce_(&et, geophysConsts, elems, state);
	}

}
#endif

static int computeDifxSpacecraftSourceEphemeris_tle(DifxSpacecraft *ds, double sc_epoch_mjd, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemStellarAber, double ephemClockError)
{
	ds->is_antenna = 0;
	if(strcmp(ephemType, "SPICE")==0)
	{
#if HAVE_SPICE
		const int NElement = 10;
		const int MaxEphemElementSets = 30;
		const SpiceInt MaxLineLength = 512;
		const SpiceInt firstYear = 2000;
		int p;
		int retcode;
		doublereal elems[MaxEphemElementSets][NElement];
		SpiceDouble epochs[MaxEphemElementSets];
		int nSet = 0;
		FILE *in;
		char inLine[MaxLineLength];
		SpiceChar lines[2][MaxLineLength];	/* To store the Two Line Element (TLE) */

		in = fopen(ephemFile, "r");
		if(!in)
		{
			fprintf(stderr, "Error opening file %s\n", ephemFile);

			return -1;
		}

		ldpool_c(naifFile);

		for(;;)
		{
			char *rv;
			int l, i;

			l = strlen(objectName);

			rv = fgets(inLine, MaxLineLength-1, in);
			if(!rv)
			{
				break;
			}
			inLine[MaxLineLength-1] = 0;
			for(i = 0; inLine[i]; ++i)
			{
				if(inLine[i] < ' ')
				{
					inLine[i] = 0;
					break;
				}
			}

			if(inLine[0] == '1')
			{
				strcpy(lines[0], inLine);
			}
			else if(inLine[0] == '2')
			{
				strcpy(lines[1], inLine);

				if(strncmp(lines[0]+2, objectName, l) == 0 && strncmp(lines[1]+2, objectName, l) == 0)
				{
					SpiceDouble elemsTemp[NElement];	/* ensure proper type safety */
					int k;

					getelm_c(firstYear, MaxLineLength, lines, epochs + nSet, elemsTemp);
					for(k = 0; k < NElement; ++k)
					{
						elems[nSet][k] = elemsTemp[k];
					}

					++nSet;
				}
			}
		}

		fclose(in);

		if(nSet == 0)
		{
			fprintf(stderr, "Error: no TLEs were properly parsed from file %s\n", ephemFile);

			return -1;
		}

		p = snprintf(ds->name, DIFXIO_NAME_LENGTH, "%s", objectName);
		if(p >= DIFXIO_NAME_LENGTH)
		{
			fprintf(stderr, "Warning: computeDifxSpacecraftSourceEphemeris_tle: spacecraft name %s is too long %d > %d\n", objectName, p, DIFXIO_NAME_LENGTH-1);
		}
		ds->nPoint = nPoint;
		free(ds->pos);
		ds->pos = (sixVector *)calloc(nPoint, sizeof(sixVector));

		if(sc_epoch_mjd == 0.0)
		{
			for(p = 0; p < nPoint; ++p)
			{
				long double mjd, jd;
				char jdstr[24];
				doublereal et;
				int set;
				doublereal state[6];
				double f = -1.0;

				mjd = mjd0 + p*deltat;
				jd = mjd + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
				sprintf(jdstr, "JD %18.12Lf", jd);
				str2et_c(jdstr, &et);

				set = findBestSet(et, epochs, nSet, &f);

				if(f < 0.0)
				{
					fprintf(stderr, "Developer error: computeDifxSpacecraftSourceEphemeris_tle: f < 0 (= %f)\n", f);

					exit(EXIT_FAILURE);
				}

				evaluateTLE(et, elems[set], state);

				if(f < 1.0)
				{
					/* linear interpolation between two TLE values */

					doublereal state2[6];
					int j;

					evaluateTLE(et, elems[set+1], state2);
					for(j = 0; j < 6; ++j)
					{
						state[j] = f*state[j] + (1.0-f)*state2[j];
					}
				}

				ECI2J2000(et, state);

				ds->pos[p].mjd = mjd;
				ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
				ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
				ds->pos[p].Y = state[1]*1000.0;
				ds->pos[p].Z = state[2]*1000.0;
				ds->pos[p].dX = state[3]*1000.0;
				ds->pos[p].dY = state[4]*1000.0;
				ds->pos[p].dZ = state[5]*1000.0;
			}
		}
		else
		{
			long double mjd, jd;
			char jdstr[24];
			doublereal et;
			int set;
			doublereal state[6];
			double f = -1.0;

			jd = (long double)(sc_epoch_mjd) + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
			sprintf(jdstr, "JD %18.12Lf", jd);
			str2et_c(jdstr, &et);

			set = findBestSet(et, epochs, nSet, &f);

			if(f < 0.0)
			{
				fprintf(stderr, "Developer error: computeDifxSpacecraftSourceEphemeris_tle: f < 0 (= %f)\n", f);

				exit(EXIT_FAILURE);
			}

			evaluateTLE(et, elems[set], state);

			if(f < 1.0)
			{
				/* linear interpolation between two TLE values */

				doublereal state2[6];
				int j;

				evaluateTLE(et, elems[set+1], state2);
				for(j = 0; j < 6; ++j)
				{
					state[j] = f*state[j] + (1.0-f)*state2[j];
				}
			}

			ECI2J2000(et, state);
			for(p = 0; p < nPoint; ++p)
			{
				mjd = mjd0 + p*deltat;

				ds->pos[p].mjd = mjd;
				ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
				ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
				ds->pos[p].Y = state[1]*1000.0;
				ds->pos[p].Z = state[2]*1000.0;
				ds->pos[p].dX = 0.0;
				ds->pos[p].dY = 0.0;
				ds->pos[p].dZ = 0.0;
			}
		}
		clpool_c();


		/* Get the orientation parameters */
		free(ds->SCAxisVectors);
		ds->SCAxisVectors = (spacecraftAxisVectors *)calloc(nPoint, sizeof(spacecraftAxisVectors));
		if(((orientationFile)) && ((orientationFile[0]))) {
			fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris_tle: SPICE orientation calculations not yet implemented in difxio.\n");
			return -3;
		}
		else
		{
			/* fprintf(stderr, "Warning: computeDifxSpacecraftSourceEphemeris_tle: blank orientationFile --- orientation set to NULL.\n"); */
			for(p = 0; p < nPoint; p++)
			{
				ds->SCAxisVectors[p].X[0] = 0.0;
				ds->SCAxisVectors[p].X[1] = 0.0;
				ds->SCAxisVectors[p].X[2] = 0.0;
				ds->SCAxisVectors[p].Y[0] = 0.0;
				ds->SCAxisVectors[p].Y[1] = 0.0;
				ds->SCAxisVectors[p].Y[2] = 0.0;
				ds->SCAxisVectors[p].Z[0] = 0.0;
				ds->SCAxisVectors[p].Z[1] = 0.0;
				ds->SCAxisVectors[p].Z[2] = 0.0;
			}
		}

		retcode = computeDifxSpacecraftTimeFrameOffset(ds, JPLplanetaryephem);
		return (retcode < 0) ? -2 : 0;
#else
		fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris_tle: spice not compiled into difxio.\n");
	
		return -1;
#endif
	}
	else
	{
		fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris_tle: Unknown ephemType '%s'.\n", ephemType);
	}
	return -1;
}

int computeDifxSpacecraftSourceEphemeris(DifxSpacecraft *ds, double sc_epoch_mjd, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemStellarAber, double ephemClockError)
{
	if((ds->calculate_own_retarded_position))
	{
		fprintf(stderr, "Error: computing non-retarded positions for sources is not yet implemented.\n");
		return -1;
	}
	if((ds->is_antenna))
	{
		fprintf(stderr, "Error: computing source ephemeris for spacecraft antenna is not allowed.\n");
		return -2;
	}
	if(strcmp(ephemType, "SPICE")==0)
	{
		int l;

		/* TODO: add mechanism to just load in state vectors */

		l = strlen(ephemFile);
		if(l > 4 && strcmp(ephemFile+l-4, ".bsp") == 0)
		{
			return computeDifxSpacecraftSourceEphemeris_bsp(ds, sc_epoch_mjd, mjd0, deltat, nPoint, objectName, ephemType, naifFile, ephemFile, orientationFile, JPLplanetaryephem, ephemStellarAber, ephemClockError);
		}
		else if(l > 4 && strcmp(ephemFile+l-4, ".tle") == 0)
		{
			return computeDifxSpacecraftSourceEphemeris_tle(ds, sc_epoch_mjd, mjd0, deltat, nPoint, objectName, ephemType, naifFile, ephemFile, orientationFile, JPLplanetaryephem, ephemStellarAber, ephemClockError);
		}
		else
		{
			fprintf(stderr, "Error: ephemFile (%s) is not of a recognized ephemeris type.  The file should end in .tle or .bsp .\n", ephemFile);
		
			return -3;
		}
	}
	else {
		fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris: Unknown ephemType '%s'.\n", ephemType);
	}
	return -4;
}

int computeDifxSpacecraftAntennaEphemeris(DifxSpacecraft *ds, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemClockError)
{
	ds->is_antenna = 1;
	if(strcmp(ephemType, "SPICE")==0)
	{
#if HAVE_SPICE
		int spiceHandle;
		int p;
		long double mjd, jd;
		char jdstr[24];
		double et;
		double state[6], range;
		int retcode;
				
		ldpool_c(naifFile);
		spklef_c(ephemFile, &spiceHandle);
				
		p = snprintf(ds->name, DIFXIO_NAME_LENGTH, "%s", objectName);
		if(p >= DIFXIO_NAME_LENGTH)
		{
			fprintf(stderr, "Warning: computeDifxSpacecraftAntennaEphemeris: spacecraft name %s is too long %d > %d\n",
					objectName, p, DIFXIO_NAME_LENGTH-1);
		}
		ds->nPoint = nPoint;
		free(ds->pos);
		ds->pos = (sixVector *)calloc(nPoint, sizeof(sixVector));
		for(p = 0; p < nPoint; ++p)
		{
			mjd = mjd0 + p*deltat;
			jd = mjd + 2400000.5 + ephemClockError/SECONDS_PER_DAY;
			sprintf(jdstr, "JD %18.12Lf", jd);
			str2et_c(jdstr, &et);
			spkezr_c(objectName, et, "J2000", "NONE", "399", state, &range);	/* ITRF93 is the standard, high precision reference frame fixed to the Earth's crust, 399 is the earth geocenter */
						
			ds->pos[p].mjd = mjd;
			ds->pos[p].fracDay = mjd - ds->pos[p].mjd;
			ds->pos[p].X = state[0]*1000.0;	/* Convert to m and m/s from km and km/s */
			ds->pos[p].Y = state[1]*1000.0;
			ds->pos[p].Z = state[2]*1000.0;
			ds->pos[p].dX = state[3]*1000.0;
			ds->pos[p].dY = state[4]*1000.0;
			ds->pos[p].dZ = state[5]*1000.0;
		}
				
		spkuef_c(spiceHandle);
		clpool_c();
				
		/* Get the orientation parameters */
		free(ds->SCAxisVectors);
		ds->SCAxisVectors = (spacecraftAxisVectors *)calloc(nPoint, sizeof(spacecraftAxisVectors));
		if(((orientationFile)) && ((orientationFile[0]))) {
			fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris: SPICE orientation calculations not yet implemented in difxio.\n");
			return -3;
		}
		else
		{
			fprintf(stderr, "Warning: computeDifxSpacecraftSourceEphemeris: blank orientationFile --- orientation set to NULL.\n");
			for(p = 0; p < nPoint; p++)
			{
				ds->SCAxisVectors[p].X[0] = 0.0;
				ds->SCAxisVectors[p].X[1] = 0.0;
				ds->SCAxisVectors[p].X[2] = 0.0;
				ds->SCAxisVectors[p].Y[0] = 0.0;
				ds->SCAxisVectors[p].Y[1] = 0.0;
				ds->SCAxisVectors[p].Y[2] = 0.0;
				ds->SCAxisVectors[p].Z[0] = 0.0;
				ds->SCAxisVectors[p].Z[1] = 0.0;
				ds->SCAxisVectors[p].Z[2] = 0.0;
			}
		}

		retcode =  computeDifxSpacecraftTimeFrameOffset(ds, JPLplanetaryephem);
		return (retcode < 0) ? -2 : 0;
#else
		fprintf(stderr, "Error: computeDifxSpacecraftAntennaEphemeris: spice not compiled into difxio.\n");
				
		return -1;
#endif
	}
	else if(strcmp(ephemType,"RUSSCF") == 0)
	{
		/* Russian RadioAstron text format */
		int p;
		int retcode;
		p = snprintf(ds->name, DIFXIO_NAME_LENGTH, "%s", objectName);
		if(p >= DIFXIO_NAME_LENGTH)
		{
			fprintf(stderr, "Warning: computeDifxSpacecraftAntennaEphemeris: spacecraft name %s is too long %d > %d\n",
					objectName, p, DIFXIO_NAME_LENGTH-1);
		}
		p = read_Russian_scf_file(ephemFile, objectName,
								  mjd0, mjd0+(nPoint-1)*deltat,
								  deltat, ephemClockError, ds);
		if((p)){
			fprintf(stderr, "Error: computeDifxSpacecraftAntennaEphemeris: read_Russian_scf_file returned %d.\n", p);
			return -1;
		}
		/* read_Russian_scf_file computes it's own nPoint and
		   mjd0, so make sure to use the correct values in subsequent
		   code below.
		*/
		nPoint = ds->nPoint;
		/* Get the orientation parameters */
		free(ds->SCAxisVectors);
		ds->SCAxisVectors = (spacecraftAxisVectors *)calloc(nPoint, sizeof(spacecraftAxisVectors));
		if(((orientationFile)) && ((orientationFile[0]))) {
			p = read_Russian_scf_axes_file(orientationFile,
										   mjd0, mjd0+(nPoint-1)*deltat,
										   ephemClockError, ds);
			if((p)) {
				fprintf(stderr, "Error: computeDifxSpacecraftSourceEphemeris: read_Russian_scf_axes_file returned %d.\n", p);
				return -4;
			}
		}
		else
		{
			fprintf(stderr, "Warning: computeDifxSpacecraftSourceEphemeris: blank orientationFile --- orientation set to NULL.\n");
			for(p = 0; p < nPoint; p++)
			{
				ds->SCAxisVectors[p].X[0] = 0.0;
				ds->SCAxisVectors[p].X[1] = 0.0;
				ds->SCAxisVectors[p].X[2] = 0.0;
				ds->SCAxisVectors[p].Y[0] = 0.0;
				ds->SCAxisVectors[p].Y[1] = 0.0;
				ds->SCAxisVectors[p].Y[2] = 0.0;
				ds->SCAxisVectors[p].Z[0] = 0.0;
				ds->SCAxisVectors[p].Z[1] = 0.0;
				ds->SCAxisVectors[p].Z[2] = 0.0;
			}
		}

		retcode =  computeDifxSpacecraftTimeFrameOffset(ds, JPLplanetaryephem);
		return (retcode < 0) ? -2 : 0;

		return 0;
	}
	else
	{
		fprintf(stderr, "Error: computeDifxSpacecraftAntennaEphemeris: Unknown ephemType '%s'.\n", ephemType);
	}
	return -1;
}
int computeDifxSpacecraftEphemerisOffsets(DifxSpacecraft *ds)
{
	int p;
	int retval;
	nineVector offset;
	for(p = 0; p < ds->nPoint; ++p)
	{
		retval = evaluateDifxSpacecraftAntennaOffset(ds,ds->pos[p].mjd,ds->pos[p].fracDay,&offset);
		if((retval)) {
			return retval;
		}
		ds->pos[p].X += offset.X;
		ds->pos[p].Y += offset.Y;
		ds->pos[p].Z += offset.Z;
		ds->pos[p].dX += offset.dX;
		ds->pos[p].dY += offset.dY;
		ds->pos[p].dZ += offset.dZ;
	}
	return 0;
}

static void copySpacecraft(DifxSpacecraft *dest, const DifxSpacecraft *src)
{
	if(dest != src)
	{
		deleteDifxSpacecraftInternals(dest);
		*dest = *src;
		dest->pos = (sixVector *)malloc(dest->nPoint* sizeof(sixVector));
		memcpy(dest->pos, src->pos, dest->nPoint*sizeof(sixVector));
		if(src->TFrameOffset)
		{
			dest->TFrameOffset = (spacecraftTimeFrameOffset *)malloc(dest->nPoint* sizeof(spacecraftTimeFrameOffset));
			memcpy(dest->TFrameOffset, src->TFrameOffset, dest->nPoint*sizeof(spacecraftTimeFrameOffset));
		}
		if(src->SCAxisVectors)
		{
			dest->SCAxisVectors = (spacecraftAxisVectors *)malloc(dest->nPoint* sizeof(spacecraftAxisVectors));
			memcpy(dest->SCAxisVectors, src->SCAxisVectors, dest->nPoint*sizeof(spacecraftAxisVectors));
		}
	}
}

static void mergeSpacecraft(DifxSpacecraft *dest, const DifxSpacecraft *src1, const DifxSpacecraft *src2)
{
	double end1;
	int j;
	deleteDifxSpacecraftInternals(dest);
	*dest = *src1;
	/* snprintf(dest->name, DIFXIO_NAME_LENGTH, "%s", src1->name); */
	
	/* put in time order */
	if(src1->pos->mjd + src1->pos->fracDay > src2->pos->mjd + src2->pos->fracDay)
	{
		const DifxSpacecraft *tmp;

		tmp = src1;
		src1 = src2;
		src2 = tmp;
	}

	end1 = src1->pos[src1->nPoint-1].mjd + src1->pos[src1->nPoint-1].fracDay;
	for(j = 0; j < src2->nPoint; ++j)
	{
		double end;

		end = src2->pos[j].mjd + src2->pos[j].fracDay;

		if(end > end1)
		{
			break;
		}
	}
	dest->nPoint = src1->nPoint + src2->nPoint - j;
	dest->pos = (sixVector *)malloc(dest->nPoint* sizeof(sixVector));
	memcpy(dest->pos, src1->pos, src1->nPoint*sizeof(sixVector));
	if((src1->TFrameOffset))
	{
		if(!src2->TFrameOffset)
		{
			fprintf(stderr, "Error in mergeSpacecraft: spacecraft 1 (%s) has TFrameOffset, but spacecraft 2 (%s) does not.", src1->name, src2->name);
			exit(EXIT_FAILURE);
		}
		dest->TFrameOffset = (spacecraftTimeFrameOffset *)malloc(dest->nPoint* sizeof(spacecraftTimeFrameOffset));
		memcpy(dest->TFrameOffset, src1->TFrameOffset, src1->nPoint*sizeof(spacecraftTimeFrameOffset));
	}
	else if((src2->TFrameOffset))
	{
		fprintf(stderr, "Error in mergeSpacecraft: spacecraft 2 (%s) has TFrameOffset, but spacecraft 1 (%s) does not.", src2->name, src1->name);
		exit(EXIT_FAILURE);
	}
	if((src1->SCAxisVectors))
	{
		if(!src2->SCAxisVectors)
		{
			fprintf(stderr, "Error in mergeSpacecraft: spacecraft 1 (%s) has SCAxisVectors, but spacecraft 2 (%s) does not.", src1->name, src2->name);
			exit(EXIT_FAILURE);
		}
		dest->SCAxisVectors = (spacecraftAxisVectors *)malloc(dest->nPoint* sizeof(sixVector));
		memcpy(dest->SCAxisVectors, src1->SCAxisVectors, src1->nPoint*sizeof(spacecraftAxisVectors));
	}
	else if((src2->SCAxisVectors))
	{
		fprintf(stderr, "Error in mergeSpacecraft: spacecraft 2 (%s) has SCAxisVectors, but spacecraft 1 (%s) does not.", src2->name, src1->name);
		exit(EXIT_FAILURE);
	}
	if(j < src2->nPoint)
	{
		memcpy(dest->pos + src1->nPoint, src2->pos + j, (src2->nPoint - j)*sizeof(sixVector));
		if((src2->TFrameOffset))
		{
			memcpy(dest->TFrameOffset + src2->nPoint, src2->TFrameOffset + j, (src2->nPoint - j)*sizeof(spacecraftTimeFrameOffset));
		}
		if((src2->SCAxisVectors))
		{
			memcpy(dest->SCAxisVectors + src2->nPoint, src2->SCAxisVectors + j, (src2->nPoint - j)*sizeof(spacecraftAxisVectors));
		}
	}
}

/* note: returns number of spacecraft on call stack */
DifxSpacecraft *mergeDifxSpacecraft(const DifxSpacecraft *ds1, int nds1, const DifxSpacecraft *ds2, int nds2, int *spacecraftIdRemap, int *nds)
{
	DifxSpacecraft *ds;
	int i, j;

	if(nds1 <= 0 && nds2 <= 0)
	{
		*nds = 0;

		return 0;
	}

	if(nds2 <= 0)
	{
		*nds = nds1;

		return dupDifxSpacecraftArray(ds1, nds1);
	}

	if(nds1 <= 0)
	{
		*nds = nds2;
		for(i = 0; i < nds2; ++i)
		{
			spacecraftIdRemap[i] = i;
		}
		return dupDifxSpacecraftArray(ds2, nds2);
	}

	/* both have spacecraft tables, so do the merge */

	*nds = nds1;

	/* first identify entries that differ and assign new spacecraftIds */
	for(j = 0; j < nds2; ++j)
	{
		for(i = 0; i < nds1; ++i)
		{
			if(strcmp(ds1[i].name, ds2[j].name) == 0)
			{
				if(ds1[i].is_antenna != ds2[j].is_antenna)
				{
					fprintf(stderr, "Error in mergeDifxSpacecraft: spacecraft '%s' has is_antenna=%d in first list, but is_antenna=%d in second list.  Cannot merge antennas with sources.  Rename to separate sources from antennas for spacecraft.\n", ds1[i].name, ds1[i].is_antenna, ds2[j].is_antenna);
					exit(EXIT_FAILURE);
				}
				spacecraftIdRemap[j] = i;
				break;
			}
		}
		if(i == nds1)
		{
			spacecraftIdRemap[j] = *nds;
			++(*nds);
		}
	}

	ds = newDifxSpacecraftArray(*nds);

	for(i = 0; i < nds1; ++i)
	{
		/* see if the spacecraft is common to both input tables */
		for(j = 0; j < nds2; ++j)
		{
			if(spacecraftIdRemap[j] == i)
			{
				break;
			}
		}
		if(j < nds2)	/* yes -- both have it! */
		{
			mergeSpacecraft(ds + i, ds1 + i, ds2 + j);
		}
		else		/* no -- just in first table */
		{
			copySpacecraft(ds + i, ds1 + i);
		}
	}

	/* finally go through input table 2 and copy unique ones */
	for(j = 0; j < nds2; ++j)
	{
		i = spacecraftIdRemap[j];
		if(i >= nds1) /* it is unique to second input */
		{
			copySpacecraft(ds + i, ds2 + j);
		}
	}

	return ds;
}


int shiftSpacecraftClockPolys(DifxInput *D)
{
	int a, sc;
	if(D->nSpacecraft <= 0)
	{
		return +1;
	}
	if(!D->job)
	{
		fprintf(stderr, "Error: shiftSpacecraftClockPolys: D->job == 0!\n");
		return -2;
	}
	if(D->nJob != 1)
	{
		fprintf(stderr, "Error: shiftSpacecraftClockPolys: nJob = %d (not 1)\n", D->nJob);
		return -3;
	}
	for(a = 0; a < D->nAntenna; a++)
	{
		sc = D->antenna[a].spacecraftId;
		if(sc >= 0)
		{
			if(D->spacecraft[sc].is_antenna)
			{
				if(D->spacecraft[sc].spacecraft_time_type==SpacecraftTimeGroundReception)
				{
					/* The clock is reset at the time of the
					   ground station recording system.	 Recompute the
					   spacecraft clock polynomials for the time of
					   synchronization, and then zero out the constant
					   offset, since that offset is set by the ground
					   synchronization process.
					*/
					double c_poly[MAX_MODEL_ORDER+1];
					double x_poly[MAX_MODEL_ORDER+1];
					double a_poly[MAX_MODEL_ORDER+1];
					double t_poly[MAX_MODEL_ORDER+1];
					double offset = D->antenna[a].clockrefmjd
									- D->spacecraft[sc].GS_mjd_sync
									- D->spacecraft[sc].GS_dayfraction_sync;
					int i,j;
					for(i=0; i < MAX_MODEL_ORDER+1; ++i)
					{
						c_poly[i] = x_poly[i] = a_poly[i] = t_poly[i] = 0.0;
					}
					for(j=0; j < D->antenna[a].clockorder+1; ++j)
					{
						if(j>0)
						{
							for(i=0; i < j; ++i)
							{
								x_poly[i+1] = t_poly[i];
								a_poly[i] = t_poly[i] * offset;
							}
							x_poly[0] = 0.0;
						}
						else
						{
							x_poly[0] = 1.0;
						}
						for(i=0; i <= j; ++i)
						{
							t_poly[i] = x_poly[i] + a_poly[i];
							c_poly[i] += t_poly[i] * D->antenna[a].clockcoeff[j];
						}
					}
					/* remember to zero out the constant offset, as the
					   ground station rereferencing destroys that
					*/
					D->antenna[a].clockcoeff[0] = 0.0;
					for(j=1; j < D->antenna[a].clockorder+1; ++j)
					{
						D->antenna[a].clockcoeff[j] = c_poly[j];
					}
					D->antenna[a].clockrefmjd = D->spacecraft[sc].GS_mjd_sync
												+ D->spacecraft[sc].GS_dayfraction_sync;
				}
				else if(D->spacecraft[sc].spacecraft_time_type==SpacecraftTimeGroundClock)
				{
					/* Zero out the spacecraft clock poly, as the
					   ground station clock sets the poly
					*/
					int j;
					for(j=0; j < D->antenna[a].clockorder+1; ++j)
					{
						D->antenna[a].clockcoeff[j] = 0.0;
					}
				}
				else
				{
					/* nothing to do */
				}
			}
		}
	}
	return 0;
}








static void evalPoly(long double poly[4], long double t, long double *V)
{
	*V = poly[0] + t*(poly[1] + t*(poly[2] + t*poly[3]));
}

int evaluateDifxSpacecraftSource(const DifxSpacecraft *sc, int mjd, double fracMjd, sixVector *interpolatedPosition)
{
	/* Note that the units of velocity in the returned sixVector are
	   m/s, not m/day as of programming developments on 2015 Apr 09
	*/
	int nRow;
	const sixVector *pos;
	long double t0, t1, tMod, t, deltat;
	long double xPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	long double yPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	long double zPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	int r, r0, r1;
	long double X, Y, Z, dX, dY, dZ;
	
	nRow = sc->nPoint;
	pos = sc->pos;
	
	tMod = mjd + fracMjd;
	
	/* first find interpolation points */
	t0 = 0.0;
	t1 = pos[0].mjd + pos[0].fracDay;
	for(r = 1; r < nRow; ++r)
	{
		t0 = t1;
		t1 = pos[r].mjd + pos[r].fracDay;
		if(t0 <= tMod && tMod <= t1)
		{
			break;
		}
	}
	if(r == nRow)
	{
		if(tMod == t1) {
			r--;
		}
		else {
			return -1;
		}
	}

	/* calculate polynomial for X, Y, Z */
	r0 = r-1;
	r1 = r;
	deltat = t1 - t0;
	t = (tMod - t0)/deltat; /* time, fraction of interval, between 0 and 1 */

	if(t < -0.01 || t > 1.01)
	{
		fprintf(stderr, "WARNING: potentially unhealthy interpolation of state vector occurring mjd=%14.8Lf t0=%14.8Lf t1=%14.8Lf\n", tMod, t0, t1);
	}

	xPoly[0] = pos[r0].X;
	xPoly[1] = pos[r0].dX*deltat;
	xPoly[2] = -3.0L*(pos[r0].X-pos[r1].X) - (2.0L*pos[r0].dX+pos[r1].dX)*deltat;
	xPoly[3] =	2.0L*(pos[r0].X-pos[r1].X) + (	  pos[r0].dX+pos[r1].dX)*deltat;
	yPoly[0] = pos[r0].Y;
	yPoly[1] = pos[r0].dY*deltat;
	yPoly[2] = -3.0L*(pos[r0].Y-pos[r1].Y) - (2.0L*pos[r0].dY+pos[r1].dY)*deltat;
	yPoly[3] =	2.0L*(pos[r0].Y-pos[r1].Y) + (	  pos[r0].dY+pos[r1].dY)*deltat;
	zPoly[0] = pos[r0].Z;
	zPoly[1] = pos[r0].dZ*deltat;
	zPoly[2] = -3.0L*(pos[r0].Z-pos[r1].Z) - (2.0L*pos[r0].dZ+pos[r1].dZ)*deltat;
	zPoly[3] =	2.0L*(pos[r0].Z-pos[r1].Z) + (	  pos[r0].dZ+pos[r1].dZ)*deltat;

	evalPoly(xPoly, t, &X);
	evalPoly(yPoly, t, &Y);
	evalPoly(zPoly, t, &Z);

#if 0
	/* linear interpolation of velocity gives smoother results than
	 * evaluating derivative polynomial.  Why??? 
	 */
	dX = pos[r0].dX + t*(pos[r1].dX - pos[r0].dX);
	dY = pos[r0].dY + t*(pos[r1].dY - pos[r0].dY);
	dZ = pos[r0].dZ + t*(pos[r1].dZ - pos[r0].dZ);
#endif

	/* override interpolation with linear... */
	X  = pos[r0].X*(1.0-t) + pos[r1].X*t;
	Y  = pos[r0].Y*(1.0-t) + pos[r1].Y*t;
	Z  = pos[r0].Z*(1.0-t) + pos[r1].Z*t;
	dX = pos[r0].dX*(1.0-t) + pos[r1].dX*t;
	dY = pos[r0].dY*(1.0-t) + pos[r1].dY*t;
	dZ = pos[r0].dZ*(1.0-t) + pos[r1].dZ*t;

	interpolatedPosition->mjd = mjd;
	interpolatedPosition->fracDay = fracMjd;
	interpolatedPosition->X = X;
	interpolatedPosition->Y = Y;
	interpolatedPosition->Z = Z;
	interpolatedPosition->dX = dX * SECONDS_PER_DAY; /* convert m/day to m/s */
	interpolatedPosition->dY = dY * SECONDS_PER_DAY; /* convert m/day to m/s */
	interpolatedPosition->dZ = dZ * SECONDS_PER_DAY; /* convert m/day to m/s */

	return r;
}

int evaluateDifxSpacecraftAntenna(const DifxSpacecraft *sc, int mjd,
								  double fracMjd,
								  nineVector *interpolatedPosition)
{
	int nRow;
	const sixVector * restrict pos;
	double t0, t1, tMod;
	double tPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	double pPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	double vPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	int r, r0;
	double X, Y, Z, dX, dY, dZ, ddX, ddY, ddZ;
	
	nRow = sc->nPoint;
	pos = sc->pos;
	
	tMod = mjd + fracMjd;
	
	/* first find interpolation points */
	t0 = 0.0;
	t1 = pos[0].mjd + pos[0].fracDay;
	for(r = 1; r < nRow; ++r)
	{
		t0 = t1;
		t1 = pos[r].mjd + pos[r].fracDay;
		if(t0 <= tMod && tMod < t1)
		{
			break;
		}
	}
	if(r == nRow)
	{
		if(tMod == t1) {
			--r;
		}
		else {
			return -1;
		}
	}
	r -= DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER/2;
	if(r<0) r=0;
	if(r+DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER > nRow) {
		r = nRow - DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER;
	}
	if(nRow < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER) {
		return -2;
	}
	/* Compute the temporary spacecraft position and velocity arrays. */
	/* Note that the time array must have units matching the units of */
	/* position/velocity in order for the Hermite interpolation to work */
	/* properly. */
	tMod = ((mjd-pos[r].mjd) + (fracMjd-pos[r].fracDay)) * SECONDS_PER_DAY;
	for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
		tPoly[r0] = ((pos[r+r0].mjd-pos[r].mjd)
					+ (pos[r+r0].fracDay-pos[r].fracDay)) * SECONDS_PER_DAY;
		pPoly[r0] = pos[r+r0].X;
		vPoly[r0] = pos[r+r0].dX;
	}
	hermite_interpolation_pva(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
							  tPoly, pPoly, vPoly, tMod,
							 &X, &dX, &ddX);
	for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
		pPoly[r0] = pos[r+r0].Y;
		vPoly[r0] = pos[r+r0].dY;
	}
	hermite_interpolation_pva(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
							  tPoly, pPoly, vPoly, tMod,
							 &Y, &dY, &ddY);
	for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
		pPoly[r0] = pos[r+r0].Z;
		vPoly[r0] = pos[r+r0].dZ;
	}
	hermite_interpolation_pva(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
							  tPoly, pPoly, vPoly, tMod,
							 &Z, &dZ, &ddZ);
	/* copy location to output */
	interpolatedPosition->mjd = mjd;
	interpolatedPosition->fracDay = fracMjd;
	interpolatedPosition->X = X;
	interpolatedPosition->Y = Y;
	interpolatedPosition->Z = Z;
	interpolatedPosition->dX = dX;
	interpolatedPosition->dY = dY;
	interpolatedPosition->dZ = dZ;
	interpolatedPosition->ddX = ddX;
	interpolatedPosition->ddY = ddY;
	interpolatedPosition->ddZ = ddZ;

	return r;
}


int evaluateDifxSpacecraftAntennaOffset(const DifxSpacecraft *sc, int mjd,
										double fracMjd,
										nineVector *interpolatedOffset)
{
	/* \Delta\boldsymbol{x} = (\Delta t)^0 \boldsymbol{x} + (\Delta t)^1 \boldsymbol{v} + \frac{(\Delta t)^2}{2} \boldsymbol{a} + \frac{(\Delta t)^2}{6} \boldsymbol{j} + \ldots
	   Velocity and acceleration are calculated as the appropriate time
	   derivatives of the above equation.

	   The position offset polynomial values have units of m, m/s, m/s^2, ...
	*/

	nineVector offset;
	int i;
	double Delta_t;
	double DtN;
	double DtNm1;
	double DtNm2;
	double factorial_factor;

	memset(&offset, 0, sizeof(nineVector));
	offset.mjd = mjd;
	offset.fracDay = fracMjd;

	Delta_t = ((mjd-sc->SC_pos_offset_refmjd)
			  +(fracMjd-sc->SC_pos_offset_reffracDay)) * SECONDS_PER_DAY;
	DtN = 1.0;
	DtNm1 = 0.0;
	DtNm2 = 0.0;
	factorial_factor = 1.0;

	for(i=0; i <= sc->SC_pos_offsetorder;) {
		offset.X += sc->SC_pos_offset[i].X * (DtN * factorial_factor);
		offset.Y += sc->SC_pos_offset[i].Y * (DtN * factorial_factor);
		offset.Z += sc->SC_pos_offset[i].Z * (DtN * factorial_factor);
		offset.dX += sc->SC_pos_offset[i].X * (DtNm1 * factorial_factor * i);
		offset.dY += sc->SC_pos_offset[i].Y * (DtNm1 * factorial_factor * i);
		offset.dZ += sc->SC_pos_offset[i].Z * (DtNm1 * factorial_factor * i);
		offset.ddX += sc->SC_pos_offset[i].X * (DtNm2 * factorial_factor * i * (i-1));
		offset.ddY += sc->SC_pos_offset[i].Y * (DtNm2 * factorial_factor * i * (i-1));
		offset.ddZ += sc->SC_pos_offset[i].Z * (DtNm2 * factorial_factor * i * (i-1));
		++i;
		DtNm2 = DtNm1;
		DtNm1 = DtN;
		DtN *= Delta_t;
		factorial_factor /= i;
	}
	/* printf("DEBUG: Offset position at %d %.7f %10.3E %10.3E %10.3E	 %10.3E %10.3E %10.3E	 %10.3E %10.3E %10.3E\n", mjd, fracMjd, offset.X, offset.Y, offset.Z, offset.dX, offset.dY, offset.dZ, offset.ddX, offset.ddY, offset.ddZ); */

	*interpolatedOffset = offset;

	return 0;
}

int evaluateDifxSpacecraftAntennaAxisVectors(const DifxSpacecraft *sc,
											 int mjd,
											 double fracMjd,
											 spacecraftAxisVectors* direction,
											 spacecraftAxisVectors* velocity)
{
	int nRow;
	const sixVector * restrict pos;
	const spacecraftAxisVectors * restrict raw_axis_info;
	double t0, t1, tMod;
	double tPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	double pPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	int r, r0;
	int i;
	double p,v;
	double sum;
	
	nRow = sc->nPoint;
	pos = sc->pos;
	raw_axis_info = sc->SCAxisVectors;
	
	tMod = mjd + fracMjd;
	
	/* first find interpolation points */
	t0 = 0.0;
	t1 = pos[0].mjd + pos[0].fracDay;
	for(r = 1; r < nRow; ++r)
	{
		t0 = t1;
		t1 = pos[r].mjd + pos[r].fracDay;
		if(t0 <= tMod && tMod < t1)
		{
			break;
		}
	}
	if(r == nRow)
	{
		if(tMod == t1) {
			--r;
		}
		else {
			return -1;
		}
	}
	r -= DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER/2;
	if(r<0) r=0;
	if(r+DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER > nRow) {
		r = nRow - DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER;
	}
	if(nRow < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER) {
		return -2;
	}
	/* Note that the velocity will come out with inverse time units
	   corresponding to the time array units.
	*/
	tMod = ((mjd-pos[r].mjd) + (fracMjd-pos[r].fracDay)) * SECONDS_PER_DAY;
	for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; ++r0) {
		tPoly[r0] = ((pos[r+r0].mjd-pos[r].mjd)
					+ (pos[r+r0].fracDay-pos[r].fracDay)) * SECONDS_PER_DAY;
	}
	/* X axis */
	sum = 0.0;
	for(i=0; i < 3; ++i) {
		for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; ++r0) {
			pPoly[r0] = raw_axis_info[r+r0].X[i];
		}
		Neville_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
									  tPoly, pPoly, tMod, &p, &v);
		direction->X[i] = p;
		velocity->X[i] = v;
		sum += p*p;
	}
	if((sum > 0.5) && (sum < 1.5)) {
		/* adjust unit vectors back to unity */
		sum = 1.0/sqrt(sum);
		for(i=0; i < 3; ++i) {
			direction->X[i] *= sum;
			velocity->X[i] *= sum;
		}
	}
	/* Y axis */
	sum = 0.0;
	for(i=0; i < 3; ++i) {
		for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; ++r0) {
			pPoly[r0] = raw_axis_info[r+r0].Y[i];
		}
		Neville_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
									  tPoly, pPoly, tMod, &p, &v);
		direction->Y[i] = p;
		velocity->Y[i] = v;
		sum += p*p;
	}
	if((sum > 0.5) && (sum < 1.5)) {
		/* adjust unit vectors back to unity */
		sum = 1.0/sqrt(sum);
		for(i=0; i < 3; ++i) {
			direction->Y[i] *= sum;
			velocity->Y[i] *= sum;
		}
	}
	/* Z axis */
	sum = 0.0;
	for(i=0; i < 3; ++i) {
		for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; ++r0) {
			pPoly[r0] = raw_axis_info[r+r0].Z[i];
		}
		Neville_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
									  tPoly, pPoly, tMod, &p, &v);
		direction->Z[i] = p;
		velocity->Z[i] = v;
		sum += p*p;
	}
	if((sum > 0.5) && (sum < 1.5)) {
		/* adjust unit vectors back to unity */
		sum = 1.0/sqrt(sum);
		for(i=0; i < 3; ++i) {
			direction->Z[i] *= sum;
			velocity->Z[i] *= sum;
		}
	}
		

	return r;
}






int writeDifxSpacecraftArray(FILE *out, int nSpacecraft, DifxSpacecraft *ds)
{
	const int MaxLineLength = 2048;
	int n;
	int i, j, v;
	char value[MaxLineLength];
	const sixVector *V;
	const spacecraftTimeFrameOffset* STFO;
	const spacecraftAxisVectors* SAV;

	writeDifxLineInt(out, "NUM SPACECRAFT", nSpacecraft);
	n = 1;
	for(i = 0; i < nSpacecraft; ++i)
	{
		writeDifxLine1(out,       "SPACECRAFT %d NAME", i, ds[i].name);
		writeDifxLineBoolean1(out,"SPACECRAFT %d ISANT", i, ds[i].is_antenna);
		writeDifxLine1(out,       "SPACECRAFT %d SC_TIMETYPE", i, spacecraftTimeTypeNames[ds[i].spacecraft_time_type]);
		writeDifxLine1(out,       "SPACECRAFT %d POSITION_FRAME", i, sourceCoordinateFrameTypeNames[ds[i].position_coord_frame]);
		writeDifxLine1(out,       "SPACECRAFT %d POINTING_FRAME", i, sourceCoordinateFrameTypeNames[ds[i].pointing_coord_frame]);
		writeDifxLineInt1(out,    "SPACECRAFT %d GS_EXISTS", i, (ds[i].GS_exists) ? 1:0);
		writeDifxLineDouble1(out, "SPACECRAFT %d SC_REC_DELAY (s)",        i, "%.16e", ds[i].SC_recording_delay);
		writeDifxLineDouble1(out, "SPACECRAFT %d SC_COMM_REC_DELAY (s)",   i, "%.16e", ds[i].SC_Comm_Rec_to_Elec);
		writeDifxLineDouble1(out, "SPACECRAFT %d SC_REC_COMM_DELAY (s)",   i, "%.16e", ds[i].SC_Elec_to_Comm);
		/* writeDifxLineDouble1(out, "SPACECRAFT %d GS_REC_DELAY (s)",        i, "%.16e", ds[i].GS_recording_delay); */
		/* writeDifxLineDouble1(out, "SPACECRAFT %d GS_TRANS_DELAY (s)",      i, "%.16e", ds[i].GS_transmission_delay); */
		/* writeDifxLineDouble1(out, "SPACECRAFT %d GS_TRANS_SYNC_DELAY (s)", i, "%.16e", ds[i].GS_transmission_delay_sync); */
		/* writeDifxLineDouble1(out, "SPACECRAFT %d SC_ELEC_DELAY (s)",       i, "%.16e", ds[i].SC_elec_delay); */
		/* writeDifxLineDouble1(out, "SPACECRAFT %d GS_CLOCK_DELAY (s)",      i, "%.16e", ds[i].GS_clock_delay); */
		/* writeDifxLineDouble1(out, "SPACECRAFT %d GS_CLOCK_SYNC_DELAY (s)", i, "%.16e", ds[i].GS_clock_delay_sync); */
		n += 9;
		if(ds[i].GS_exists)
		{
			writeDifxLine1(out,       "SPACECRAFT %d GS_NAME", i, ds[i].GS_Name);
			writeDifxLine1(out,       "SPACECRAFT %d GS_CALCNAME", i, ds[i].GS_calcName);
			writeDifxLineInt1(out,    "SPACECRAFT %d GS_MJD_SYNC", i, ds[i].GS_mjd_sync);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_DAYFRAC_SYNC",         i, "%.16f", ds[i].GS_dayfraction_sync);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_CLOCK_FUDGE (s)",      i, "%.16e", ds[i].GS_clock_break_fudge_sec);
			writeDifxLine1(out,       "SPACECRAFT %d GS_MOUNT", i, antennaMountTypeNames[ds[i].GS_mount]);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_OFFSET (m)",  i, "%8.6f", ds[i].GS_offset[0]);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_OFFSET1 (m)", i, "%8.6f", ds[i].GS_offset[1]);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_OFFSET2 (m)", i, "%8.6f", ds[i].GS_offset[2]);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_X (m)", i, "%8.6f", ds[i].GS_X);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_Y (m)", i, "%8.6f", ds[i].GS_Y);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_Z (m)", i, "%8.6f", ds[i].GS_Z);
			writeDifxLineDouble1(out, "SPACECRAFT %d GS_CLOCK REFMJD", i, "%.16f", ds[i].GS_clockrefmjd);
			writeDifxLineInt1(out,    "SPACECRAFT %d GS_CLOCK ORDER", i, ds[i].GS_clockorder);
			writeDifxLine(out, "@ ***** GS clock poly coeff N", " has units microsec / sec^N ***** @");
			for(j=0;j<ds[i].GS_clockorder+1;++j)
			{
				writeDifxLineDouble2(out, "SPACECRAFT %d GS_CLOCK COEFF %d", i, j, "%18.16e", ds[i].GS_clockcoeff[j]);
			}
			n += 15 + ds[i].GS_clockorder+1;
		}
		/* write out position offset information */
		writeDifxLineInt1(out,    "SPACECRAFT %d SC_POSOFFSET REFMJD", i, ds[i].SC_pos_offset_refmjd);
		writeDifxLineDouble1(out, "SPACECRAFT %d SC_POSOFFSET REFFRAC", i, "%.16f", ds[i].SC_pos_offset_reffracDay);
		writeDifxLineInt1(out,    "SPACECRAFT %d SC_POSOFFSET ORDER", i, ds[i].SC_pos_offsetorder);
		writeDifxLine(out, "@ ***** SC position offset poly coeff N", " has units m / s^N ***** @");
		for(j = 0; j <= ds[i].SC_pos_offsetorder; ++j)
		{
			v = snprintf(value, MaxLineLength, "%24.16e %24.16e %24.16e", ds[i].SC_pos_offset[j].X, ds[i].SC_pos_offset[j].Y, ds[i].SC_pos_offset[j].Z);
			if(v >= MaxLineLength)
			{
				fprintf(stderr, "Error: Spacecraft %d SC_pos_offset %d is too long!\n", i, j);

				return -1;
			}
			writeDifxLine2(out, "SPACECRAFT %d SC_POSOFFSET %d", i, j, value);
		}
		n += 4 + ds[i].SC_pos_offsetorder+1;
		writeDifxLineInt1(out,    "SPACECRAFT %d CALC_OWN_RETARDATION", i, ds[i].calculate_own_retarded_position);
		++n;
		/* write out ephemeris information */
		writeDifxLineInt1(out, "SPACECRAFT %d ROWS", i, ds[i].nPoint);
		for(j = 0; j < ds[i].nPoint; ++j)
		{
			V = ds[i].pos + j;
			STFO = ds[i].TFrameOffset + j;
			SAV = ds[i].SCAxisVectors + j;
			v = snprintf(value, MaxLineLength,
						 "%5d %19.16f "
						 "%27.19Le %27.19Le %27.19Le "
						 "%27.19Le %27.19Le %27.19Le "
						 "%24.16e %24.16e "
						 "%24.16e %24.16e %24.16e "
						 "%24.16e %24.16e %24.16e "
						 "%24.16e %24.16e %24.16e", 
						 V->mjd, V->fracDay,
						 V->X, V->Y, V->Z,
						 V->dX, V->dY, V->dZ,
						 STFO->Delta_t, STFO->dtdtau,
						 SAV->X[0], SAV->X[1], SAV->X[2],
						 SAV->Y[0], SAV->Y[1], SAV->Y[2],
						 SAV->Z[0], SAV->Z[1], SAV->Z[2]
						 );
			if(v >= MaxLineLength)
			{
				fprintf(stderr, "Error: Spacecraft %d row %d is too long!\n", i, j);

				return -1;
			}
			writeDifxLine2(out, "SPACECRAFT %d ROW %d", i, j, value);
		}
		n += (1 + ds[i].nPoint);
	}

	return n;
}



/* routines for handling Russian SCF format data for spacecraft positions */
/* 2012 Feb 13	James M Anderson  --MPIfR  start adding SCF stuff in */



static int ymd2mjd(int yr, int mo, int day)
{
	int a;
	int y;
	int m;
	int mjd;
	a=(14-mo)/12;
	y=yr+4800-a;
	m=mo+12*a-3;
	mjd=day+(153*m+2)/5+365*y+y/4-y/100+y/400-32045-2400001;
	return mjd;
}
void get_next_Russian_scf_line(char * const line,
							   const int MAX_LEN,
							   FILE *fp)
{
	size_t last;
	line[0] = 0;
	fgets(line, MAX_LEN, fp);
	last = strlen(line);
	while(last>0) {
		--last;
		if((line[last] == '\n') || (line[last] == '\r')) {
			line[last] = 0;
		}
		else {
			break;
		}
	}
	while((line[0] == '\r') || (line[0] == '\n')) {
		memmove(line,line+1,MAX_LEN-1);
	}
	return;
}
int read_Russian_scf_file(const char * const filename,
						  const char * const spacecraftname,
						  const double MJD_start,
						  const double MJD_end,
						  const double MJD_delta,
						  const double ephemClockError,
						  DifxSpacecraft * const ds)
{
	static const double km2m = 1000.0;
	static const double ONE_SECOND_IN_DAYS = 1.0 / SEC_DAY_DBL;
	const int MAX_LINE_SIZE=2048;
	char line[MAX_LINE_SIZE];
	FILE* fp;
	int year, month, day, hour, minute;
	double second;
	double MJD;
	double frac_day;
	int MJDi;
	int check_flags = 0;
	double table_interval = 0.0;
	double table_MJD_start = 0.0;
	double table_MJD_end = 0.0;
	long table_start_pos;
	int count;
	double x,y,z;
	double dx, dy, dz;
	

	fp = fopen(filename, "r");
	if(fp == NULL) {
		fprintf(stderr, "Error: read_Russian_scf_file: cannot open RUSSCF file '%s'\n", filename);
		return -1;
	}
	/* Read the header information and check values */
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	if(strcmp(line,"META_START")) {
		fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has invalid header start\n", filename);
		goto read_Russian_scf_file_fail;
	}
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	while(((line[0]) && strcmp(line,"META_STOP"))) {
		if(!strncmp(line,"OBJECT_NAME	= ", 16)) {
			if(strncmp(line+16, spacecraftname, strlen(spacecraftname))) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has OBJECT_NAME '%s', but the requested spacecraftname was '%s'\n", filename, line+16, spacecraftname);
				goto read_Russian_scf_file_fail;
			}
			check_flags |= 0x1;
		}
		else if(!strncmp(line,"OBJECT_ID	 = ", 16)) check_flags |= 0x2;
		else if(!strncmp(line,"CENTER_NAME	 = ", 16)) {
			if(strcmp(line, "CENTER_NAME   = Earth Barycenter")) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has unknown CENTER_NAME '%s'\n", filename, line+16);
				goto read_Russian_scf_file_fail;
			}
			check_flags |= 0x4;
		}
		else if(!strncmp(line,"REF_FRAME	 = ", 16)) {
			if(strcmp(line, "REF_FRAME	   = EME2000")) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has unknown REF_FRAME '%s'\n", filename, line+16);
				goto read_Russian_scf_file_fail;
			}
			check_flags |= 0x8;
		}
		else if(!strncmp(line,"TIME_SYSTEM	 = ", 16)) {
			if(strcmp(line, "TIME_SYSTEM   = UTC")) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has unknown TIME_SYSTEM '%s'\n", filename, line+16);
				goto read_Russian_scf_file_fail;
			}
			check_flags |= 0x10;
		}
		else if(!strncmp(line,"START_TIME	 = ", 16)) {
			if(sscanf(line+16,"%d-%d-%dT%d:%d:%lf",
					 &year, &month, &day,
					 &hour, &minute, &second) != 6) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has bad START_TIME '%s'\n", filename, line+16);
				goto read_Russian_scf_file_fail;
			}
			MJDi = ymd2mjd(year, month, day);
			frac_day = (hour + (minute + (second - ephemClockError)/60.0)/60.0)/24.0;
			MJD = MJDi + frac_day;
			/* allow 1 second fudge factor, \sim 1.1574E-5 days */
			if(MJD > MJD_start + ONE_SECOND_IN_DAYS) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' START_TIME '%s', MJD %.6f is after required start MJD %.6f\n", filename, line+16, MJD, MJD_start);
				goto read_Russian_scf_file_fail;
			}
			table_MJD_start = MJD;
			check_flags |= 0x20;
		}
		else if(!strncmp(line,"STOP_TIME	 = ", 16)) {
			if(sscanf(line+16,"%d-%d-%dT%d:%d:%lf",
					 &year, &month, &day,
					 &hour, &minute, &second) != 6) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has bad STOP_TIME '%s'\n", filename, line+16);
				goto read_Russian_scf_file_fail;
			}
			MJDi = ymd2mjd(year, month, day);
			frac_day = (hour + (minute + (second - ephemClockError)/60.0)/60.0)/24.0;
			MJD = MJDi + frac_day;
			/* allow 1 second fudge factor, \sim 1.1574E-5 days */
			if(MJD < MJD_end - ONE_SECOND_IN_DAYS) {
				fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' STOP_TIME '%s', MJD %.6f is before required end MJD %.6f\n", filename, line+16, MJD, MJD_end);
				goto read_Russian_scf_file_fail;
			}
			table_MJD_end = MJD;
			check_flags |= 0x40;
		}
		else {
			fprintf(stderr, "Warning: read_Russian_scf_file: RUSSCF file '%s' has unknown header keyword=value pair '%s'", filename, line);
		}
		get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	}
	if(check_flags != 0x7F) {
		fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' did not have complete header information\n", filename);
		goto read_Russian_scf_file_fail;
	}
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	if(line[0] != 0) {
		fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' missing data gap\n", filename);
		goto read_Russian_scf_file_fail;
	}
	table_start_pos = ftell(fp);
	if((table_MJD_start > MJD_start) || (table_MJD_end < MJD_end))
	{
		fprintf(stderr, "Warning: read_Russian_scf_file: RUSSCF file '%s' has start/stop dates of %.6f/%.6f but requested date range of %.6f/%.6f falls outside this range.\n", filename, table_MJD_start, table_MJD_end, MJD_start, MJD_end);
	}

	/* now check how many entries we will actually get */
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	count = 0;
	while(line[0] != 0) {
		if(sscanf(line,"%d-%d-%dT%d:%d:%lf",
				 &year, &month, &day,
				 &hour, &minute, &second) != 6) {
			fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has bad data date '%s'\n", filename, line);
			goto read_Russian_scf_file_fail;
		}
		MJDi = ymd2mjd(year, month, day);
		frac_day = (hour + (minute + (second - ephemClockError)/60.0)/60.0)/24.0;
		MJD = MJDi + frac_day;
		if(table_interval == 0.0) {
			if(fabs(MJD-table_MJD_start) < 1E-10) {
				/* table start, pass */
			}
			else {
				table_interval = MJD-table_MJD_start;
				if((table_interval - MJD_delta)/MJD_delta > 1E-5) {
					fprintf(stderr, "Warning: read_Russian_scf_file: RUSSCF file '%s' tabulated interval %E days, wherease the requested tabulated interval was %E days (%E seconds)\n", filename, table_interval, MJD_delta, MJD_delta*SECONDS_PER_DAY);
				}
			}
		}
		if((MJD >= MJD_start - ONE_SECOND_IN_DAYS) && (MJD <= MJD_end + ONE_SECOND_IN_DAYS)) {
			count++;
		}
		get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	}
	if(count < 1) {
		fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s', found 0 data values in MJD range %.6f--%.6f\n", filename, MJD_start, MJD_end);
		goto read_Russian_scf_file_fail;
	}
	/* allocate space for the entries in the DifxSpacecraft struct */
	ds->nPoint = count;
	free(ds->pos);
	ds->pos = (sixVector *)calloc(count, sizeof(sixVector));
	if(!ds->pos) {
		fprintf(stderr, "Error: read_Russian_scf_file: cannot malloc memory for %d sixVectors\n", count);
		goto read_Russian_scf_file_fail;
	}
	/* now go back and read in the data-points */
	clearerr(fp);
	if(fseek(fp, table_start_pos, SEEK_SET)<0) {
		fprintf(stderr, "Error: read_Russian_scf_file: count not seek to start of data section in RUSSCF file '%s'\n", filename);
		goto read_Russian_scf_file_fail;
	}
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	count = 0;
	while(line[0] != 0) {
		if(sscanf(line,"%d-%d-%dT%d:%d:%lf %lf %lf %lf %lf %lf %lf",
				 &year, &month, &day,
				 &hour, &minute, &second,
				 &x, &y, &z,
				 &dx, &dy, &dz) != 12) {
			fprintf(stderr, "Error: read_Russian_scf_file: RUSSCF file '%s' has bad data '%s'\n", filename, line);
			goto read_Russian_scf_file_fail;
		}
		x *= km2m;
		y *= km2m;
		z *= km2m;
		dx *= km2m;
		dy *= km2m;
		dz *= km2m;
		MJDi = ymd2mjd(year, month, day);
		frac_day = (hour + (minute + (second - ephemClockError)/60.0)/60.0)/24.0;
		if(frac_day < 0.0) {
			frac_day += 1.0;
			--MJDi;
		}
		else if(frac_day >= 1.0) {
			frac_day -= 1.0;
			++MJDi;
		}
		MJD = MJDi + frac_day;
		if((MJD >= MJD_start - ONE_SECOND_IN_DAYS) && (MJD <= MJD_end + ONE_SECOND_IN_DAYS)) {
			ds->pos[count].mjd = MJDi;
			ds->pos[count].fracDay = frac_day;
			ds->pos[count].X = x;
			ds->pos[count].Y = y;
			ds->pos[count].Z = z;
			ds->pos[count].dX = dx;
			ds->pos[count].dY = dy;
			ds->pos[count].dZ = dz;
			count++;
		}
		get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	}
		

	fclose(fp);
	return 0;
read_Russian_scf_file_fail:
	fclose(fp);
	fprintf(stderr, "Error: read_Russian_scf_file: failure while reading RUSSCF file '%s'\n", filename);
	return -1;
}

int read_Russian_scf_axes_file(const char * const filename,
							   const double MJD_start,
							   const double MJD_end,
							   const double ephemClockError,
							   DifxSpacecraft * const ds)
{
	/* converts RUSSCF axes data file to DiFX style axes information */
	/* Note that the RUSSCF convention (RadioAstron) has
	   X axis points toward antenna (source) direction
	   Y axis is along the Solar panel extension
	   Z axis is perpendicular
	   The DiFX convention has
	   Z axis is the pointing direction (source) of the telescope
	   X axis is the referece direction for polarization
	   Y axis makes a right-handed coordiante system
	*/
	const int MAX_LINE_SIZE=2048;
	char line[MAX_LINE_SIZE];
	FILE* fp;
	int year, month, day, hour, minute;
	double second;
	double MJD;
	double frac_day;
	int MJDi;
	int last_MJDi;
	double last_frac_day;
	int count;
	int* restrict mjd_array = NULL;
	double* restrict mjd_frac_array = NULL;
	spacecraftAxisVectors* restrict sav_array = NULL;
	double X_ra, X_dec, Y_ra, Y_dec, Z_ra, Z_dec;
	double cra, sra, cdec, sdec;
	double x, y, z;
	static const char UTC_OFFSET_str[] = "UTC_OFFSET";
	size_t UTC_OFFSET_str_SIZE = strlen(UTC_OFFSET_str);
	static const char UTC_OFFSET_comment_str[] = "# Time is UTC";
	size_t UTC_OFFSET_comment_str_SIZE = strlen(UTC_OFFSET_comment_str);
	double UTC_OFFSET = 3.0 / 24.0; /* Spektr-R runs on UTC +3 hours */
	int UTC_OFFSET_found = 0;
	int UTC_OFFSET_comment_found = 0;
	int RUSSCF_format_code = 0;
	const double SEARCH_WINDOW = 1.0; /* suck in axis information within +-
										 1.0 days of the required start/stop
										 times */

	if((ds->nPoint <= 0) || (ds->SCAxisVectors == NULL)) {
		fprintf(stderr, "Error: read_Russian_scf_axes_file: programmer error: spacecraft ephemeris information must be read in first!\n");
		return -2;
	}

	fp = fopen(filename, "r");
	if(fp == NULL) {
		fprintf(stderr, "Error: read_Russian_scf_axes_file: cannot open RUSSCF axes file '%s'\n", filename);
		return -1;
	}
	/* check how many entries we will actually get */
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	last_MJDi = 0;
	last_frac_day = -1E300;
	count = 0;
	while(line[0] != 0) {
		if(line[0] == '#') {
			/* comment */
			/* 2014 Feb 07	James M Anderson  The new RUSSCF format for the
			   orientation information has dropped the explicit
			   UTC offset statement, and now writes the UTC
			   offset inside of a comment.	Grrrrr.	 */
			if(strncmp(line, UTC_OFFSET_comment_str, UTC_OFFSET_comment_str_SIZE) == 0) {
				UTC_OFFSET = 0.0;
				UTC_OFFSET_found++;
				UTC_OFFSET_comment_found++;
				RUSSCF_format_code = 1;
			}
		}
		else if(strncmp(line, UTC_OFFSET_str, UTC_OFFSET_str_SIZE) == 0) {
			double offset;
			if(sscanf(line+UTC_OFFSET_str_SIZE, "%lf", &offset) != 1) {
				fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s' has bad UTC_OFFSET line '%s'\n", filename, line);
				goto read_Russian_scf_axes_file_fail;
			}
			UTC_OFFSET = offset / 24.0;
			UTC_OFFSET_found++;
		}
		else {
			if(RUSSCF_format_code == 0) {
				if(sscanf(line,"%d.%d.%d %d:%d:%lf",
						 &day, &month, &year,
						 &hour, &minute, &second) != 6) {
					fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s', format %d, has bad data date in line '%s'\n", filename, RUSSCF_format_code, line);
					goto read_Russian_scf_axes_file_fail;
				}
			}
			else if(RUSSCF_format_code == 1) {
				if(sscanf(line,"%*s %d-%d-%d %d:%d:%lf",
						 &year, &month, &day,
						 &hour, &minute, &second) != 6) {
					fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s', format %d, has bad data date in line '%s'\n", filename, RUSSCF_format_code, line);
					goto read_Russian_scf_axes_file_fail;
				}
			}
			else {
				fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file programmer error, unknown format %d\n", RUSSCF_format_code);
				goto read_Russian_scf_axes_file_fail;
			}
			MJDi = ymd2mjd(year, month, day);
			frac_day = (hour + (minute + (second - ephemClockError)/60.0)/60.0)/24.0;
			frac_day -= UTC_OFFSET;
			if(frac_day >= 1.0) {
				frac_day -= 1.0;
				MJDi += 1;
			}
			else if(frac_day < 0.0) {
				frac_day += 1.0;
				MJDi -= 1;
			}
			if((MJDi != last_MJDi) || (frac_day != last_frac_day)) {
				MJD = MJDi + frac_day;
				if((MJD >= MJD_start - SEARCH_WINDOW) && (MJD <= MJD_end + SEARCH_WINDOW)) {
					count++;
				}
			}
			else {
				/* duplicate time entry --- discard */
			}
			last_MJDi = MJDi;
			last_frac_day = frac_day;
		}
		get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	}
	if(count < 1) {
		fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s', found 0 data values in MJD range %.6f--%.6f\n", filename, MJD_start, MJD_end);
		goto read_Russian_scf_axes_file_fail;
	}
	if(count < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER) {
		fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s', found only %d data values in MJD range %.6f--%.6f, but need %d for interpolation\n", filename, count, MJD_start, MJD_end, DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER);
		goto read_Russian_scf_axes_file_fail;
	}
	if(UTC_OFFSET_found == 0) {
		fprintf(stderr, "Warning: read_Russian_scf_axes_file: RUSSCF axes file '%s', found no lines starting with '%s'.	 Is the default value of %.2f correct?\n", filename, UTC_OFFSET_str, UTC_OFFSET * 24.0);
	}
	else if(UTC_OFFSET_found > 1) {
		fprintf(stderr, "Warning: read_Russian_scf_axes_file: RUSSCF axes file '%s', found multiple (%d) lines starting with '%s' or '%s'.	Are the multiple clock offset commands correct?\n", filename, UTC_OFFSET_found, UTC_OFFSET_str, UTC_OFFSET_comment_str);
	}
	else if(UTC_OFFSET_comment_found > 1) {
		fprintf(stderr, "Warning: read_Russian_scf_axes_file: RUSSCF axes file '%s', found multiple (%d) lines starting with '%s'.	Are the multiple clock offset commands correct?\n", filename, UTC_OFFSET_found, UTC_OFFSET_comment_str);
	}
	UTC_OFFSET_found = 0;
	UTC_OFFSET_comment_found = 0;
	RUSSCF_format_code = 0;
	/* allocate space for the entries in the DifxSpacecraft struct */
	mjd_array = (int* restrict)calloc(count, sizeof(int));
	mjd_frac_array = (double* restrict)calloc(count, sizeof(double));
	sav_array = (spacecraftAxisVectors* restrict)calloc(count, sizeof(spacecraftAxisVectors));
	if((mjd_array==NULL)||(mjd_frac_array==NULL)||(sav_array==NULL)) {
		fprintf(stderr, "Error: read_Russian_scf_axes_file: cannot malloc memory for %d spacecraftAxisVectors plus time\n", count);
		goto read_Russian_scf_axes_file_fail;
	}
	/* now go back and read in the data-points */
	rewind(fp);
	get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	last_MJDi = 0;
	last_frac_day = -1E300;
	count = 0;
	while(line[0] != 0) {
		if(line[0] == '#') {
			/* comment */
			/* 2014 Feb 07	James M Anderson  The new RUSSCF format for the
			   orientation information has dropped the explicit
			   UTC offset statement, and now writes the UTC
			   offset inside of a comment.	Grrrrr.	 */
			if(strncmp(line, UTC_OFFSET_comment_str, UTC_OFFSET_comment_str_SIZE) == 0) {
				UTC_OFFSET = 0.0;
				UTC_OFFSET_found++;
				UTC_OFFSET_comment_found++;
				RUSSCF_format_code = 1;
			}
		}
		else if(strncmp(line, UTC_OFFSET_str, UTC_OFFSET_str_SIZE) == 0) {
			double offset;
			if(sscanf(line+UTC_OFFSET_str_SIZE, "%lf", &offset) != 1) {
				fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s' has bad UTC_OFFSET line '%s'\n", filename, line);
				goto read_Russian_scf_axes_file_fail;
			}
			UTC_OFFSET = offset * 3600.0 / SECONDS_PER_DAY;
			UTC_OFFSET_found++;
		}
		else {
			if(RUSSCF_format_code == 0) {
				if(sscanf(line,"%d.%d.%d %d:%d:%lf %lf %lf %lf %lf %lf %lf",
						 &day, &month, &year,
						 &hour, &minute, &second,
						 &X_ra, &X_dec,
						 &Y_ra, &Y_dec,
						 &Z_ra, &Z_dec) != 12) {
					fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s', format %d, has bad data in line '%s'\n", filename, RUSSCF_format_code, line);
					goto read_Russian_scf_axes_file_fail;
				}
			}
			else if(RUSSCF_format_code == 1) {
				if(sscanf(line,"%*s %d-%d-%d %d:%d:%lf %lf %lf %lf %lf %lf %lf",
						 &year, &month, &day,
						 &hour, &minute, &second,
						 &X_ra, &X_dec,
						 &Y_ra, &Y_dec,
						 &Z_ra, &Z_dec) != 12) {
					fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file '%s', format %d, has bad data in line '%s'\n", filename, RUSSCF_format_code, line);
					goto read_Russian_scf_axes_file_fail;
				}
			}
			else {
				fprintf(stderr, "Error: read_Russian_scf_axes_file: RUSSCF axes file programmer error, unknown format %d\n", RUSSCF_format_code);
				goto read_Russian_scf_axes_file_fail;
			}
			MJDi = ymd2mjd(year, month, day);
			frac_day = (hour + (minute + (second - ephemClockError)/60.0)/60.0)/24.0;
			frac_day -= UTC_OFFSET;
			if(frac_day >= 1.0) {
				frac_day -= 1.0;
				MJDi += 1;
			}
			else if(frac_day < 0.0) {
				frac_day += 1.0;
				MJDi -= 1;
			}
			if((MJDi != last_MJDi) || (frac_day != last_frac_day)) {
				MJD = MJDi + frac_day;
				if((MJD >= MJD_start - SEARCH_WINDOW) && (MJD <= MJD_end + SEARCH_WINDOW)) {
					mjd_array[count] = MJDi;
					mjd_frac_array[count] = frac_day;
					/* X */
					cra = cos(Y_ra*M_PI/180.0);
					sra = sin(Y_ra*M_PI/180.0);
					cdec = cos(Y_dec*M_PI/180.0);
					sdec = sin(Y_dec*M_PI/180.0);
					x = cra*cdec;
					y = sra*cdec;
					z = sdec;
					sav_array[count].X[0] = x;
					sav_array[count].X[1] = y;
					sav_array[count].X[2] = z;
					/* Y */
					cra = cos(Z_ra*M_PI/180.0);
					sra = sin(Z_ra*M_PI/180.0);
					cdec = cos(Z_dec*M_PI/180.0);
					sdec = sin(Z_dec*M_PI/180.0);
					x = cra*cdec;
					y = sra*cdec;
					z = sdec;
					sav_array[count].Y[0] = x;
					sav_array[count].Y[1] = y;
					sav_array[count].Y[2] = z;
					/* Z */
					cra = cos(X_ra*M_PI/180.0);
					sra = sin(X_ra*M_PI/180.0);
					cdec = cos(X_dec*M_PI/180.0);
					sdec = sin(X_dec*M_PI/180.0);
					x = cra*cdec;
					y = sra*cdec;
					z = sdec;
					sav_array[count].Z[0] = x;
					sav_array[count].Z[1] = y;
					sav_array[count].Z[2] = z;
					count++;
				}
			}
			else {
				/* duplicate time entry --- discard */
			}
			last_MJDi = MJDi;
			last_frac_day = frac_day;			 
		}
		get_next_Russian_scf_line(line, MAX_LINE_SIZE, fp);
	}
	fclose(fp);

	/* Now run through to interpolate onto the ephemeris time grid */
	{
		int nRow;
		const sixVector * restrict pos;
		spacecraftAxisVectors* restrict interp_axis_info;
		double t0, t1, tMod;
		double tPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
		double pPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
		int r, r0, rstart;
		int i, c;
		double p,v;
		double sum;
	
		nRow = ds->nPoint;
		pos = ds->pos;
		interp_axis_info = ds->SCAxisVectors;
	
		tMod = pos[0].mjd + pos[0].fracDay;
	
		/* find first interpolation point */
		t0 = 0.0;
		t1 = mjd_array[0] + mjd_frac_array[0];
		for(r = 1; r < count; r++)
		{
			t0 = t1;
			t1 = mjd_array[r] + mjd_frac_array[r];
			if(t0 <= tMod && tMod < t1)
			{
				break;
			}
		}
		if(r == count)
		{
			if(tMod == t1) {
				r--;
			}
			else {
				fprintf(stderr, "Error: read_Russian_scf_axes_file: start of ephemeris data time outside of axis direction time window\n");
				fprintf(stderr, "Ephem start=%.10f	axis data start=%.10f\n", tMod, mjd_array[0] + mjd_frac_array[0]);
				goto read_Russian_scf_axes_file_fail2;
			}
		}
		rstart = r;
		for(c=0; c < nRow; c++) {
			tMod = pos[c].mjd + pos[c].fracDay;
			r = rstart-1;
			do {
				r++;
				t0 = mjd_array[r] + mjd_frac_array[r];
			} while((tMod > t0)&&(r < count));
			if(r == count) {
				if(tMod > t0) {
					fprintf(stderr, "Error: read_Russian_scf_axes_file: ephemeris data time outside of axis direction time window\n");
					fprintf(stderr, "Ephem time=%.10f  axis data end=%.10f\n", tMod, t0);
					goto read_Russian_scf_axes_file_fail2;
				}
				r--;
			}
			rstart = r;
			r -= DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER/2;
			if(r<0) r=0;
			if(r+DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER > count) {
				r = count - DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER;
			}
			/* Note that the velocity will come out with inverse time units
			   corresponding to the time array units.
			*/
			tMod = ((pos[c].mjd-mjd_array[r]) + (pos[c].fracDay-mjd_frac_array[r])) * SECONDS_PER_DAY;
			for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
				tPoly[r0] = ((pos[c].mjd-mjd_array[r+r0])
							+ (pos[c].fracDay-mjd_frac_array[r+r0])) * SECONDS_PER_DAY;
			}
			/* X axis */
			sum = 0.0;
			for(i=0; i < 3; i++) {
				for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
					pPoly[r0] = sav_array[r+r0].X[i];
				}
				Neville_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
											  tPoly, pPoly, tMod, &p, &v);
				interp_axis_info[c].X[i] = p;
				sum += p*p;
			}
			if((sum > 0.5) && (sum < 1.5)) {
				/* adjust unit vectors back to unity */
				sum = 1.0/sqrt(sum);
				for(i=0; i < 3; i++) {
					interp_axis_info[c].X[i] *= sum;
				}
			}
			/* Y axis */
			sum = 0.0;
			for(i=0; i < 3; i++) {
				for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
					pPoly[r0] = sav_array[r+r0].Y[i];
				}
				Neville_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
											  tPoly, pPoly, tMod, &p, &v);
				interp_axis_info[c].Y[i] = p;
				sum += p*p;
			}
			if((sum > 0.5) && (sum < 1.5)) {
				/* adjust unit vectors back to unity */
				sum = 1.0/sqrt(sum);
				for(i=0; i < 3; i++) {
					interp_axis_info[c].Y[i] *= sum;
				}
			}
			/* Z axis */
			sum = 0.0;
			for(i=0; i < 3; i++) {
				for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
					pPoly[r0] = sav_array[r+r0].Z[i];
				}
				Neville_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
											  tPoly, pPoly, tMod, &p, &v);
				interp_axis_info[c].Z[i] = p;
				sum += p*p;
			}
			if((sum > 0.5) && (sum < 1.5)) {
				/* adjust unit vectors back to unity */
				sum = 1.0/sqrt(sum);
				for(i=0; i < 3; i++) {
					interp_axis_info[c].Z[i] *= sum;
				}
			}
		}
	}
	

	free(mjd_array);
	free(mjd_frac_array);
	free(sav_array);
	return 0;
read_Russian_scf_axes_file_fail:
	fclose(fp);
read_Russian_scf_axes_file_fail2:
	free(mjd_array);
	free(mjd_frac_array);
	free(sav_array);
	fprintf(stderr, "Error: read_Russian_scf_axes_file: failure while reading RUSSCF axes file '%s'\n", filename);
	return -1;
}













/******************************************************************************/
/* Spacecraft Time Frame Modeling *********************************************/
/******************************************************************************/


int DiFX_model_spacecraft_time_frame_delay_rate(const DifxSpacecraft* const spacecraft, 
												const int MJD_TT,
												const double frac_TT,
												double* const rate)
{
	/*
	  2012 Jun 03  James M Anderson	 --MPIfR  start

	  See
	  http://en.wikipedia.org/wiki/Terrestrial_Time
	  http://maia.usno.navy.mil/NSFA/NSFA_cbe.html#GME2009
	  http://en.wikipedia.org/wiki/Gravitational_redshift
	  http://en.wikipedia.org/wiki/Gravitational_time_dilation

	  TCG is Geocentric Coordinate Time, the time frame outside of the Earth's
	  graviational potentail.
	  http://en.wikipedia.org/wiki/Geocentric_Coordinate_Time

	  TT is Terrestrial Time, the time frame of the standard Earth observing
	  frame, in the standard Earth's potential.
	  http://en.wikipedia.org/wiki/Terrestrial_Time

	  T_{SC} is the spacecraft time

	  By definition by the IAU,
	  TT = (1 - L_G) TCG + E_{TT}
	  where E_{TT} is the reference epoch for TT time, and
	  L_G \equiv 6.969290134 × 10^{-10}

	  Follow the argument of http://en.wikipedia.org/wiki/Gravitational_redshift
	  I have
	  T_{SC} = TCG \sqrt{1-\frac{2GM_{Earth}}{c^2 R_{SC}} - \frac{v_{SC}^2}{c^2}}
	  + E_{SC}
	  wher E_{SC} is the reference epoch for the spacecraft time,
	  G is the gravitational constant, M_{Earth} is the mass of the Earth,
	  c is the speed of light, R is the Schwarzschild coordinate
	  (similar, but not exactly, the radial distance from the center of the
	  Earth), and v is the magnitude of the velocity of the spacecraft
	  in the initial frame defined by the center of the Earth.
	  Note that this equation assumes that the Earth is spherical and non-
	  rotating.

	  For higher precision, the Earth's quadrupole moment should be
	  taken into account.  The gravitational potential is then
	  \frac{2GM_{Earth}}{c^2 R_{SC}} \left[1
	  - \frac{J_2 a_1^2}{R^2}\left(\frac{3\cos^{2}\theta
	  -1}{2}\right)\right],
	  where J_2 is the Earth's quadrupole moment coefficient, a_1
	  is the Earth's equatorial radius, and \theta is the geocentric
	  colatitute (angle measured from North pole).	Note that
	  \cos^{2}\theta = \frac{z^2}{r^2}, where z is the z Cartesian component
	  and r is the radial distance.

	  For the TT frame, I have
	  GM_{Earth} = 3.986004415 x 10^{14} m^{3}s^{-2} [TT-compatible]

	  In the code below, the Cartesian distance r is calculated instead of
	  the Schwarzschild coordinate R.  This should be corrected at some
	  point.

	  These equations relating TT, TCGm abd T_{SC} can be rewritten
	  to give T_{SC} in terms of TT and the reference epochs.  However, what
	  is needed for correlation of spacecraft--ground baselines is the
	  *difference* (delay) in time between the two frames.	Since
	  TT and T_{SC} run at nearly the same rate (the Earth is not a black hole
	  and the spacecraft orbit is large), calculating T_{SC}(TT) - TT
	  from the equations above directly
	  will lead to large numerical errors, as the spacecraft time frame
	  has a changing rate with respect to TT, but is very nearly the same.
	  Instead, I want to just get the relative time rate between the two
	  frames, minus one.

	  So, \frac{\partial TCG}{\partial\tau} =
	  \frac{1}{1-L_G} \frac{\partial TT}{\partial\tau}, and
	  \frac{\partial TCG}{\partial\tau} =
	  A \frac{\partial T_{SC}}{\partial\tau}, and
	
	  where A \equiv \sqrt{1-\frac{2GM_{Earth}}{c^2 R_{SC}} - \frac{v_{SC}^2}{c^2}},
	  or is quadrupole moment corrected equivalent.
	  Then the relative clock rate of T_{SC} from TT, minus 1, is
	  rate_{differential} = \frac{A}{1-L_G} -1.

	  Now, because both the TT and T_{SC} frames are in the weak gravitational
	  field regime, A is approximately 1, and L_G is approximately 0.
	  Rewrite the rate_{differential} and take a Taylor series expansion.
	  Let X \equiv -\frac{2GM_{Earth}}{c^2 R_{SC}} - \frac{v_{SC}^2}{c^2},
	  so that the Taylor series expansion of A is
	  A = 1
	  + \frac{1}{2} X
	  - \frac{1}{2\cdot 4} X^2
	  + \frac{1\cdot 3}{2\cdot 4\cdot 6} X^3
	  - \frac{1\cdot 3\cdot 5}{2\cdot 4\cdot 6\cdot 8} X^4
	  + \ldots.
	  The rewritten form of the rate_{differential} is
	  rate_{differential} = \frac{(A-1) + L_G A + L_G^2}{1-L_G^2}.
	  The first term in the numerator can be seen to be small, for small X,
	  from the Taylor series expansion of A.  L_G is known to be small.
	  The denominator is \sim 1, so the whole rate_{differential} is small.

	  DiFX wants to have the negative of this, so that integrating up the rate
	  from the sync time to time t (in the TT frame), the total frame offset
	  (offset) will be such that the spacecraft clock reads time t - offset.
	*/

	/* constants partially from http://maia.usno.navy.mil/NSFA/NSFA_cbe.html */
	
	static const double GM_Earth = 3.986004415E14;	/* m^{3}s^{-2} */
	static const double L_G		 = 6.969290134E-10; /* unitless	   */
	static const double c		 = 299792458.0;		/* m s^{-1}	   */
	static const double J_2		 = 1.0826359E-3;	/* unitless	   */
	static const double a_1		 = 6.3781366E6;		/* m		   */
	
	double pos[3];
	double vel[3];
	double R;
	double v2;
	double quadrupole;
	double X;
	double B;
	int retcode = 0;
	

	if((spacecraft->is_antenna)) {
		nineVector sc_pos;
		int return_code = evaluateDifxSpacecraftAntenna(spacecraft,
														MJD_TT,
														frac_TT,
													   &sc_pos);
		if(return_code < 0) {
			retcode = -3;
			return retcode;
		}
		pos[0] = sc_pos.X;
		pos[1] = sc_pos.Y;
		pos[2] = sc_pos.Z;
		vel[0] = sc_pos.dX;
		vel[1] = sc_pos.dY;
		vel[2] = sc_pos.dZ;
	}
	else {
		sixVector sc_pos;
		int return_code = evaluateDifxSpacecraftSource(spacecraft,
													   MJD_TT,
													   frac_TT,
													  &sc_pos);
		if(return_code < 0) {
			retcode = -3;
			return retcode;
		}
		pos[0] = sc_pos.X;
		pos[1] = sc_pos.Y;
		pos[2] = sc_pos.Z;
		vel[0] = sc_pos.dX * SECONDS_PER_DAY;
		vel[1] = sc_pos.dY * SECONDS_PER_DAY;
		vel[2] = sc_pos.dZ * SECONDS_PER_DAY;
	}

	R  = pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2];
	v2 = vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2];
	if(R <= 0.0) {
		retcode = +1;  /* center of Earth has no gravitational potential */
		*rate = 0.0;
		return retcode;
	}
	else if(R < 6.384E6) {
		retcode = -1;  /* Inside Earth, potential is wrong */
		*rate = 0.0;
		return retcode;
	}
	R = sqrt(R);
	quadrupole = J_2*a_1*a_1/(R*R)*(3.0*pos[2]*pos[2]/(R*R)-1.0)*0.5;
	X = -(2.0*GM_Earth/R * (1.0 - quadrupole) + v2) / (c*c);
	if(X < -1E-5) {
		retcode = -2; /* Taylor series expansion does not have enough terms
						 to give high accuracy
					  */
	}
	B = ((((-0.0390625)*X + 0.0625)*X - 0.125)*X + 0.5)*X;
	*rate = -((B + L_G*(1.0+B) + L_G*L_G)/(1.0 - L_G*L_G));
	/*fprintf(stderr, "SC TT=%15E R=%15E v2=%15E X=%15E B=%15E rate=%15E\n", TT, R, v2, X, B, *rate);*/
	return retcode;
}







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
											)
{
	/* This function calculates the time frame delay between the spacecraft
	   time frame and TT, as a function of input TT time. */
	/* if the
	   TT time at which the wavefront reaches the telescope is t, then the
	   spacecraft clock reads t - *interpolated_timeoffset.
	*/
	/* See, e.g. Numerical Recipes function qromb */
	const int ITER_MAX = 20;
	const int ITER_SOL = 5;
	const int ITER_SOL_OFFSET = ITER_SOL-1;
	double delta_time;						  /* in seconds */
	double ss = 0.0, dss = 0.0;
	double s[ITER_MAX], h[ITER_MAX];
	int j;
	double hh = 1.0;
	double trapzd_sum = 0.0;
	int retcode = 0;
	int check;

	*delay = 0.0;
	*delta_delay = 1E100;
	*t1_rate = 0.0;

	delta_time = ((mjd1-mjd0) + (frac1-frac0)) * SECONDS_PER_DAY;
	if(delta_time == 0) {
		check = DiFX_model_spacecraft_time_frame_delay_rate(spacecraft,
															mjd1, frac1,
															t1_rate);
		if(check < 0) {
			return -1;
		}
		else if(check > 0) {
			retcode = +1;
		}
		*delta_delay = 0.0;
		return retcode;
	}
	for(j=0; j < ITER_MAX; j++, hh *= 0.25) {
		h[j] = hh;
		{
			double tnm, sum, del;
			int mjd_eval;
			double frac_eval;
			double frac_del;
			int mjd_offset;
			int it, k;
			if(j==0) {
				double res0, res1;
				check = DiFX_model_spacecraft_time_frame_delay_rate(spacecraft,
																	mjd0, frac0,
																   &res0);
				if(check < 0) {
					return -1;
				}
				else if(check > 0) {
					retcode = +1;
				}
				check = DiFX_model_spacecraft_time_frame_delay_rate(spacecraft,
																	mjd1, frac1,
																   &res1);
				if(check < 0) {
					return -1;
				}
				else if(check > 0) {
					retcode = +1;
				}
				*t1_rate = res1;
				trapzd_sum = 0.5 *delta_time*(res0+res1);
			}
			else {
				it = 1 << (j-1);
				tnm = it;
				del = delta_time/tnm;
				frac_del = del / SECONDS_PER_DAY;
				mjd_eval = mjd0;
				frac_eval = frac0 + 0.5 * frac_del;
				mjd_offset = (int)(floor(frac_eval));
				mjd_eval += mjd_offset;
				frac_eval -= mjd_offset;
				for(sum=0.0, k=0; k < it; k++) {
					double res;
					check = DiFX_model_spacecraft_time_frame_delay_rate(spacecraft,
																		mjd_eval, frac_eval,
																	   &res);
					if(check < 0) {
						return -1;
					}
					else if(check > 0) {
						retcode = +1;
					}
					sum += res;
					
					frac_eval += frac_del;
					mjd_offset = (int)(floor(frac_eval));
					mjd_eval += mjd_offset;
					frac_eval -= mjd_offset;
				}
				trapzd_sum = 0.5 * (trapzd_sum + del*sum);
			}
		}
		s[j] = trapzd_sum;
		if(j >= ITER_SOL_OFFSET) {
			int i, m, ns;
			double den, ho, hp, w;
			double c[ITER_SOL];
			double d[ITER_SOL];
			ns = ITER_SOL_OFFSET;
			for(i=0; i < ITER_SOL; i++) {
				c[i] = s[j-ITER_SOL_OFFSET+i];
				d[i] = s[j-ITER_SOL_OFFSET+i];
			}
			ss = s[j-ITER_SOL_OFFSET+(ns--)];
			for(m=1; m < ITER_SOL; m++) {
				for(i=0; i < ITER_SOL-m; i++) {
					ho = h[j-ITER_SOL_OFFSET+i];
					hp = h[j-ITER_SOL_OFFSET+i+m];
					w = c[i+1]-d[i];
					if((den=ho-hp) == 0.0) {
						retcode = -9;
						return retcode;
					}
					den = w/den;
					d[i] = hp*den;
					c[i] = ho*den;
				}
				ss += (dss = (2*ns+2 < (ITER_SOL-m) ? c[ns+1] : d[ns--]));
			}
			*delay = ss;
			*delta_delay = dss;
			if((fabs(dss) < fractional_error*fabs(ss))
			  || (fabs(dss) < absolute_error)) {
				return retcode;
			}
		}
	}
	retcode = -10; /* failure to converge */
	return retcode;
}


int evaluateDifxSpacecraftAntennaTimeFrameOffset(const DifxSpacecraft *sc,
												 int mjd,
												 double fracMjd,
												 spacecraftTimeFrameOffset *interpolated_timeoffset)
{
	/* input mjd and fracMjd are in TT time frame. */
	/* The output *interpolated_timeoffset is in units of seconds, and if the
	   TT time at which the wavefront reaches the telescope is t, then the
	   spacecraft clock reads t - *interpolated_timeoffset.
	*/
	int nRow;
	const sixVector * restrict pos;
	const spacecraftTimeFrameOffset * restrict raw_timeoffset;
	double t0, t1, tMod;
	double tPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	double pPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	double vPoly[DIFXIO_SPACECRAFT_MAX_POLY_ORDER];
	int r, r0;
	double p,v;
	
	nRow = sc->nPoint;
	pos = sc->pos;
	raw_timeoffset = sc->TFrameOffset;
	
	tMod = mjd + fracMjd;
	/* first find interpolation points */
	t0 = 0.0;
	t1 = pos[0].mjd + pos[0].fracDay;
	for(r = 1; r < nRow; r++)
	{
		t0 = t1;
		t1 = pos[r].mjd + pos[r].fracDay;
		if(t0 <= tMod && tMod < t1)
		{
			break;
		}
	}
	if(r == nRow)
	{
		if(tMod == t1) {
			r--;
		}
		else {
			return -1;
		}
	}
	r -= DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER/2;
	if(r<0) r=0;
	if(r+DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER > nRow) {
		r = nRow - DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER;
	}
	if(nRow < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER) {
		return -2;
	}
	/* Compute the temporary spacecraft delay and rate arrays. */
	/* Note that the time array must have units matching the units of */
	/* delay/rate in order for the Hermite interpolation to work */
	/* properly. */
	tMod = ((mjd-pos[r].mjd) + (fracMjd-pos[r].fracDay)) * SECONDS_PER_DAY;
	for(r0=0; r0 < DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER; r0++) {
		tPoly[r0] = ((pos[r+r0].mjd-pos[r].mjd)
					+ (pos[r+r0].fracDay-pos[r].fracDay)) * SECONDS_PER_DAY;
		pPoly[r0] = raw_timeoffset[r+r0].Delta_t;
		vPoly[r0] = raw_timeoffset[r+r0].dtdtau;
	}
	hermite_interpolation_pos_vel(DIFXIO_SPACECRAFT_ANTENNA_POLY_ORDER,
								  tPoly, pPoly, vPoly, tMod,
								 &p, &v);
	/* copy to output */
	interpolated_timeoffset->Delta_t = p;
	interpolated_timeoffset->dtdtau = v;
		

	return r;
}


