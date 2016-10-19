/***************************************************************************
 *   Copyright (C) 2015, 2016 by James M Anderson                                *
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
#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS // For non-compliant C++ compilers
#endif
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
#include <signal.h>
#include <string.h>
#include <limits>

#include "DelayHandlerCalcServer9_1.h"
#include "DelayTimestamp.h"
#include "DelayHandlerBasePipeInterface.h"








namespace DiFX {
namespace Delay {
namespace Handler {






// GLOBALS
const char* DelayHandlerCalcServer9_1::server_command_name = "CalcServer_pipe";

namespace {
/* Note: This is a particular NaN variant the FITS-IDI format/convention 
 * wants, namely 0xFFFFFFFFFFFFFFFF */
static const union
{
	uint64_t u64;
	double d;
	float f;
} fitsnan = {UINT64_C(0xFFFFFFFFFFFFFFFF)};
    
}
// FUNCTIONS

DelayHandlerCalcServer9_1::DelayHandlerCalcServer9_1(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_) throw()
		:
		DelayHandlerBase(NUM_THREADS_, verbosity_),
		local_arg(NULL),
		local_res(NULL),
		local_Num_Stations_Sources(NULL),
		local_current_Num(NULL)
{
	int_fast32_t retval;
	retval = allocate_memory();
	if((retval))
	{
		overall_status = delayHandlerStatus_HandlerError;
		return;
	}
	retval = init_threads();
	if((retval))
	{
		overall_status = delayHandlerStatus_HandlerError;
		return;
	}
	return;
}

    
DelayHandlerCalcServer9_1::~DelayHandlerCalcServer9_1()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running ~DelayHandlerCalcServer9_1\n");
	}
	stop_threads();
	dealloc_memory();
	return;
}






int_fast32_t DelayHandlerCalcServer9_1::allocate_memory() throw()
{
	local_arg = reinterpret_cast<struct CALCSERVER9_1_MODEL_DELAY_ARGUMENT**>(malloc(NUM_THREADS*sizeof(struct CALCSERVER9_1_MODEL_DELAY_ARGUMENT*)));
	local_res = reinterpret_cast<struct CALCSERVER9_1_MODEL_DELAY_RESPONSE**>(malloc(NUM_THREADS*sizeof(struct CALCSERVER9_1_MODEL_DELAY_RESPONSE*)));
	local_Num_Stations_Sources = reinterpret_cast<uint_fast32_t*>(malloc(NUM_THREADS*sizeof(uint_fast32_t)));
	local_current_Num = reinterpret_cast<uint_fast32_t*>(malloc(NUM_THREADS*sizeof(uint_fast32_t)));
	if((local_arg == NULL) || (local_res == NULL) || (local_Num_Stations_Sources == NULL) || (local_current_Num == NULL))
	{
		DHBfprintf(stderr, "Error: could not malloc memory\n");
		return -1;
	}
	for(int_fast32_t tid=0; tid < NUM_THREADS; ++tid)
	{
		local_arg[tid] = NULL;
		local_res[tid] = NULL;
		local_Num_Stations_Sources[tid] = 0;
		local_current_Num[tid] = 0;
	}
	return 0;
}
void DelayHandlerCalcServer9_1::dealloc_memory() throw()
{
	free(local_arg); local_arg=NULL;
	free(local_res); local_res=NULL;
	free(local_Num_Stations_Sources); local_Num_Stations_Sources=NULL;
	free(local_current_Num); local_current_Num=NULL;
	return;
}






int_fast32_t DelayHandlerCalcServer9_1::callServerForInit(const int_fast32_t tid) throw()
{
    return 0;
}


int_fast32_t DelayHandlerCalcServer9_1::callServerForDelay(const int_fast32_t tid) throw()
{
	int_fast32_t retval = 0;
	int_fast32_t final_retval = 0;
	uint32_t Num;
	uint64_t Bytes;

	if(local_current_Num[tid] == 0)
	{
		// No data to transfer
		return 0;
	}

	if((child_process[tid] <= 0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		final_retval = -1;
		goto callServerForDelay_error;
	}
	// The do_thread calls have set up the ARGUMENT and RESPONSE pointer areas
	// already, so we can call the child process now.
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" calling delay server for delay\n", tid);
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DODELAY, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    Num = local_current_Num[tid];
    if(fwrite(&Num, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    // The following is guaranteed to fit in a uint64_t, as
    // Num is only 32 bits and sizeof(CALCSERVER9_1_MODEL_DELAY_ARGUMENT)
    // is not very large.
    Bytes = uint64_t(sizeof(CALCSERVER9_1_MODEL_DELAY_ARGUMENT)*Num);
    if(fwrite(&Bytes, sizeof(uint64_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATA, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(local_arg[tid], size_t(Bytes), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATADONE, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fflush(child_stdin[tid]) != 0)
    {
	    retval -= 20;
    }
    if((retval))
    {
	    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to write to child process with failure response code %"PRIdFAST32"\n", tid, retval);
        final_retval = -2;
		goto callServerForDelay_error;
    }
    if((verbose>=5))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" delay request sent\n", tid);
    }
    // Now read in the response
    Num = 0;
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START)
    {
	    final_retval = -3;
	    goto callServerForDelay_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DODELAY)
    {
	    final_retval = -4;
	    goto callServerForDelay_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_STATUS_GOOD)
    {
	    final_retval = -5;
	    goto callServerForDelay_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fread(&Bytes, sizeof(uint64_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if((Num != local_current_Num[tid]) || (Bytes != sizeof(CALCSERVER9_1_MODEL_DELAY_RESPONSE)*Num))
    {
	    final_retval = -6;
	    goto callServerForDelay_error;
    }
    Num = 0;
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATA)
    {
	    final_retval = -7;
	    goto callServerForDelay_error;
    }
    if(fread(local_res[tid], Bytes, 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATADONE)
    {
	    final_retval = -8;
	    goto callServerForDelay_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END)
    {
	    final_retval = -9;
	    goto callServerForDelay_error;
    }
    if((verbose>=5))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" delay response read\n", tid);
    }
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" delay response read back\n", tid);
    }
    return 0;
callServerForDelay_error:
    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to read delay from child with final_retval=%"PRIdFAST32" retval=%"PRIdFAST32"\n", tid, final_retval, retval);
    panic_stop_child(tid);
    return final_retval;
}
int_fast32_t DelayHandlerCalcServer9_1::callServerForParameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval = 0;
	int_fast32_t final_retval = 0;
	uint32_t Num;
	uint64_t Bytes;

	if(tid != 0)
	{
		// Only thread 0 requests the parameters
		return 0;
	}

	if((child_process[tid] <= 0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		final_retval = -1;
		goto callServerForParameters_error;
	}
	// The do_thread calls have set up the ARGUMENT and RESPONSE pointer areas
	// already, so we can call the child process now.
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" calling delay server for parameters\n", tid);
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOPARAM, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    Num = 1;
    if(fwrite(&Num, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    // The following is guaranteed to fit in a uint64_t, as
    // Num is only 1 and sizeof(CALCSERVER9_1_MODEL_PARAMETERS_ARGUMENT)
    // is not very large.
    Bytes = uint64_t(sizeof(CALCSERVER9_1_MODEL_PARAMETERS_ARGUMENT));
    if(fwrite(&Bytes, sizeof(uint64_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATA, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&local_parg, size_t(Bytes), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATADONE, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fflush(child_stdin[tid]) != 0)
    {
	    retval -= 20;
    }
    if((retval))
    {
	    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to write to child process with failure response code %"PRIdFAST32"\n", tid, retval);
        final_retval = -2;
		goto callServerForParameters_error;
    }
    if((verbose>=5))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" parameters request sent\n", tid);
    }
    // Now read in the response
    Num = 0;
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START)
    {
	    final_retval = -3;
	    goto callServerForParameters_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOPARAM)
    {
	    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to read child process response command 0x%"PRIX32", got 0x%"PRIX32" instead\n", tid, DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOPARAM, Num);
	    final_retval = -4;
	    goto callServerForParameters_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_STATUS_GOOD)
    {
	    final_retval = -5;
	    goto callServerForParameters_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fread(&Bytes, sizeof(uint64_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if((Num != 1) || (Bytes != sizeof(CALCSERVER9_1_MODEL_PARAMETERS_RESPONSE)))
    {
	    final_retval = -6;
	    goto callServerForParameters_error;
    }
    Num = 0;
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATA)
    {
	    final_retval = -7;
	    goto callServerForParameters_error;
    }
    if(fread(&local_pres, Bytes, 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATADONE)
    {
	    final_retval = -8;
	    goto callServerForParameters_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END)
    {
	    final_retval = -9;
	    goto callServerForParameters_error;
    }
    if((verbose>=5))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" parameters response read\n", tid);
    }
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" parameters response read back\n", tid);
    }
    return 0;
callServerForParameters_error:
    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to read parameters from child with final_retval=%"PRIdFAST32" retval=%"PRIdFAST32"\n", tid, final_retval, retval);
    panic_stop_child(tid);
    return final_retval;
}
int_fast32_t DelayHandlerCalcServer9_1::callServerForStop(const int_fast32_t tid) throw()
{
	int_fast32_t retval = 0;
	int_fast32_t final_retval = 0;
	uint32_t Num;

	if((child_process[tid] <= 0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		final_retval = -1;
		goto callServerForStop_error;
	}
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" calling delay server for stop\n", tid);
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOSTOP, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fwrite(&DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END, sizeof(uint32_t), 1, child_stdin[tid]) != 1)
    {
	    retval -= 1;
    }
    if(fflush(child_stdin[tid]) != 0)
    {
	    retval -= 20;
    }
    if((retval))
    {
	    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to write to child process with failure response code %"PRIdFAST32"\n", tid, retval);
        final_retval = -2;
		goto callServerForStop_error;
    }
    if((verbose>=5))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" stop request sent\n", tid);
    }
    // Now read in the response
    Num = 0;
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START)
    {
	    final_retval = -3;
	    goto callServerForStop_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOSTOP)
    {
	    final_retval = -4;
	    goto callServerForStop_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_STATUS_DONE)
    {
	    final_retval = -5;
	    goto callServerForStop_error;
    }
    if(fread(&Num, sizeof(uint32_t), 1, child_stdout[tid]) != 1)
    {
	    retval -= 1;
    }
    if(Num != DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END)
    {
	    final_retval = -6;
	    goto callServerForStop_error;
    }
    if((verbose>=5))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" stop response read\n", tid);
    }
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerCalcServer9_1 thread %"PRIdFAST32" stop response read back\n", tid);
    }
    return 0;
callServerForStop_error:
    DHBfprintf(stderr, "thread %"PRIdFAST32" failed to stop child with final_retval=%"PRIdFAST32" retval=%"PRIdFAST32"\n", tid, final_retval, retval);
    panic_stop_child(tid);
    return final_retval;
}

int_fast32_t DelayHandlerCalcServer9_1::callServerForPanic(const int_fast32_t tid) throw()
{
    DHBfprintf(stderr, "thread %"PRIdFAST32" attempting panic stop of child process\n", tid);
    panic_stop_child(tid);
    DHBfprintf(stderr, "thread %"PRIdFAST32" finished panic stop of child process\n", tid);
    return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::freeServerDelayPointers(const int_fast32_t tid) throw()
{
	/* Nothing to do */
	return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::freeServerParametersPointers(const int_fast32_t tid) throw()
{
	/* Nothing to do */
	return 0;
}




                      


int_fast32_t DelayHandlerCalcServer9_1::do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" will do_work on action=%d\n", tid, int(action_copy));
	}
	int_fast32_t retval = 0;
    switch(action_copy) {
    case delayHandlerAction_Sleep:
    case delayHandlerAction_Create:
        break;
    case delayHandlerAction_Init:
	    retval = do_thread_init(tid);
	    break;
    case delayHandlerAction_TestDelays:
	    retval = do_thread_test_delays(tid);
	    break;
    case delayHandlerAction_TestParameters:
	    retval = do_thread_test_parameters(tid);
	    break;
    case delayHandlerAction_GetDelays:
	    retval = do_thread_get_delays(tid);
	    break;
    case delayHandlerAction_GetParameters:
	    retval = do_thread_get_parameters(tid);
	    break;
    case delayHandlerAction_Stop:
	    retval = do_thread_stop(tid);
	    break;
    case delayHandlerAction_Panic:
	    retval = do_thread_panic(tid);
	    break;
    default:
	    DHBfprintf(stdout, "ERROR: %s:%d: thread %"PRIdFAST32" unknown action=%d\n", __FILE__, __LINE__, tid, int(action_copy));
	    retval = -201;
        break;
    }
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" finished do_work on action=%d with retval=%"PRIdFAST32"\n", tid, int(action_copy), retval);
	}
    return retval;
}



int_fast32_t DelayHandlerCalcServer9_1::do_thread_init(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_init\n", tid);
	}
	const char* argv[4];
	const size_t ID_MAX = 64;
	char arg2[ID_MAX];
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_init_error;
	}
	{
		int return_code = snprintf(arg2, ID_MAX, "%06"PRIdLEAST64"_%03"PRIdFAST32, parent_pid, tid);
		if((return_code < 0) || (size_t(return_code) >= ID_MAX))
		{
			DHBfprintf(stderr, "Error: %s:%d: not enough space in string\n", __FILE__, __LINE__);
			retval_final = -2;
			goto do_thread_init_error;
		}
	}
	argv[0] = server_command_name;
	argv[1] = TMP_DIRECTORY;
	argv[2] = arg2;
	argv[3] = NULL;
	retval = start_child_process(server_command_name, argv, server_command_name, tid);
	if((retval))
	{
		DHBfprintf(stderr, "Error: start_child_process for server command '%s' failed for thread=%"PRIdFAST32" with code %"PRIdFAST32"\n", server_command_name, tid, retval);
		retval_final = -3;
		goto do_thread_init_error;
	}
	retval = callServerForInit(tid);
	if((retval))
	{
		DHBfprintf(stderr, "Error: callServerForInit failed for thread=%"PRIdFAST32" with code %"PRIdFAST32"\n", tid, retval);
		retval_final = -4;
		goto do_thread_init_error;
	}
