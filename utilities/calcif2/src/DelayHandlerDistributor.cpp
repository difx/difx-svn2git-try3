/***************************************************************************
 *   Copyright (C) 2015 by James M Anderson                                *
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


// INCLUDES
#ifndef __STDC_CONSTANT_MACROS
#  define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#ifndef __USE_ISOC99
#  define __USE_ISOC99 1
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef __USE_MISC
#  define __USE_MISC 1
#endif
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include <inttypes.h>
#include <limits>
#ifdef __cplusplus
#  include <cstddef>
#else
#  include <stddef.h>
#endif
#include <stdint.h>
// we want to use ISO C9X stuff
// we want to use some GNU stuff
// But this sometimes breaks time.h
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <limits>

#include "DelayHandlerDistributor.h"
#include "DelayHandlerBase.h"
#include "DelayHandlerCalcServer9_1.h"
#include "DelayHandlerCalcServer9_1_RA.h"
#include "DelayHandlerDiFXDelayServer.h"
#include <difxio.h>

namespace DiFX {
namespace Delay {
namespace Handler {






// GLOBALS


// FUNCTIONS

DelayHandlerDistributor::DelayHandlerDistributor(const uint_fast32_t NUM_THREADS_, const char* const delayServerHost, const enum DelayServerHandlerType handler_, const enum DelayServerType server_, const int_fast32_t verbosity_) throw()
		:
		delayHandler(NULL)
{
	uint_fast32_t NUM_THREADS = get_best_NUM_THREADS(NUM_THREADS_, verbosity_);

	switch(handler_)
	{
	case DelayServerHandler_DiFX_Delay_Server:
		delayHandler = new DelayHandlerDiFXDelayServer(NUM_THREADS, verbosity_, delayServerHost, delayServerTypeIds[uint32_t(server_)]);
		break;
	case DelayServerHandler_Pipe:
		switch(server_)
		{
		case CALCServer:
			delayHandler = new DelayHandlerCalcServer9_1(NUM_THREADS, verbosity_);
			break;
		case CALC_9_1_RA_Server:
			delayHandler = new DelayHandlerCalcServer9_1_RA(NUM_THREADS, verbosity_);
			break;
		default:
			fprintf(stderr, "Error: unknown DelayServerType %d ('%s') in DelayHandlerDistributor::DelayHandlerDistributor\n", int(server_), delayServerTypeNames[uint32_t(server_)]);
		}
		break;
	default:
		fprintf(stderr, "Error: unknown DelayServerHandlerType %d ('%s') in DelayHandlerDistributor::DelayHandlerDistributor\n", int(handler_), delayServerHandlerTypeNames[uint32_t(handler_)]);
	}
	return;
}

    
DelayHandlerDistributor::~DelayHandlerDistributor()
{
	delete delayHandler; delayHandler = NULL;
	return;
}


uint_fast32_t DelayHandlerDistributor::get_best_NUM_THREADS(const uint_fast32_t NUM_THREADS_, const int_fast32_t verbosity) const throw()
{
	if(NUM_THREADS_ > 0)
	{
		return NUM_THREADS_;
	}
	const char* env = getenv("DIFX_NUMBERCALCSERVERTHREADS");
	if((env))
	{
		uint_fast32_t n = strtoul(env, NULL, 0);
		if(n > 0)
		{
			return n;
		}
		if(verbosity > 1)
		{
			fprintf(stderr, "Warning: environment variable DIFX_NUMBERCALCSERVERTHREADS has invalid value '%s' --- using 1 thread for CALC calls\n", env);
		}
	}
	else if(verbosity > 1)
	{
		fprintf(stderr, "Warning: environment variable DIFX_NUMBERCALCSERVERTHREADS not found --- using 1 thread for CALC calls\n");
	}
	return 1;
}


}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX


