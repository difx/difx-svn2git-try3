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
#include <string.h> 
#include <unistd.h>
#include "xlrapi.h"
#include "watchdog.h"
#include "difxmessage.h"

/* Note: this program is largely based on Haystack's SSErase.  Thanks
 * to John Ball and Dan Smythe for providing a nice template 
 */

const char program[] = "mk5erase";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100525";

int verbose = 0;
int die = 0;

typedef void (*sighandler_t)(int);

sighandler_t oldsiginthand;

void siginthand(int j)
{
	if(verbose)
	{
		printf("Being killed\n");
	}
	die = 1;
}

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);
	printf("A program to erase or condition a Mark5 module.\n");
	printf("\nUsage : %s [<options>] <vsn>\n\n", pgm);
	printf("options can include:\n");
	printf("  --help\n");
	printf("  -h             Print this help message\n\n");
	printf("  --verbose\n");
	printf("  -v             Be more verbose\n\n");
	printf("  --force\n");
	printf("  -f             Don't ask to continue\n\n");
	printf("  --condition\n");
	printf("  -c             Do full conditioning, not just erasing\n");
	printf("<vsn> is a valid module VSN (8 characters)\n\n");
	printf("Note: A single Mark5 unit needs to be installed in bank A for\n");
	printf("proper operation.  If the VSN is not set, use the  vsn  utility\n");
	printf("To assign it prior to erasure or conditioning.\n\n");

	return 0;
}

int main(int argc, char **argv)
{
	int force = 0;
	int cond = 0;
	char vsn[10] = "";

	for(int a = 1; a < argc; a++)
	{
		if(strcmp(argv[a], "-h") == 0 ||
		   strcmp(argv[a], "--help") == 0)
		{
			return usage(argv[0]);
		}
		else if(strcmp(argv[a], "-v") == 0 ||
		        strcmp(argv[a], "--verbose") == 0)
		{
			verbose++;
		}
		else if(strcmp(argv[a], "-c") == 0 ||
		        strcmp(argv[a], "--condition") == 0)
		{
			cond = 1;
		}
		else if(strcmp(argv[a], "-f") == 0 ||
		        strcmp(argv[a], "--force") == 0)
		{
			force = 1;
		}
		else if(argv[a][0] == '-')
		{
			fprintf(stderr, "Unknown option %s provided\n", argv[a]);
			return 0;
		}
		else
		{
			if(vsn[0])
			{
				fprintf(stderr, "Error: two VSNs provided : %s and %s\n",
					vsn, argv[a]);
				return 0;
			}
			strncpy(vsn, argv[a], 10);
			vsn[9] = 0;
			if(strlen(vsn) != 8)
			{
				fprintf(stderr, "Error: VSN %s not 8 chars long!\n", argv[a]);
				return 0;
			}
		}
	}

	printf("About to proceed.  verbose=%d cond=%d force=%d vsn=%s\n",
		verbose, cond, force, vsn);

	return 0;
}
