
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
#include <xlrapi.h>
#include "config.h"
#include "watchdog.h"
#include "../config.h"

const char program[] = "recover";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100517";

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);
	printf("A program that attempts to recover a Mark5 module\n\n");
	printf("Usage: %s <options> <type> <bank> [<vsn>]\n\n", pgm);
	printf("<options> can include:\n\n");
	printf("  --help\n");
	printf("  -h         Print help info and quit\n\n");
	printf("  --verbose\n");
	printf("  -v         Be more verbose in execution\n\n");
	printf("  --force\n");
	printf("  -f         Don't ask before continuing\n\n");
	printf("<type> should be 0, 1 or 2.  See below.\n\n");
	printf("<bank> should be either A or B.\n\n");
	printf("<vsn> is the new module VSN (must be 8 characters long).\n");
	printf("  If not provided, the existing VSN will be returned.\n\n");
	printf("The three types of recovery that can be attempted are:\n");
	printf("  0. Fix directory if power failed during recording.\n");
	printf("  1. Allow access to data that might have been overwritten.\n");
	printf("  2. Unerase module.\n\n");

	return 0;
}

int recoverModule(int type, int bank, int force)
{
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	S_BANKSTATUS bankStat;
	S_DIR dir;
	int nSlash=0, nDash=0, nSep=0;
	int go = 1;
	int i;
	char label[XLR_LABEL_LENGTH+1];
	char str[10];

	WATCHDOGTEST( XLROpen(1, &xlrDevice) );
	WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
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
	for(i = 0; label[i]; i++)
	{
		if(label[i] == '/')
		{
			nSlash++;
		}
		if(label[i] == '+' || label[i] == '-')
		{
			nDash++;
		}
		if(label[i] == 30)
		{
			nSep++;
		}
		if(label[i] < ' ')
		{
		
			label[i] = 0;
			break;
		}
	}
	if(nSlash == 2 && nDash == 1)
	{
		printf("\nCurrent extended VSN is %s\n", label);
		if(nSep == 1)
		{
			printf("Current disk module state is %s\n", label+1+i);
		}
	}
	else
	{
		printf("\nNo VSN currently set on module\n");
	}

	printf("%lld bytes are apparently recorded on this module\n", dir.Length);

	printf("\n");

	if(!force)
	{
		do
		{
			printf("About to attempt recover=%d on this module.  Continue? [y/n]", type);
			fgets(str, 9, stdin);
			if(strcasecmp(str, "y") == 0)
			{
				go = 1;
			}
			else if(strcasecmp(str, "n") == 0)
			{
				go = 0;
			}
			else
			{
				go = -1;
			}
		}
		while(go == -1);
	}

	if(go == 1)
	{
		switch(type)
		{
			case 0:
				WATCHDOG( xlrRC = XLRRecoverData(xlrDevice, SS_RECOVER_POWERFAIL) );
				break;
			case 1:
				WATCHDOG( xlrRC = XLRRecoverData(xlrDevice, SS_RECOVER_OVERWRITE) );
				break;
			case 2:
				WATCHDOG( xlrRC = XLRRecoverData(xlrDevice, SS_RECOVER_UNERASE) );
				break;
			default:
				xlrRC = XLR_FAIL;
				fprintf(stderr, "Developer error: type = %d\n", type);
		}

		if(xlrRC == XLR_SUCCESS)
		{
			printf("Recovery appears to be successful!\n");
		}
		else
		{
			printf("Recovery appears to have failed.\n");
		}
	}
	else
	{
		printf("\nRecovery not attempted.\n");
	}

	WATCHDOG( XLRClose(xlrDevice) );

	return 0;
}

int main(int argc, char **argv)
{
	int a, v, i;
	int type = -99;
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
		else if(type < 0)
		{
			i = sscanf(argv[a], "%d", &type);
			if(i < 0 || type < 0 || type > 2)
			{
				fprintf(stderr, "Error: type %s not recognized.  Want 0, 1, or 2",
					argv[a]);
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
			fprintf(stderr, "Error: too many arguments given.\n");
			fprintf(stderr, "Run with -h for help info\n");
			return -1;
		}
	}

	if(bank < 0)
	{
		fprintf(stderr, "Error: incomplete command line\n");
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

	recoverModule(type, bank, force);

	/* *********** */

	stopWatchdog();

	return 0;
}
