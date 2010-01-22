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
// $Id: mk5cp.cpp 1888 2009-12-31 21:56:22Z WalterBrisken $
// $HeadURL: https://svn.atnf.csiro.au/difx/applications/mk5daemon/branches/difx-1.5/mk5dir/mk5cp.cpp $
// $LastChangedRevision: 1888 $
// $Author: WalterBrisken $
// $LastChangedDate: 2009-12-31 14:56:22 -0700 (Thu, 31 Dec 2009) $
//
//============================================================================

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <pthread.h>
#include <time.h>

extern time_t watchdogTime;
extern int watchdogVerbose;
extern char watchdogStatement[256];
extern pthread_mutex_t watchdogLock;

/* Macro to run "statement" but set a thread to watch to make sure it doesn't take too long */
#define WATCHDOG(statement) \
{ \
	pthread_mutex_lock(&watchdogLock); \
	watchdogTime = time(0); \
	strcpy(watchdogStatement, #statement); \
	if(watchdogVerbose > 1) printf("Executing (at time %d): %s\n", (int)(watchdogTime), watchdogStatement); \
	pthread_mutex_unlock(&watchdogLock); \
	statement; \
	pthread_mutex_lock(&watchdogLock); \
	if(watchdogVerbose > 2) printf("Executed (in %d seconds): %s\n", (int)(time(0)-watchdogTime), watchdogStatement); \
	watchdogTime = 0; \
	watchdogStatement[0] = 0; \
	pthread_mutex_unlock(&watchdogLock); \
}

/* same as WATCHDOG but performs a return -1 if the return value is not XLR_SUCCESS */
#define WATCHDOGTEST(statement) \
{ \
	XLR_RETURN_CODE watchdogRC; \
	pthread_mutex_lock(&watchdogLock); \
	watchdogTime = time(0); \
	strcpy(watchdogStatement, #statement); \
	if(watchdogVerbose > 1) printf("Executing (at time %d): %s\n", (int)(watchdogTime), watchdogStatement); \
	pthread_mutex_unlock(&watchdogLock); \
	watchdogRC = (statement); \
	pthread_mutex_lock(&watchdogLock); \
	if(watchdogVerbose > 2) printf("Executed (in %d seconds): %s\n", (int)(time(0)-watchdogTime), watchdogStatement); \
	watchdogTime = 0; \
	watchdogStatement[0] = 0; \
	pthread_mutex_unlock(&watchdogLock); \
	if(watchdogRC != XLR_SUCCESS) \
	{ \
		fprintf(stderr, "%s failed!\n", #statement); \
		return -1; \
	} \
}

void setWatchdogVerbosity(int v);

void setWatchdogTimeout(int timeout);

int initWatchdog();

void stopWatchdog();

#endif
