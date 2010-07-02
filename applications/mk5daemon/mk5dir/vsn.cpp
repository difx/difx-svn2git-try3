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
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <xlrapi.h>
#include "config.h"
#include "mark5dir.h"
#include "watchdog.h"
#include "../config.h"

const char program[] = "vsn";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100525";

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);
	printf("A program to set/get a Mark5 module Volume Serial Number (VSN)\n\n");
	printf("Usage: %s <options> <bank> [<vsn>]\n\n", pgm);
	printf("<options> can include:\n\n");
	printf("  --help\n");
	printf("  -h         Print help info and quit\n\n");
	printf("  --verbose\n");
	printf("  -v         Be more verbose in execution\n\n");
	printf("  --force\n");
	printf("  -f         Don't ask before continuing\n\n");
	printf("<bank> should be either A or B.\n\n");
	printf("<vsn> is the new module VSN (must be 8 characters long).\n");
	printf("  If not provided, the existing VSN will be returned.\n\n");

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

int roundsize(int s)
{
	long long a;

	a = (long long)s*512LL;
	a /= 1000000000;
	a = (a+2)/5;

	return a*5;
}

int setvsn(int bank, const char *newVSN, int force)
{
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	S_DRIVEINFO driveInfo;
	S_BANKSTATUS bankStat;
	S_DIR dir;
	char label[XLR_LABEL_LENGTH+1];
	char oldLabel[XLR_LABEL_LENGTH+1];
	int size, capacity;
	int ndisk, rate;
	long long isz;
	int icnt;
	int i, d;
	int nSlash=0, nDash=0;
	char drivename[XLR_MAX_DRIVENAME+1];
	char driveserial[XLR_MAX_DRIVESERIAL+1];
	char driverev[XLR_MAX_DRIVEREV+1];
	char oldVSN[10];
	char resp[12]="";
	char *v;
	const char *moduleStatus;
	int dirLength;

	WATCHDOGTEST( XLROpen(1, &xlrDevice) );
	WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	WATCHDOGTEST( XLRSetOption(xlrDevice, SS_OPT_DRVSTATS) );
	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, bank, &bankStat) );
	if(bankStat.State != STATE_READY)
	{
		fprintf(stderr, "Bank %c not ready\n", 'A'+bank);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	if(!bankStat.Selected)
	{
		WATCHDOGTEST( XLRSelectBank(xlrDevice, bank) );
	}

	/* the following line is essential to work around an apparent streamstor bug */
	WATCHDOGTEST( XLRGetDirectory(xlrDevice, &dir) );

	WATCHDOGTEST( XLRGetLabel(xlrDevice, label) );
	label[XLR_LABEL_LENGTH] = 0;

	WATCHDOG( dirLength = XLRGetUserDirLength(xlrDevice) );

	if(dirLength == 0)
	{
		moduleStatus = "Unknown";

		fprintf(stderr, "Warning: there is no directory on this module.\n");
	}
	else if(dirLength % 128 != 0)
	{
		if(strstr(label, "Erased") != 0)
		{
			moduleStatus = "Erased";
		}
		else if(strstr(label, "Recorded") != 0)
		{
			moduleStatus = "Recorded";
		}
		else
		{
			moduleStatus = "Played";
		}
	}
	else
	{
		struct Mark5DirectoryHeaderVer1 dirHeader;
		WATCHDOGTEST( XLRGetUserDir(xlrDevice, sizeof(struct Mark5DirectoryHeaderVer1), 0, &dirHeader) );
		moduleStatus = moduleStatusName(dirHeader.status);
	}

	strcpy(oldLabel, label);
	for(i = 0; oldLabel[i]; i++)
	{
		if(oldLabel[i] == '/')
		{
			nSlash++;
		}
		if(oldLabel[i] == '+' || oldLabel[i] == '-')
		{
			nDash++;
		}
		if(oldLabel[i] < ' ')
		{
		
			oldLabel[i] = 0;
			break;
		}
	}
	if(nSlash == 2 && nDash == 1)
	{
		printf("\nCurrent extended VSN is %s\n", oldLabel);
		printf("Current disk module state is %s\n", moduleStatus);
	}
	else
	{
		printf("\nNo VSN currently set on module\n");
	}

	printf("%lld bytes recorded on this module\n", dir.Length);

	printf("\n");

	capacity = 0;
	ndisk = 0;
	isz = 0LL;
	icnt = 0;
	for(d = 0; d < 8; d++)
	{
		WATCHDOG( xlrRC = XLRGetDriveInfo(xlrDevice, d/2, d%2, &driveInfo) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRGetDriveInfo failed for disk %d\n", d);
			continue;
		}
		trim(drivename, driveInfo.Model);
		trim(driveserial, driveInfo.Serial);
		trim(driverev, driveInfo.Revision);
		if(drivename[0] == 0)
		{
			printf(" %d  MISSING\n", d);
			continue;
		}
		if(driveInfo.Capacity > 0)
		{
			icnt++;
		}
		if(isz == 0 || (driveInfo.Capacity > 0 && driveInfo.Capacity * 512LL < isz))
		{
			isz = driveInfo.Capacity * 512LL;
		}
		size = roundsize(driveInfo.Capacity);
		printf(" %d  %s (%s) %s  %d %d %d", d, drivename, driveserial, driverev, 
			size, driveInfo.SMARTCapable, driveInfo.SMARTState);
		if(driveInfo.SMARTCapable && !driveInfo.SMARTState)
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

	capacity = (icnt*isz/10000000000LL)*10;

	if(newVSN[0])
	{
		if(nSlash == 2 && nDash == 1)
		{
			strncpy(oldVSN, oldLabel, 8);
			oldVSN[8] = 0;
			printf("\nAbout to change the module VSN from %s to %s\n", oldVSN, newVSN);
		}
		else
		{
			printf("\nAbout to set the module VSN to %s\n", newVSN);
		}
		if(force == 0)
		{
			printf("Is this OK? [y|n]\n");
			v = fgets(resp, 10, stdin);
		}
		if(force || resp[0] == 'Y' || resp[0] == 'y')
		{
			WATCHDOGTEST( XLRClearWriteProtect(xlrDevice) );

			rate = ndisk*128;
			sprintf(label, "%8s/%d/%d%c%s", newVSN, capacity, rate, 30, moduleStatus);	/* ASCII "RS" == 30 */

			WATCHDOGTEST( XLRSetLabel(xlrDevice, label, strlen(label)) );

			printf("Done.\n");
		}
		else
		{
			printf("\nNot changing VSN.\n");
		}
	}
	else
	{
		printf("\nNot changing VSN.\n");
	}

	WATCHDOG( XLRClose(xlrDevice) );

	return 0;
}

int main(int argc, char **argv)
{
	int a, v, i;
	char newVSN[10] = "";
	int bank = -1;
	int verbose = 0;
	int force = 0;

	for(a = 1; a < argc; a++)
	{
		if(argv[a][0] == '-')
		{
			if(strcmp(argv[a], "-v") == 0 ||
			   strcmp(argv[a], "--verbose") == 0)
			{
				verbose++;
			}
			else if(strcmp(argv[a], "-f") == 0 ||
			   strcmp(argv[a], "--force") == 0)
			{
				force++;
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
			for(i = 0; i < 8; i++)
			{
				newVSN[i] = toupper(newVSN[i]);
			}
		}
	}

	if(bank < 0)
	{
		fprintf(stderr, "Error: no bank specified\n");
		fprintf(stderr, "Run with -h for help info\n");
		return -1;
	}

	v = initWatchdog();
	if(v < 0)
	{
		return 0;
	}

	/* 60 seconds should be enough to complete any XLR command */
	setWatchdogTimeout(60);

	setWatchdogVerbosity(verbose);

	/* *********** */

	v = setvsn(bank, newVSN, force);
	if(v < 0)
	{
		if(watchdogXLRError[0] != 0)
		{
			char message[DIFX_MESSAGE_LENGTH];
			snprintf(message, DIFX_MESSAGE_LENGTH, 
				"Streamstor error executing: %s : %s",
				watchdogStatement, watchdogXLRError);
			difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		}
	}

	/* *********** */

	stopWatchdog();

	return 0;
}