do_thread_init_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_init with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerCalcServer9_1::do_thread_test_delays(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_test_delays\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_test_delays_error;
	}
	retval = test_local_SERVER_MODEL_DELAY_ARGUMENT(tid);
	if((retval))
	{
		DHBfprintf(stderr, "test_local_SERVER_MODEL_DELAY_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -2;
		goto do_thread_test_delays_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_DELAY_ARGUMENT(tid);
	}
	retval = callServerForDelay(tid);
	if((retval))
	{
		DHBfprintf(stderr, "callServerForDelay failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -3;
		goto do_thread_test_delays_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_DELAY_RESPONSE(tid);
	}
	retval = test_local_SERVER_MODEL_DELAY_RESPONSE(tid);
	if((retval))
	{
		DHBfprintf(stderr, "test_local_SERVER_MODEL_DELAY_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		freeServerDelayPointers(tid);
		retval_final = -4;
		goto do_thread_test_delays_error;
	}
	retval = freeServerDelayPointers(tid);
	if((retval))
	{
		DHBfprintf(stderr, "freeServerDelayPointers failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -5;
		goto do_thread_test_delays_error;
	}
do_thread_test_delays_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_test_delays with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerCalcServer9_1::do_thread_test_parameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_test_parameters\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_test_parameters_error;
	}
	retval = test_local_SERVER_MODEL_PARAMETERS_ARGUMENT(tid);
	if((retval))
	{
		DHBfprintf(stderr, "test_local_SERVER_MODEL_PARAMETERS_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -2;
		goto do_thread_test_parameters_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_PARAMETERS_ARGUMENT(tid);
	}
	retval = callServerForParameters(tid);
	if((retval))
	{
		DHBfprintf(stderr, "callServerForParameters failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -3;
		goto do_thread_test_parameters_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_PARAMETERS_RESPONSE(tid);
	}
	retval = test_local_SERVER_MODEL_PARAMETERS_RESPONSE(tid);
	if((retval))
	{
		DHBfprintf(stderr, "test_local_SERVER_MODEL_PARAMETERS_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		freeServerParametersPointers(tid);
		retval_final = -4;
		goto do_thread_test_parameters_error;
	}
	retval = freeServerParametersPointers(tid);
	if((retval))
	{
		DHBfprintf(stderr, "freeServerParametersPointers failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -5;
		goto do_thread_test_parameters_error;
	}
do_thread_test_parameters_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_test_parameters with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerCalcServer9_1::do_thread_get_delays(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_get_delays\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_get_delays_error;
	}
	retval = convert_to_local_SERVER_MODEL_DELAY_ARGUMENT(tid);
	if((retval))
	{
		DHBfprintf(stderr, "convert_to_local_SERVER_MODEL_DELAY_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -2;
		goto do_thread_get_delays_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_DELAY_ARGUMENT(tid);
	}
	retval = callServerForDelay(tid);
	if((retval))
	{
		DHBfprintf(stderr, "callServerForDelay failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -3;
		goto do_thread_get_delays_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_DELAY_RESPONSE(tid);
	}
	retval = convert_from_local_SERVER_MODEL_DELAY_RESPONSE(tid);
	if((retval))
	{
		DHBfprintf(stderr, "convert_from_local_SERVER_MODEL_DELAY_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		freeServerDelayPointers(tid);
		retval_final = -4;
		goto do_thread_get_delays_error;
	}
	retval = freeServerDelayPointers(tid);
	if((retval))
	{
		DHBfprintf(stderr, "freeServerDelayPointers failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -5;
		goto do_thread_get_delays_error;
	}
do_thread_get_delays_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_get_delays with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerCalcServer9_1::do_thread_get_parameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_get_parameters\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_get_parameters_error;
	}
	retval = convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT(tid);
	if((retval))
	{
		DHBfprintf(stderr, "convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -2;
		goto do_thread_get_parameters_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_PARAMETERS_ARGUMENT(tid);
	}
	retval = callServerForParameters(tid);
	if((retval))
	{
		DHBfprintf(stderr, "callServerForParameters failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -3;
		goto do_thread_get_parameters_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_PARAMETERS_RESPONSE(tid);
	}
	retval = convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE(tid);
	if((retval))
	{
		DHBfprintf(stderr, "convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		freeServerParametersPointers(tid);
		retval_final = -4;
		goto do_thread_get_parameters_error;
	}
	retval = freeServerParametersPointers(tid);
	if((retval))
	{
		DHBfprintf(stderr, "freeServerParametersPointers failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -5;
		goto do_thread_get_parameters_error;
	}
do_thread_get_parameters_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_get_parameters with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}




int_fast32_t DelayHandlerCalcServer9_1::do_thread_stop(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_stop\n", tid);
	}
	retval = callServerForStop(tid);
	if((retval))
	{
		DHBfprintf(stderr, "callServerForStop failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -1;
		goto do_thread_stop_error;
	}
do_thread_stop_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_stop with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerCalcServer9_1::do_thread_panic(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_panic\n", tid);
	}
	retval = callServerForPanic(tid);
	if((retval))
	{
		DHBfprintf(stderr, "callServerForPanic failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -1;
		goto do_thread_panic_error;
	}
do_thread_panic_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_panic with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}





int_fast32_t DelayHandlerCalcServer9_1::test_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t tid) throw()
{
	uint_fast32_t my_NUM_Stations = 1;
	uint_fast32_t my_NUM_Sources = 1;
	uint_fast32_t my_NUM_EOPs = 5;
	uint_fast32_t my_NUM = my_NUM_Stations*my_NUM_Sources;
	if(my_NUM > local_Num_Stations_Sources[tid])
	{
		local_arg[tid] = reinterpret_cast<CALCSERVER9_1_MODEL_DELAY_ARGUMENT*>(realloc(local_arg[tid],my_NUM*sizeof(CALCSERVER9_1_MODEL_DELAY_ARGUMENT)));
		local_res[tid] = reinterpret_cast<CALCSERVER9_1_MODEL_DELAY_RESPONSE*>(realloc(local_res[tid],my_NUM*sizeof(CALCSERVER9_1_MODEL_DELAY_RESPONSE)));
		if((local_arg[tid] == NULL) || (local_res[tid] == NULL))
		{
			DHBfprintf(stderr, "cannot realloc local memory for thread=%"PRIdFAST32"\n", tid);
			return -1;
		}
		local_Num_Stations_Sources[tid] = my_NUM;
	}
	local_current_Num[tid] = my_NUM;
	if(my_NUM_EOPs != NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS)
	{
		DHBfprintf(stderr, "Error: %s:%d: Hardcoded my_Num_EOPs (%"PRIuFAST32") wrong\n", __FILE__, __LINE__, my_NUM_EOPs);
		return -2;
	}

	local_arg[tid][0].request_id = 150;
	local_arg[tid][0].date = 50774;
	local_arg[tid][0].ref_frame = 0;
	for(uint_fast8_t k=0; k < NUM_DIFX_DELAYHANDLERCALCSERVER9_1_KFLAGS; ++k)
	{
		local_arg[tid][0].kflags[k] = -1;
	}
	local_arg[tid][0].time = 22.0/24.0 + 2.0/(24.0*60.0);
	local_arg[tid][0].a_x = 0.0;
	local_arg[tid][0].a_y = 0.0;
	local_arg[tid][0].a_z = 0.0;
	local_arg[tid][0].axis_off_a = 0.0;
	local_arg[tid][0].b_x = -1995678.4969;
	local_arg[tid][0].b_y = -5037317.8209;
	local_arg[tid][0].b_z =  3357328.0825;
	local_arg[tid][0].axis_off_b = 2.1377;
	local_arg[tid][0].ra = (M_PI*2/24.0)*(19.0 + 39.0/60.0 + 38.560210/3600.0);
	local_arg[tid][0].dec = (M_PI*2/360.)*(21.0 + 34.0/60.0 + 59.141000/3600.0);
	local_arg[tid][0].dra = 0.0;
	local_arg[tid][0].ddec = 0.0;
	local_arg[tid][0].depoch = 0.0;
	local_arg[tid][0].parallax = 0.0;
	for(uint_fast32_t e=0; e < my_NUM_EOPs; ++e)
	{
		local_arg[tid][0].EOP_time[e] = 50773.0 + double(e); 
		local_arg[tid][0].tai_utc[e]  = 31.0;
	}
	local_arg[tid][0].ut1_utc[0] = 0.285033;
	local_arg[tid][0].xpole[0]   = 0.19744;
	local_arg[tid][0].ypole[0]   = 0.24531;
     
	local_arg[tid][0].ut1_utc[1] = 0.283381;
	local_arg[tid][0].xpole[1]   = 0.19565;
	local_arg[tid][0].ypole[1]   = 0.24256;
     
	local_arg[tid][0].ut1_utc[2] = 0.281678;
	local_arg[tid][0].xpole[2]   = 0.19400;
	local_arg[tid][0].ypole[2]   = 0.24000;
     
	local_arg[tid][0].ut1_utc[3] = 0.280121;
	local_arg[tid][0].xpole[3]   = 0.19244;
	local_arg[tid][0].ypole[3]   = 0.23700;
     
	local_arg[tid][0].ut1_utc[4] = 0.278435;
	local_arg[tid][0].xpole[4]   = 0.19016;
	local_arg[tid][0].ypole[4]   = 0.23414;
	
	local_arg[tid][0].pressure_a   = 0.0;
	local_arg[tid][0].pressure_b   = 0.0;
	local_arg[tid][0].station_a[0] = char('E');
	local_arg[tid][0].station_a[1] = char('C');
	local_arg[tid][0].station_a[2] = 0;
	difx_strlcpy(local_arg[tid][0].axis_type_a, "altz", DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE);
	local_arg[tid][0].station_b[0] = (char)('K');
	local_arg[tid][0].station_b[1] = (char)('P');
	local_arg[tid][0].station_b[2] = 0;
	difx_strlcpy(local_arg[tid][0].axis_type_b, "altz", DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE);
	difx_strlcpy(local_arg[tid][0].source, "B1937+21", DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE);
	return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::test_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t tid) throw()
{
	struct CALCSERVER9_1_MODEL_DELAY_RESPONSE* local_resp = &(local_res[tid][0]);
	if(std::fabs(local_resp->delay[0] - (-2.04212341289221e-02)) >= 1.0e-13)
	{
		return -1;
	}
	return 0;
}



int_fast32_t DelayHandlerCalcServer9_1::test_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t tid) throw()
{
	if(tid != 0)
	{
		return 0;
	}
	
	local_parg.request_id               = 150;
	local_parg.server_struct_setup_code = 0;
	local_parg.verbosity                = verbose;
	return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::test_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t tid) throw()
{
	if(tid != 0)
	{
		return 0;
	}
	if(std::fabs(local_pres.vlight - (299792458.0)) >= 1.0e-3)
	{
		return -1;
	}
	return 0;
}





int_fast32_t DelayHandlerCalcServer9_1::convert_to_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t tid) throw()
{
	const struct SERVER_MODEL_DELAY_ARGUMENT* const arg(reinterpret_cast<const struct SERVER_MODEL_DELAY_ARGUMENT*>(data_input));
	uint_fast32_t my_NUM_Stations = stationEnd[tid] - stationStart[tid];
	uint_fast32_t my_NUM_Sources = arg->Num_Sources;
	uint_fast32_t my_NUM = my_NUM_Stations*my_NUM_Sources;
	if(my_NUM > local_Num_Stations_Sources[tid])
	{
		local_arg[tid] = reinterpret_cast<CALCSERVER9_1_MODEL_DELAY_ARGUMENT*>(realloc(local_arg[tid],my_NUM*sizeof(CALCSERVER9_1_MODEL_DELAY_ARGUMENT)));
		local_res[tid] = reinterpret_cast<CALCSERVER9_1_MODEL_DELAY_RESPONSE*>(realloc(local_res[tid],my_NUM*sizeof(CALCSERVER9_1_MODEL_DELAY_RESPONSE)));
		if((local_arg[tid] == NULL) || (local_res[tid] == NULL))
		{
			DHBfprintf(stderr, "cannot realloc local memory for thread=%"PRIdFAST32"\n", tid);
			return -1;
		}
		local_Num_Stations_Sources[tid] = my_NUM;
	}
	local_current_Num[tid] = my_NUM;
	int_fast32_t EOP_min_loc = 0;
	int_fast32_t EOP_max_loc = 0;
	if(arg->Num_EOPs < NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS)
	{
		DHBfprintf(stderr, "Error: %s:%d: Input Num_EOPs (%"PRIu32") too small\n", __FILE__, __LINE__, arg->Num_EOPs);
		return -2;
	}
    {
        int_fast32_t best_loc = 0;
        double best_diff = 1E300;
        int_fast32_t e;
        double this_date = arg->date + arg->time;
        for(e=0; e < (int_fast32_t)arg->Num_EOPs; ++e)
        {
            double diff = fabs(arg->EOP[e].EOP_time - this_date);
            if(diff < best_diff)
            {
                best_diff = diff;
                best_loc = e;
            }
        }
        EOP_min_loc = best_loc - 2;
        if(EOP_min_loc < 0)
        {
            EOP_min_loc = 0;
        }
        EOP_max_loc = EOP_min_loc + NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS;
        if(EOP_max_loc > (int_fast32_t)arg->Num_EOPs)
        {
            EOP_max_loc = (int_fast32_t)arg->Num_EOPs;
        }
        EOP_min_loc = EOP_max_loc - NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS;
    }
	for(uint_fast32_t st=stationStart[tid]; st < stationEnd[tid]; ++st)
	{
		for(uint_fast32_t so=0; so < my_NUM_Sources; ++so)
		{
			const uint_fast32_t s = (st-stationStart[tid])*my_NUM_Sources+so;

			local_arg[tid][s].request_id = arg->request_id;
			local_arg[tid][s].date = arg->date;
			local_arg[tid][s].ref_frame = arg->ref_frame;
			if(NUM_DIFX_DELAYHANDLERCALCSERVER9_1_KFLAGS > NUM_DIFX_DELAYHANDLERDISTRIBUTOR_KFLAGS)
			{
				DHBfprintf(stderr, "Error: %s:%d: KFLAGS size problem", __FILE__, __LINE__);
				return -3;
			}
			for(uint_fast16_t k=0; k < NUM_DIFX_DELAYHANDLERCALCSERVER9_1_KFLAGS; ++k)
			{
				local_arg[tid][s].kflags[k] = arg->kflags[k];
			}
			local_arg[tid][s].time = arg->time;
			local_arg[tid][s].a_x = arg->station[0].station_pos[0];
			local_arg[tid][s].a_y = arg->station[0].station_pos[1];
			local_arg[tid][s].a_z = arg->station[0].station_pos[2];
			local_arg[tid][s].axis_off_a = arg->station[0].axis_off;
			local_arg[tid][s].b_x = arg->station[st].station_pos[0];
			local_arg[tid][s].b_y = arg->station[st].station_pos[1];
			local_arg[tid][s].b_z = arg->station[st].station_pos[2];
			local_arg[tid][s].axis_off_b = arg->station[st].axis_off;
			local_arg[tid][s].ra = arg->source[so].ra;
			local_arg[tid][s].dec = arg->source[so].dec;
			local_arg[tid][s].dra = arg->source[so].dra;
			local_arg[tid][s].ddec = arg->source[so].ddec;
			local_arg[tid][s].depoch = arg->source[so].depoch;
			local_arg[tid][s].parallax = arg->source[so].parallax;
			for(int_fast32_t e=EOP_min_loc; e < EOP_max_loc; ++e)
			{
				local_arg[tid][s].EOP_time[e-EOP_min_loc] = arg->EOP[e].EOP_time;
				local_arg[tid][s].tai_utc[e-EOP_min_loc]  = arg->EOP[e].tai_utc;
				local_arg[tid][s].ut1_utc[e-EOP_min_loc]  = arg->EOP[e].ut1_utc;
				local_arg[tid][s].xpole[e-EOP_min_loc]    = arg->EOP[e].xpole;
				local_arg[tid][s].ypole[e-EOP_min_loc]    = arg->EOP[e].ypole;
			}
			local_arg[tid][s].pressure_a   = 0.0;
			local_arg[tid][s].pressure_b   = 0.0;
			local_arg[tid][s].station_a[0] = (char)(arg->station[0].site_ID&0xFF);
			local_arg[tid][s].station_a[1] = (char)(arg->station[0].site_ID>>8);
			local_arg[tid][s].station_a[2] = 0;
			difx_strlcpy(local_arg[tid][s].axis_type_a, arg->station[0].axis_type, DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE);
			local_arg[tid][s].station_b[0] = (char)(arg->station[st].site_ID&0xFF);
			local_arg[tid][s].station_b[1] = (char)(arg->station[st].site_ID>>8);
			local_arg[tid][s].station_b[2] = 0;
			difx_strlcpy(local_arg[tid][s].axis_type_b, arg->station[st].axis_type, DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE);
			difx_strlcpy(local_arg[tid][s].source, arg->source[so].source_name, DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE);
		}
	}
	return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::convert_from_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t tid) throw()
{
	struct SERVER_MODEL_DELAY_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE*>(data_output));
	uint_fast32_t my_NUM_Sources = res->Num_Sources;

	for(uint_fast32_t st=stationStart[tid]; st < stationEnd[tid]; ++st)
	{
		for(uint_fast32_t so=0; so < my_NUM_Sources; ++so)
		{
			const uint_fast32_t sr = st*my_NUM_Sources+so;
			const uint_fast32_t sl = (st-stationStart[tid])*my_NUM_Sources+so;

			int_fast32_t retval = convert_from_local_SERVER_MODEL_DELAY_RESPONSE_N(tid, sr, sl);
			if((retval))
			{
				DHBfprintf(stderr, "Error: %s:%d: convert_from_local_SERVER_MODEL_DELAY_RESPONSE_N failed for thread=%"PRIdFAST32"\n", __FILE__, __LINE__, tid);
				return -1;
			}
		}
	}
	if(tid == 0)
	{
		// Copy over the header information
		res->rpc_handler_error        = 0;
		res->server_error             = 0;
		res->model_error              = 0;
		res->request_id = local_res[tid][0].request_id;
		res->server_struct_setup_code = 0;
		res->server_version = local_res[tid][0].server_version;
		res->date = local_res[tid][0].date;
		res->time = local_res[tid][0].time;
		DiFX::Delay::Time::DelayTimestamp t(res->date,res->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
		res->utc_second = t.i();
		res->utc_second_fraction = t.f();
		DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
		res->tai_second = t_TAI.i();
		res->tai_second_fraction = t_TAI.f();
		// Also copy over the station 0 information
		for(uint_fast32_t so=0; so < my_NUM_Sources; ++so)
		{
			int_fast32_t retval = convert_from_local_SERVER_MODEL_DELAY_RESPONSE_0(tid, so);
			if((retval))
			{
				DHBfprintf(stderr, "Error: %s:%d: convert_from_local_SERVER_MODEL_DELAY_RESPONSE_0 failed for thread=%"PRIdFAST32"\n", __FILE__, __LINE__, tid);
				return -2;
			}
		}
		// Store some information for later use.
		detailed_version_number = local_res[tid][0].server_version;
	}
	return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::convert_from_local_SERVER_MODEL_DELAY_RESPONSE_N(const int_fast32_t tid, const uint_fast32_t sr, const uint_fast32_t sl) throw()
{
	struct SERVER_MODEL_DELAY_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE*>(data_output));
	struct CALCSERVER9_1_MODEL_DELAY_RESPONSE* local_resp = &(local_res[tid][sl]);
	
	res->result[sr].delay      = local_resp->delay[0];
	res->result[sr].dry_atmos  = local_resp->dry_atmos[1];
	res->result[sr].wet_atmos  = local_resp->wet_atmos[1];
	res->result[sr].iono_atmos = fitsnan.d;
	res->result[sr].az_corr    = fitsnan.d;
	res->result[sr].el_corr    = fitsnan.d;
	res->result[sr].az_geom    = local_resp->az[1];
	res->result[sr].el_geom    = local_resp->el[1];
	res->result[sr].primary_axis_angle    = fitsnan.d;
	res->result[sr].secondary_axis_angle  = fitsnan.d;
	res->result[sr].mount_source_angle    = fitsnan.d;
	res->result[sr].station_antenna_theta = fitsnan.d;
	res->result[sr].station_antenna_phi   = fitsnan.d;
	res->result[sr].source_antenna_theta  = fitsnan.d;
	res->result[sr].source_antenna_phi    = fitsnan.d;
	res->result[sr].UVW[0]           = local_resp->UV[0];
	res->result[sr].UVW[1]           = local_resp->UV[1];
	res->result[sr].UVW[2]           = local_resp->UV[2];
	res->result[sr].baselineP2000[0] = fitsnan.d;
	res->result[sr].baselineP2000[1] = fitsnan.d;
	res->result[sr].baselineP2000[2] = fitsnan.d;
	res->result[sr].baselineV2000[0] = fitsnan.d;
	res->result[sr].baselineV2000[1] = fitsnan.d;
	res->result[sr].baselineV2000[2] = fitsnan.d;
	res->result[sr].baselineA2000[0] = fitsnan.d;
	res->result[sr].baselineA2000[1] = fitsnan.d;
	res->result[sr].baselineA2000[2] = fitsnan.d;
	return 0;
}
int_fast32_t DelayHandlerCalcServer9_1::convert_from_local_SERVER_MODEL_DELAY_RESPONSE_0(const int_fast32_t tid, const uint_fast32_t so) throw()
{
	struct SERVER_MODEL_DELAY_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE*>(data_output));
	struct CALCSERVER9_1_MODEL_DELAY_RESPONSE* local_resp = &(local_res[tid][so]);
	
	res->result[so].delay      = fitsnan.d;
	res->result[so].dry_atmos  = local_resp->dry_atmos[0];
	res->result[so].wet_atmos  = local_resp->wet_atmos[0];
	res->result[so].iono_atmos = fitsnan.d;
	res->result[so].az_corr    = fitsnan.d;
	res->result[so].el_corr    = fitsnan.d;
	res->result[so].az_geom    = local_resp->az[0];
	res->result[so].el_geom    = local_resp->el[0];
	res->result[so].primary_axis_angle    = fitsnan.d;
	res->result[so].secondary_axis_angle  = fitsnan.d;
	res->result[so].mount_source_angle    = fitsnan.d;
	res->result[so].station_antenna_theta = fitsnan.d;
	res->result[so].station_antenna_phi   = fitsnan.d;
	res->result[so].source_antenna_theta  = fitsnan.d;
	res->result[so].source_antenna_phi    = fitsnan.d;
	res->result[so].UVW[0]           = 0.0;
	res->result[so].UVW[1]           = 0.0;
	res->result[so].UVW[2]           = 0.0;
	res->result[so].baselineP2000[0] = 0.0;
	res->result[so].baselineP2000[1] = 0.0;
	res->result[so].baselineP2000[2] = 0.0;
	res->result[so].baselineV2000[0] = 0.0;
	res->result[so].baselineV2000[1] = 0.0;
	res->result[so].baselineV2000[2] = 0.0;
	res->result[so].baselineA2000[0] = 0.0;
	res->result[so].baselineA2000[1] = 0.0;
	res->result[so].baselineA2000[2] = 0.0;
	return 0;
}



int_fast32_t DelayHandlerCalcServer9_1::convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t tid) throw()
{
	if(tid != 0)
	{
		return 0;
	}
	
	const struct SERVER_MODEL_PARAMETERS_ARGUMENT* const arg(reinterpret_cast<const struct SERVER_MODEL_PARAMETERS_ARGUMENT*>(data_input));
	local_parg.request_id               = arg->request_id;
	local_parg.server_struct_setup_code = arg->server_struct_setup_code;
	local_parg.verbosity                = arg->verbosity;
	return 0;
}

int_fast32_t DelayHandlerCalcServer9_1::convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t tid) throw()
{
	if(tid != 0)
	{
		return 0;
	}
	struct SERVER_MODEL_PARAMETERS_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_PARAMETERS_RESPONSE*>(data_output));

	res->rpc_handler_error        = local_pres.rpc_handler_error;
	res->server_error             = local_pres.server_error;
	res->model_error              = local_pres.model_error;
	res->request_id               = local_pres.request_id;
	res->server_struct_setup_code = local_pres.server_struct_setup_code;
	res->server_version           = local_pres.server_version;
	res->accelgrv                 = local_pres.accelgrv;
	res->e_flat                   = local_pres.e_flat;
	res->earthrad                 = local_pres.earthrad;
	res->mmsems                   = local_pres.mmsems;
	res->ephepoc                  = local_pres.ephepoc;
	res->gauss                    = local_pres.gauss;
	res->u_grv_cn                 = local_pres.u_grv_cn;
	res->gmsun                    = local_pres.gmsun;
	res->gmmercury                = local_pres.gmmercury;
	res->gmvenus                  = local_pres.gmvenus;
	res->gmearth                  = local_pres.gmearth;
	res->gmmoon                   = local_pres.gmmoon;
	res->gmmars                   = local_pres.gmmars;
	res->gmjupiter                = local_pres.gmjupiter;
	res->gmsaturn                 = local_pres.gmsaturn;
	res->gmuranus                 = local_pres.gmuranus;
	res->gmneptune                = local_pres.gmneptune;
	res->etidelag                 = local_pres.etidelag;
	res->love_h                   = local_pres.love_h;
	res->love_l                   = local_pres.love_l;
	res->pre_data                 = local_pres.pre_data;
	res->rel_data                 = local_pres.rel_data;
	res->tidalut1                 = local_pres.tidalut1;
	res->au                       = local_pres.au;
	res->tsecau                   = local_pres.tsecau;
	res->vlight                   = local_pres.vlight;

	
	// Store some information for later use.
	detailed_version_number = local_pres.server_version;
	return 0;
}











void DelayHandlerCalcServer9_1::print_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t tid) throw()
{
	for(uint_fast32_t s=0; s < local_current_Num[tid]; ++s)
	{
		struct CALCSERVER9_1_MODEL_DELAY_ARGUMENT* l_arg = local_arg[tid]+s;
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": Delay Argument\n", tid, s);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: request_id=0x%"PRIX64"\n", tid, s, l_arg->request_id);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: date=%"PRId64" time=%16.12f ref_frame=%"PRId64"\n", tid, s, l_arg->date, l_arg->time, l_arg->ref_frame);
		if(verbose >= 4) {
			for(uint_fast8_t k=0; k < NUM_DIFX_DELAYHANDLERCALCSERVER9_1_KFLAGS; k++) {
				DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: kflag[%02"PRIu8"]=%"PRId16"\n", tid, s, k, l_arg->kflags[k]);
			}
		}
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_a='%s'\n", tid, s, l_arg->station_a);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_a axis_type='%s'\n", tid, s, l_arg->axis_type_a);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_a pos= [%14.4f, %14.4f, %14.4f]\n", tid, s, l_arg->a_x, l_arg->a_y, l_arg->a_z);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_a axis_off=%7.4f pressure=%12.3E\n", tid, s, l_arg->axis_off_a, l_arg->pressure_a);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_b='%s'\n", tid, s, l_arg->station_b);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_b axis_type='%s'\n", tid, s, l_arg->axis_type_b);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_b pos= [%14.4f, %14.4f, %14.4f]\n", tid, s, l_arg->b_x, l_arg->b_y, l_arg->b_z);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: station_b axis_off=%7.4f pressure=%12.3E\n", tid, s, l_arg->axis_off_b, l_arg->pressure_b);

		
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: source='%s'\n", tid, s, l_arg->source);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: ra=           %20.16f\n", tid, s, l_arg->ra);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: dec=          %20.16f\n", tid, s, l_arg->dec);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: dra=          %20.10f\n", tid, s, l_arg->dra);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: ddec=         %20.10f\n", tid, s, l_arg->ddec);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: depoch=       %20.16f\n", tid, s, l_arg->depoch);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: parallax=     %20.3f\n", tid, s, l_arg->parallax);
		
		for(uint_fast32_t e=0; e < NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS; ++e) {
			DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: EOP=%"PRIuFAST32" EOP_time=  %20.11f\n", tid, s, e, l_arg->EOP_time[e]);
			DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: EOP=%"PRIuFAST32" tai_utc=   %20.12f\n", tid, s, e, l_arg->tai_utc[e]);
			DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: EOP=%"PRIuFAST32" ut1_utc=   %20.12f\n", tid, s, e, l_arg->ut1_utc[e]);
			DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: EOP=%"PRIuFAST32" xpole=     %10.6f\n", tid, s, e, l_arg->xpole[e]);
			DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": request arg: EOP=%"PRIuFAST32" ypole=     %10.6f\n", tid, s, e, l_arg->ypole[e]);
		}
	}
	return;
}

