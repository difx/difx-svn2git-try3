/***************************************************************************
 *   Copyright (C) 2010 by Walter Brisken                                  *
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <difxmessage.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include "config.h"
#include "mark5dir.h"
#include "replaced.h"
#include "watchdog.h"
#include "../config.h"

const char program[] = "setvsn";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100122";

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);
	printf("A program to test Mark5 modules\n\n");
	printf("Usage: %s <options> <bank> <vsn>\n\n", pgm);
	printf("<options> can include:\n\n");
	printf("  --help\n");
	printf("  -h         Print help info and quit\n\n");
	printf("  --verbose\n");
	printf("  -v         Be more verbose in execution\n\n");
	printf("<bank> should be either A or B\n\n");
	printf("<vsn> is the new module VSN (must be 8 characters long)\n\n");

	return 0;
}

int trim(char *out, const char *in)
{
	int i, s=-1, e=0;

	for(i = 0; in[i]; i++)
	{
		if(in[i] > ' ')
		{
			if(s == -1) 
			{
				s = e = i;
			}
			else
			{
				e = i;
			}
		}
	}

	if(s == -1)
	{
		out[0] = 0;
	}
	else
	{
		strncpy(out, in+s, e-s+1);
		out[e-s+1] = 0;
	}

	return 0;
}

int setvsn(int bank, const char *newVSN)
{
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	S_DRIVEINFO dinfo;
	S_BANKSTATUS bankStat;
	char label[XLR_LABEL_LENGTH+1];
	int size, capacity;
	int ndisk, rate;
	int d;
	char drivename[XLR_MAX_DRIVENAME+1];
	char driveserial[XLR_MAX_DRIVESERIAL+1];
	char driverev[XLR_MAX_DRIVEREV+1];
	const char *newState;

	WATCHDOGTEST( XLROpen(1, &xlrDevice) );
	WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	WATCHDOGTEST( XLRSetOption(xlrDevice, SS_OPT_DRVSTATS) );
	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, bank, &bankStat) );
	if(bankStat.State != STATE_READY)
	{
		fprintf(stderr, "Bank %d not ready\n", bank);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	if(!bankStat.Selected)
	{
		printf("Hold on a few seconds while switching banks...\n");
		WATCHDOGTEST( xlrRC = XLRSelectBank(xlrDevice, bank) );
		sleep(5);
	}

	WATCHDOGTEST( XLRGetLabel(xlrDevice, label) );
	label[XLR_LABEL_LENGTH] = 0;
	if(strstr(label, "Erased") != 0)
	{
		newState = "Erased";
	}
	else if(strstr(label, "Recorded") != 0)
	{
		newState = "Recorded";
	}
	else
	{
		newState = "Played";
	}

	capacity = 0;
	ndisk = 0;
	for(d = 0; d < 8; d++)
	{
		WATCHDOG( xlrRC = XLRGetDriveInfo(xlrDevice, d/2, d%2, &dinfo) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRGetDriveInfo failed for disk %d\n", d);
			continue;
		}
		trim(drivename, dinfo.Model);
		trim(driveserial, dinfo.Serial);
		trim(driverev, dinfo.Revision);
		if(drivename[0] == 0)
		{
			printf(" %d  MISSING\n", d);
			continue;
		}
		size = (dinfo.Capacity/1000000)*512/1000;
		printf(" %d  %s %s %s  %d %d %d", d, drivename, driveserial, driverev, (dinfo.Capacity/1000000)*512/1000,
			dinfo.SMARTCapable, dinfo.SMARTState);
		if(dinfo.SMARTCapable && !dinfo.SMARTState)
		{
			printf("  FAILED\n");
		}
		else
		{
			printf("\n");
		}
		capacity += size;
		ndisk++;
	}

	WATCHDOGTEST( XLRClearWriteProtect(xlrDevice) );

	rate = ndisk*128;
	sprintf(label, "%8s/%d/%d%c%s", newVSN, capacity, rate, 30, newState);	/* ASCII "RS" == 30 */

	WATCHDOGTEST( XLRSetLabel(xlrDevice, label, strlen(label)) );

	WATCHDOG( XLRClose(xlrDevice) );

	return 0;
}

int main(int argc, char **argv)
{
	int a, v, i;
	char newVSN[10] = "";
	int bank = -1;
	int verbose = 0;

	v = initWatchdog();
	if(v < 0)
	{
		return 0;
	}

	/* 20 seconds should be enough to complete any XLR command */
	setWatchdogTimeout(20);

	for(a = 1; a < argc; a++)
	{
		if(argv[a][0] == '-')
		{
			if(strcmp(argv[a], "-v") == 0 ||
			   strcmp(argv[a], "--verbose") == 0)
			{
				verbose++;
			}
			else if(strcmp(argv[a], "-h") == 0 ||
			   strcmp(argv[a], "--help") == 0)
			{
				return usage(argv[0]);
			}
			else
			{
				fprintf(stderr, "Unknown option: %s\n", argv[a]);
				fprintf(stderr, "Run with -h for help info\n");
				return -1;
			}
		}
		else if(bank < 0)
		{
			if(strlen(argv[a]) != 1)
			{
				fprintf(stderr, "Error: expecting bank name, got %s\n", argv[a]);
				fprintf(stderr, "Run with -h for help info\n");
				return -1;
			}
			else if(argv[a][0] == 'A' || argv[a][0] == 'a')
			{
				bank = BANK_A;
			}
			else if(argv[a][0] == 'B' || argv[a][0] == 'b')
			{
				bank = BANK_B;
			}
			else
			{
				fprintf(stderr, "Error: expecting bank name, got %s\n", argv[a]);
				fprintf(stderr, "Run with -h for help info\n");
				return -1;
			}
		}
		else
		{
			if(strlen(argv[a]) != 8)
			{
				fprintf(stderr, "Error: VSN length must be 8 characters\n");
				fprintf(stderr, "Run with -h for help info\n");
				return 0;
			}
			strcpy(newVSN, argv[a]);
			newVSN[8] = 0;
			for(i = 0; 8; i++)
			{
				newVSN[i] = toupper(newVSN[i]);
			}
		}
	}

	setWatchdogVerbosity(verbose);

	if(bank < 0)
	{
		fprintf(stderr, "Error: no bank specified\n");
		fprintf(stderr, "Run with -h for help info\n");
		return -1;
	}

	if(newVSN[0] == 0)
	{
		fprintf(stderr, "Error: VSN not specified\n");
		fprintf(stderr, "Run with -h for help info\n");
		return -1;
	}

	/* *********** */

	setvsn(bank, newVSN);

	/* *********** */

	stopWatchdog();

	return 0;
}