void DelayHandlerCalcServer9_1::print_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t tid) throw()
{
	for(uint_fast32_t s=0; s < local_current_Num[tid]; ++s)
	{
		struct CALCSERVER9_1_MODEL_DELAY_RESPONSE* l_res = local_res[tid]+s;
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": Delay Response\n", tid, s);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: request_id=0x%"PRIX64"\n", tid, s, l_res->request_id);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: date=%"PRId64" time=%.16f\n", tid, s, l_res->date, l_res->time);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: server_version=0x%"PRIX64"\n", tid, s, l_res->server_version);		
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: delay=                [%24.16E %24.16E]\n", tid, s, l_res->delay[0], l_res->delay[1]);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: dry_atmos=            [%24.16E %24.16E %24.16E %24.16E]\n", tid, s, l_res->dry_atmos[0], l_res->dry_atmos[1], l_res->dry_atmos[2], l_res->dry_atmos[3]);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: wet_atmos=            [%24.16E %24.16E %24.16E %24.16E]\n", tid, s, l_res->wet_atmos[0], l_res->wet_atmos[1], l_res->wet_atmos[2], l_res->wet_atmos[3]);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: az=                   [%24.16E %24.16E]\n", tid, s, l_res->az[0], l_res->az[1]);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: el=                   [%24.16E %24.16E]\n", tid, s, l_res->el[0], l_res->el[1]);
		DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Num %"PRIuFAST32": response res: UV=                   [%24.16E %24.16E %24.16E]\n", tid, s, l_res->UV[0], l_res->UV[1], l_res->UV[2]);
	}
	return;
}

void DelayHandlerCalcServer9_1::print_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t tid) throw()
{
	if(tid != 0)
	{
		return;
	}
	
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Local Parameters Argument\n", tid);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": request_id=%"PRId64"\n", tid, local_parg.request_id);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": server_struct_setup_code=0x%"PRIX64"\n", tid, local_parg.server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": verbosity=%"PRId32"\n", tid, local_parg.verbosity);
	return;
}
void DelayHandlerCalcServer9_1::print_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t tid) throw()
{
	if(tid != 0)
	{
		return;
	}
	
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": Local Parameters Response\n", tid);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": handler_error=%"PRId32" rpc_handler_error=%"PRId32" server_error=%"PRId32" model_error=%"PRId32"\n", tid, local_pres.handler_error, local_pres.rpc_handler_error, local_pres.server_error, local_pres.model_error);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": request_id=%"PRId64"\n", tid, local_pres.request_id);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": server_struct_setup_code=0x%"PRIX64"\n", tid, local_pres.server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": server_version=0x%"PRIX64"\n", tid, local_pres.server_version);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": accelgrv =%25.16E\n", tid, local_pres.accelgrv);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": e_flat   =%25.16E\n", tid, local_pres.e_flat);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": earthrad =%25.16E\n", tid, local_pres.earthrad);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": mmsems   =%25.16E\n", tid, local_pres.mmsems);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": ephepoc  =%25.16E\n", tid, local_pres.ephepoc);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gauss    =%25.16E\n", tid, local_pres.gauss);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": u_grv_cn =%25.16E\n", tid, local_pres.u_grv_cn);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmsun    =%25.16E\n", tid, local_pres.gmsun);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmmercury=%25.16E\n", tid, local_pres.gmmercury);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmvenus  =%25.16E\n", tid, local_pres.gmvenus);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmearth  =%25.16E\n", tid, local_pres.gmearth);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmmoon   =%25.16E\n", tid, local_pres.gmmoon);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmmars   =%25.16E\n", tid, local_pres.gmmars);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmjupiter=%25.16E\n", tid, local_pres.gmjupiter);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmsaturn =%25.16E\n", tid, local_pres.gmsaturn);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmuranus =%25.16E\n", tid, local_pres.gmuranus);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": gmneptune=%25.16E\n", tid, local_pres.gmneptune);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": etidelag =%25.16E\n", tid, local_pres.etidelag);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": love_h   =%25.16E\n", tid, local_pres.love_h);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": love_l   =%25.16E\n", tid, local_pres.love_l);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": pre_data =%25.16E\n", tid, local_pres.pre_data);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": rel_data =%25.16E\n", tid, local_pres.rel_data);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": tidalut1 =%25.16E\n", tid, local_pres.tidalut1);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": au       =%25.16E\n", tid, local_pres.au);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": tsecau   =%25.16E\n", tid, local_pres.tsecau);
	DHBfprintf(stdout, "DelayHandlerCalcServer9_1: Thread %"PRIdFAST32": vlight   =%25.16E\n", tid, local_pres.vlight);
	return;
}



}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX


