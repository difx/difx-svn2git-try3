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

#include "DelayHandlerDiFXDelayServer.h"
#include "DiFX_Delay_Server.h"
#include "DelayTimestamp.h"
#include "difxio/difx_input.h"
#include "DiFX_Delay_Server.h"

namespace DiFX {
namespace Delay {
namespace Handler {






// GLOBALS

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

DelayHandlerDiFXDelayServer::DelayHandlerDiFXDelayServer(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_, const char* const delayServerHost, const uint64_t delayServer_) throw()
		:
		DelayHandlerBase(NUM_THREADS_, verbosity_),
		clnt(NULL),
		delayServer(delayServer_),
		local_Num_Stations(0),
		local_Num_Sources(0),
		local_Num_EOPs(0),
		local_station(NULL),
		local_source(NULL),
		local_EOP(NULL)
{
	int_fast32_t retval;
	retval = init_threads();
	if((retval))
	{
		overall_status = delayHandlerStatus_HandlerError;
		return;
	}
	retval = create_RPC_client(delayServerHost);
	if((retval))
	{
		overall_status = delayHandlerStatus_HandlerError;
		return;
	}
	return;
}

    
DelayHandlerDiFXDelayServer::~DelayHandlerDiFXDelayServer()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running ~DelayHandlerDiFXDelayServer\n");
	}
	stop_threads();
	destroy_RPC_client();
	dealloc_memory();
	return;
}







int_fast32_t DelayHandlerDiFXDelayServer::init_NUM_THREADS() throw()
{
	NUM_THREADS = 1;
	return 0;
}

 
void DelayHandlerDiFXDelayServer::dealloc_memory() throw()
{
	free(reinterpret_cast<void*>(local_station)); local_station=NULL;
	free(reinterpret_cast<void*>(local_source)); local_source=NULL;
	free(reinterpret_cast<void*>(local_EOP)); local_EOP=NULL;
	return;
}




int_fast32_t DelayHandlerDiFXDelayServer::create_RPC_client(const char* const delayServerHost) throw()
{
	static const char localhost[] = "localhost";
	if(verbosity > 1)
	{
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer::create_RPC_client attempting to create RPC client with host='%s' handler=0x%lX version=0x%lX\n", delayServerHost, delayHandler, delayVersion);
	}
	const char* host = delayServerHost;
	if((host == NULL) || (host[0] == 0))
	{
		if(verbosity > 1)
		{
			fprintf(stderr, "DelayHandlerDiFXDelayServer::create_RPC_client: Warning: input delayServerHost is NULL or empty.  Will check the DIFX_DELAY_SERVER environment variable.\n");
		}
		host = getenv("DIFX_DELAY_SERVER");
		if(host == NULL)
		{
			host = localhost;
			if(verbosity > -1)
			{
				fprintf(stderr, "DelayHandlerDiFXDelayServer::create_RPC_client: Warning: DIFX_DELAY_SERVER environment variable is not set.  Defaulting to \"%s\".\n", host);
			}
		}
	}
	clnt = clnt_create(host, delayHandler, delayVersion, "tcp");
	if(!clnt)
	{
		clnt_pcreateerror(delayServerHost);
		DHBfprintf(stderr, "Error: DelayHandlerDiFXDelayServer::create_RPC_client: RPC clnt_create fails for host : '%s' program id 0x%lX version %lu\n", host, delayHandler, delayVersion);
		return -1;
	}
	if(verbosity > 1)
	{
		DHBfprintf(stdout, "RPC client created\n");
	}
	return 0;
}
int_fast32_t DelayHandlerDiFXDelayServer::destroy_RPC_client() throw()
{
	if((clnt))
	{
		if(verbosity > 1)
		{
			fprintf(stdout, "DelayHandlerDiFXDelayServer::destroy_RPC_client attempting to destroy RPC client\n");
		}
		clnt_destroy(clnt); clnt=NULL;
		if(verbosity > 1)
		{
			fprintf(stdout, "RPC client destroyed\n");
		}
	}
	return 0;
}



namespace {
static struct timeval TIMEOUT = {25, 0};
}

int_fast32_t DelayHandlerDiFXDelayServer::callCalcDelay() throw()
{
	int_fast32_t retval = 0;
    enum clnt_stat clnt_stat;
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerDiFXDelayServer calling delay server for delay\n");
    }
    memset(&local_res, 0, sizeof(struct getDIFX_DELAY_SERVER_1_res));
    
    clnt_stat = clnt_call(clnt, GETDIFX_DELAY_SERVER,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_1_arg, 
                          (caddr_t)(&local_arg),
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_1_res, 
                          (caddr_t)(&local_res),
                          TIMEOUT);
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "Returned from clnt_call\n");
    }
    if(clnt_stat != RPC_SUCCESS)
    {
        DHBfprintf(stderr, "clnt_call failed %d!\n", int(clnt_stat));
        retval = -1;
    }
    if(local_res.this_error)
    {
        DHBfprintf(stderr,"Error: callCalc: %s\n", local_res.getDIFX_DELAY_SERVER_1_res_u.errmsg);
        retval = -2;
    }
    return retval;
}
int_fast32_t DelayHandlerDiFXDelayServer::callCalcParameters() throw()
{
	int_fast32_t retval = 0;
    enum clnt_stat clnt_stat;
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerDiFXDelayServer calling delay server for parameters\n");
    }
    memset(&local_pres, 0, sizeof(struct getDIFX_DELAY_SERVER_PARAMETERS_1_res));
    
    clnt_stat = clnt_call(clnt, GETDIFX_DELAY_SERVER_PARAMETERS,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_arg, 
                          (caddr_t)(&local_parg),
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_res, 
                          (caddr_t)(&local_pres),
                          TIMEOUT);
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "Returned from clnt_call\n");
    }
    if(clnt_stat != RPC_SUCCESS)
    {
	    DHBfprintf(stderr, "clnt_call failed with %d!\n", int(clnt_stat));
        retval = -1;
    }
    if(local_pres.this_error)
    {
        DHBfprintf(stderr,"Error: callCalc: %s\n", local_pres.getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.errmsg);
        retval = -2;
    }
    return retval;
}

int_fast32_t DelayHandlerDiFXDelayServer::freeCalcDelayResults() throw()
{
	int_fast32_t retval = 0;
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerDiFXDelayServer calling clnt_freeres for delay\n");
    }
    if(clnt_freeres(clnt, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) &local_res) != 1)
    {
        DHBfprintf(stderr, "Failed to free delay response buffer\n");
        retval = -1;
    }
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "Return from clnt_freeres\n");
    }
    return retval;
}

int_fast32_t DelayHandlerDiFXDelayServer::freeCalcParametersResults() throw()
{
	int_fast32_t retval = 0;
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "DelayHandlerDiFXDelayServer calling clnt_freeres for parameters\n");
    }
    if(clnt_freeres(clnt, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_res, (caddr_t) &local_pres) != 1)
    {
        DHBfprintf(stderr, "Failed to free parameters response buffer\n");
        retval = -1;
    }
    if((verbose>=3))
    {
	    DHBfprintf(stdout, "Return from clnt_freeres\n");
    }
    return retval;
}




                      


int_fast32_t DelayHandlerDiFXDelayServer::do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw()
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



int_fast32_t DelayHandlerDiFXDelayServer::do_thread_init(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_init\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_init_error;
	}
do_thread_init_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_init with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerDiFXDelayServer::do_thread_test_delays(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_test_delays\n", tid);
	}
	if(tid != 0)
	{
		goto do_thread_test_delays_error;
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_test_delays_error;
	}
	for(int_fast32_t t=0; /* No check */; ++t)
	{
		if(verbosity >= 5)
		{
			DHBfprintf(stdout, "thread %"PRIdFAST32" testing delay server for case %"PRIdFAST32"\n", tid, t);
		}
		retval = test_local_SERVER_MODEL_DELAY_ARGUMENT(t);
		if(retval > 0)
		{
			// no more tests to be done
			break;
		}
		if((retval))
		{
			DHBfprintf(stderr, "test_local_SERVER_MODEL_DELAY_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			retval_final = -2;
			goto do_thread_test_delays_error;
		}
		if(verbosity >= 5)
		{
			print_local_SERVER_MODEL_DELAY_ARGUMENT();
		}
		retval = callCalcDelay();
		if((retval))
		{
			DHBfprintf(stderr, "callCalcDelay failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			retval_final = -3;
			goto do_thread_test_delays_error;
		}
		if(verbosity >= 5)
		{
			print_local_SERVER_MODEL_DELAY_RESPONSE();
		}
		retval = test_local_SERVER_MODEL_DELAY_RESPONSE(t);
		if((retval))
		{
			DHBfprintf(stderr, "test_local_SERVER_MODEL_DELAY_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			freeCalcDelayResults();
			retval_final = -4;
			goto do_thread_test_delays_error;
		}
		retval = freeCalcDelayResults();
		if((retval))
		{
			DHBfprintf(stderr, "freeCalcDelayResults failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			retval_final = -5;
			goto do_thread_test_delays_error;
		}
	}
do_thread_test_delays_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_test_delays with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerDiFXDelayServer::do_thread_test_parameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_test_parameters\n", tid);
		fflush(stdout);
	}
	if(tid != 0)
	{
		goto do_thread_test_parameters_error;
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_test_parameters_error;
	}
	for(int_fast32_t t=0; /* No check */; ++t)
	{
		if(verbosity >= 5)
		{
			DHBfprintf(stdout, "thread %"PRIdFAST32" testing delay server for case %"PRIdFAST32"\n", tid, t);
			fflush(stdout);
		}
		retval = test_local_SERVER_MODEL_PARAMETERS_ARGUMENT(t);
		if(retval > 0)
		{
			// no more tests to be done
			break;
		}
		if((retval))
		{
			DHBfprintf(stderr, "test_local_SERVER_MODEL_PARAMETERS_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			retval_final = -2;
			goto do_thread_test_parameters_error;
		}
		if(verbosity >= 5)
		{
			print_local_SERVER_MODEL_PARAMETERS_ARGUMENT();
			fflush(stdout);
			fflush(stderr);
		}
		retval = callCalcParameters();
		if((retval))
		{
			DHBfprintf(stderr, "callCalcParameters failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			retval_final = -3;
			goto do_thread_test_parameters_error;
		}
		if(verbosity >= 5)
		{
			print_local_SERVER_MODEL_PARAMETERS_RESPONSE();
			fflush(stdout);
			fflush(stderr);
		}
		retval = test_local_SERVER_MODEL_PARAMETERS_RESPONSE(t);
		if((retval))
		{
			DHBfprintf(stderr, "test_local_SERVER_MODEL_PARAMETERS_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			freeCalcParametersResults();
			retval_final = -4;
			goto do_thread_test_parameters_error;
		}
		retval = freeCalcParametersResults();
		if((retval))
		{
			DHBfprintf(stderr, "freeCalcParametersResults failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
			retval_final = -5;
			goto do_thread_test_parameters_error;
		}
	}
do_thread_test_parameters_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_test_parameters with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerDiFXDelayServer::do_thread_get_delays(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_get_delays\n", tid);
	}
	if(tid != 0)
	{
		goto do_thread_get_delays_error;
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_get_delays_error;
	}
	retval = convert_to_local_SERVER_MODEL_DELAY_ARGUMENT();
	if((retval))
	{
		DHBfprintf(stderr, "convert_to_local_SERVER_MODEL_DELAY_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -2;
		goto do_thread_get_delays_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_DELAY_ARGUMENT();
	}
	retval = callCalcDelay();
	if((retval))
	{
		DHBfprintf(stderr, "callCalcDelay failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -3;
		goto do_thread_get_delays_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_DELAY_RESPONSE();
	}
	retval = convert_from_local_SERVER_MODEL_DELAY_RESPONSE();
	if((retval))
	{
		DHBfprintf(stderr, "convert_from_local_SERVER_MODEL_DELAY_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		freeCalcDelayResults();
		retval_final = -4;
		goto do_thread_get_delays_error;
	}
	retval = freeCalcDelayResults();
	if((retval))
	{
		DHBfprintf(stderr, "freeCalcDelayResults failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
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


int_fast32_t DelayHandlerDiFXDelayServer::do_thread_get_parameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	int_fast32_t retval;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_get_parameters\n", tid);
	}
	if(tid != 0)
	{
		goto do_thread_get_parameters_error;
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
		retval_final = -1;
		goto do_thread_get_parameters_error;
	}
	retval = convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT();
	if((retval))
	{
		DHBfprintf(stderr, "convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -2;
		goto do_thread_get_parameters_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_PARAMETERS_ARGUMENT();
	}
	retval = callCalcParameters();
	if((retval))
	{
		DHBfprintf(stderr, "callCalcParameters failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		retval_final = -3;
		goto do_thread_get_parameters_error;
	}
	if(verbosity >= 5)
	{
		print_local_SERVER_MODEL_PARAMETERS_RESPONSE();
	}
	retval = convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE();
	if((retval))
	{
		DHBfprintf(stderr, "convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
		freeCalcParametersResults();
		retval_final = -4;
		goto do_thread_get_parameters_error;
	}
	retval = freeCalcParametersResults();
	if((retval))
	{
		DHBfprintf(stderr, "freeCalcParametersResults failed for thread=%"PRIdFAST32" with %"PRIdFAST32"\n", tid, retval);
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




int_fast32_t DelayHandlerDiFXDelayServer::do_thread_stop(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_stop\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
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


int_fast32_t DelayHandlerDiFXDelayServer::do_thread_panic(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_panic\n", tid);
	}
	if(overall_status >= delayHandlerStatus_HandlerStopped)
	{
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





int_fast32_t DelayHandlerDiFXDelayServer::test_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t t) throw()
{
	if(t < 0)
	{
		return -1;
	}
	else if(t >= 4)
	{
		return +1;
	}
	uint_fast32_t Num_Stations = 2;
	uint_fast32_t Num_Sources  = 1;
	uint_fast32_t Num_EOPs     = 5;

	switch(t) {
	case 0:
		local_arg.request_id = long(150);
		local_arg.delay_server = u_long(delayServerTypeIds[CALCServer]);
		local_arg.server_struct_setup_code = long(0);
		break;
	case 1:
		local_arg.request_id = long(150);
		local_arg.delay_server = u_long(delayServerTypeIds[CALC_9_1_RA_Server]);
		local_arg.server_struct_setup_code = long(0x0);
		break;
	case 2:
		local_arg.request_id = long(150);
		local_arg.delay_server = u_long(delayServerTypeIds[CALC_9_1_RA_Server]);
		local_arg.server_struct_setup_code = long(150);
		break;
	case 3:
		local_arg.request_id = long(150);
		local_arg.delay_server = u_long(delayServerTypeIds[CALC_9_1_RA_Server]);
		local_arg.server_struct_setup_code = long(0x0510);
		break;
	default:
		return -2;
	}
	local_arg.date = (50774);
	local_arg.ref_frame = long(0);
	local_arg.verbosity = int(verbose);
	for(uint_fast16_t k=0; k < NUM_DIFX_DELAY_SERVER_1_KFLAGS; ++k)
	{
		local_arg.kflags[k] = short(-1);
	}
	local_arg.time = (22.0/24.0 + 2.0/(24.0*60.0));
	local_arg.sky_frequency = (10.E9);
	local_arg.Use_Server_Station_Table = int(0);
	local_arg.Use_Server_Source_Table  = int(0);
	local_arg.Use_Server_EOP_Table     = int(0);
	local_arg.Num_Stations = u_int(Num_Stations);
	if(Num_Stations > local_Num_Stations)
	{
		local_station = reinterpret_cast<DIFX_DELAY_SERVER_1_station*>(realloc(local_station, Num_Stations*sizeof(DIFX_DELAY_SERVER_1_station)));
		if(local_station == NULL)
		{
			return -3;
		}
		local_Num_Stations = Num_Stations;
	}
	local_arg.station.station_len = u_int(Num_Stations);
	local_arg.station.station_val = local_station;
	// Station 0
	{
		difx_strlcpy(local_arg.station.station_val[0].station_name, "EC", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[0].antenna_name, "EC", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[0].site_name,    "EC", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[0].site_ID = u_short((unsigned short)('E') | ((unsigned short)('C') << 8));
		difx_strlcpy(local_arg.station.station_val[0].site_type, "fixed", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[0].axis_type, "altz",  DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[0].station_pos.x = 0.0;
		local_arg.station.station_val[0].station_pos.y = 0.0;
		local_arg.station.station_val[0].station_pos.z = 0.0;
		local_arg.station.station_val[0].station_vel.x = 0.0;
		local_arg.station.station_val[0].station_vel.y = 0.0;
		local_arg.station.station_val[0].station_vel.z = 0.0;
		local_arg.station.station_val[0].station_acc.x = 0.0;
		local_arg.station.station_val[0].station_acc.y = 0.0;
		local_arg.station.station_val[0].station_acc.z = 0.0;
		local_arg.station.station_val[0].station_pointing_dir.x  = 0.0;
		local_arg.station.station_val[0].station_pointing_dir.y  = 0.0;
		local_arg.station.station_val[0].station_pointing_dir.z  = 0.0;
		local_arg.station.station_val[0].station_reference_dir.x = 0.0;
		local_arg.station.station_val[0].station_reference_dir.y = 0.0;
		local_arg.station.station_val[0].station_reference_dir.z = 0.0;
		difx_strlcpy(local_arg.station.station_val[0].station_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameITRF2008], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[0].pointing_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameJ2000], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[0].pointing_corrections_applied  = int(0);
		local_arg.station.station_val[0].station_position_delay_offset = (0.0);
		local_arg.station.station_val[0].axis_off            = (0.0);
		local_arg.station.station_val[0].primary_axis_wrap   = int(0);
		local_arg.station.station_val[0].secondary_axis_wrap = int(0);
		difx_strlcpy(local_arg.station.station_val[0].receiver_name, "", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[0].pressure         = (0.0);
		local_arg.station.station_val[0].antenna_pressure = (0.0);
		local_arg.station.station_val[0].temperature      = (0.0);
		local_arg.station.station_val[0].wind_speed       = (fitsnan.d);
		local_arg.station.station_val[0].wind_direction   = (fitsnan.d);
		local_arg.station.station_val[0].antenna_phys_temperature = (0.0);
	}
	// Station 1
	switch(t) {
	case 0:
	case 1:
	case 2:
		{
			difx_strlcpy(local_arg.station.station_val[1].station_name, "KP", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].antenna_name, "KP", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].site_name,    "KP", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].site_ID = u_short((unsigned short)('K') | ((unsigned short)('P') << 8));
			difx_strlcpy(local_arg.station.station_val[1].site_type, "fixed", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].axis_type, "altz",  DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].station_pos.x =     -1995678.4969;
			local_arg.station.station_val[1].station_pos.y =     -5037317.8209;
			local_arg.station.station_val[1].station_pos.z =      3357328.0825;
			local_arg.station.station_val[1].station_vel.x = 0.0;
			local_arg.station.station_val[1].station_vel.y = 0.0;
			local_arg.station.station_val[1].station_vel.z = 0.0;
			local_arg.station.station_val[1].station_acc.x = 0.0;
			local_arg.station.station_val[1].station_acc.y = 0.0;
			local_arg.station.station_val[1].station_acc.z = 0.0;
			local_arg.station.station_val[1].station_pointing_dir.x  = 0.0;
			local_arg.station.station_val[1].station_pointing_dir.y  = 0.0;
			local_arg.station.station_val[1].station_pointing_dir.z  = 0.0;
			local_arg.station.station_val[1].station_reference_dir.x = 0.0;
			local_arg.station.station_val[1].station_reference_dir.y = 0.0;
			local_arg.station.station_val[1].station_reference_dir.z = 0.0;
			difx_strlcpy(local_arg.station.station_val[1].station_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameITRF2008], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].pointing_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameJ2000], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].pointing_corrections_applied  = int(0);
			local_arg.station.station_val[1].station_position_delay_offset = (0.0);
			local_arg.station.station_val[1].axis_off            = (2.1377);
			local_arg.station.station_val[1].primary_axis_wrap   = int(0);
			local_arg.station.station_val[1].secondary_axis_wrap = int(0);
			difx_strlcpy(local_arg.station.station_val[1].receiver_name, "", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].pressure         = (0.0);
			local_arg.station.station_val[1].antenna_pressure = (0.0);
			local_arg.station.station_val[1].temperature      = (0.0);
			local_arg.station.station_val[1].wind_speed       = (fitsnan.d);
			local_arg.station.station_val[1].wind_direction   = (fitsnan.d);
			local_arg.station.station_val[1].antenna_phys_temperature = (0.0);
		}
		break;
	case 3:
		{
			difx_strlcpy(local_arg.station.station_val[1].station_name, "RA", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].antenna_name, "RA", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].site_name,    "RA", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].site_ID = u_short((unsigned short)('R') | ((unsigned short)('A') << 8));
			difx_strlcpy(local_arg.station.station_val[1].site_type, "earth_orbit", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].axis_type, "space",  DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].station_pos.x =   3.376890878000000e+07;
			local_arg.station.station_val[1].station_pos.y =  -2.575678163900000e+07;
			local_arg.station.station_val[1].station_pos.z =   9.492685457100001e+07;
			local_arg.station.station_val[1].station_vel.x =  -2.644577860000000e+02;
			local_arg.station.station_val[1].station_vel.y =   1.117674795000000e+03;
			local_arg.station.station_val[1].station_vel.z =   2.011362193000000e+03;
			local_arg.station.station_val[1].station_acc.x =  -1.196600E-02;
			local_arg.station.station_val[1].station_acc.y =   9.138000E-03;
			local_arg.station.station_val[1].station_acc.z =  -3.365000E-02;
			local_arg.station.station_val[1].station_pointing_dir.x  = 0.0;
			local_arg.station.station_val[1].station_pointing_dir.y  = 0.0;
			local_arg.station.station_val[1].station_pointing_dir.z  = 0.0;
			local_arg.station.station_val[1].station_reference_dir.x = 0.0;
			local_arg.station.station_val[1].station_reference_dir.y = 0.0;
			local_arg.station.station_val[1].station_reference_dir.z = 0.0;
			difx_strlcpy(local_arg.station.station_val[1].station_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameJ2000_Earth], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			difx_strlcpy(local_arg.station.station_val[1].pointing_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameJ2000], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].pointing_corrections_applied  = int(0);
			local_arg.station.station_val[1].station_position_delay_offset = (0.0);
			local_arg.station.station_val[1].axis_off            = (2.1377);
			local_arg.station.station_val[1].primary_axis_wrap   = int(0);
			local_arg.station.station_val[1].secondary_axis_wrap = int(0);
			difx_strlcpy(local_arg.station.station_val[1].receiver_name, "", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
			local_arg.station.station_val[1].pressure         = (0.0);
			local_arg.station.station_val[1].antenna_pressure = (0.0);
			local_arg.station.station_val[1].temperature      = (0.0);
			local_arg.station.station_val[1].wind_speed       = (fitsnan.d);
			local_arg.station.station_val[1].wind_direction   = (fitsnan.d);
			local_arg.station.station_val[1].antenna_phys_temperature = (0.0);
		}
		break;
	default:
		return -4;
	}
	local_arg.Num_Sources = u_int(Num_Sources);
	if(Num_Sources > local_Num_Sources)
	{
		local_source = reinterpret_cast<DIFX_DELAY_SERVER_1_source*>(realloc(local_source, Num_Sources*sizeof(DIFX_DELAY_SERVER_1_source)));
		if(local_source == NULL)
		{
			return -5;
		}
		local_Num_Sources = Num_Sources;
	}
	local_arg.source.source_len = u_int(Num_Sources);
	local_arg.source.source_val = local_source;
	// Source 0;
	{
		difx_strlcpy(local_arg.source.source_val[0].source_name, "B1937+21", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.source.source_val[0].IAU_name,    "B1937+21", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.source.source_val[0].source_type, "star", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.source.source_val[0].ra       = ((2.0*M_PI/24.0)*(19.0 + 39.0/60.0 + 38.560210/3600.0));
		local_arg.source.source_val[0].dec      = ((2.0*M_PI/360.)*(21.0 + 34.0/60.0 + 59.141000/3600.0));
		local_arg.source.source_val[0].dra      = (0.0);
		local_arg.source.source_val[0].ddec     = (0.0);
		local_arg.source.source_val[0].depoch   = (0.0);
		local_arg.source.source_val[0].parallax = (0.0);
		difx_strlcpy(local_arg.source.source_val[0].coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameJ2000], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.source.source_val[0].source_pos.x = (0.0);
		local_arg.source.source_val[0].source_pos.y = (0.0);
		local_arg.source.source_val[0].source_pos.z = (0.0);
		local_arg.source.source_val[0].source_vel.x = (0.0);
		local_arg.source.source_val[0].source_vel.y = (0.0);
		local_arg.source.source_val[0].source_vel.z = (0.0);
		local_arg.source.source_val[0].source_acc.x = (0.0);
		local_arg.source.source_val[0].source_acc.y = (0.0);
		local_arg.source.source_val[0].source_acc.z = (0.0);
		local_arg.source.source_val[0].source_pointing_dir.x           = (0.0);
		local_arg.source.source_val[0].source_pointing_dir.y           = (0.0);
		local_arg.source.source_val[0].source_pointing_dir.z           = (0.0);
		local_arg.source.source_val[0].source_pointing_reference_dir.x = (0.0);
		local_arg.source.source_val[0].source_pointing_reference_dir.y = (0.0);
		local_arg.source.source_val[0].source_pointing_reference_dir.z = (0.0);
	}
	local_arg.Num_EOPs = u_int(Num_EOPs);
	if(Num_EOPs > local_Num_EOPs)
	{
		local_EOP = reinterpret_cast<DIFX_DELAY_SERVER_1_EOP*>(realloc(local_EOP, Num_EOPs*sizeof(DIFX_DELAY_SERVER_1_EOP)));
		if(local_EOP == NULL)
		{
			return -3;
		}
		local_Num_EOPs = Num_EOPs;
	}
	local_arg.EOP.EOP_len = u_int(Num_EOPs);
	local_arg.EOP.EOP_val = local_EOP;
	for(uint_fast32_t e=0; e < Num_EOPs; ++e)
	{
		local_arg.EOP.EOP_val[e].EOP_time = (50773.0 + double(e));
		local_arg.EOP.EOP_val[e].tai_utc  = (31.0);
	}
	
	local_arg.EOP.EOP_val[0].ut1_utc  = (0.285033);
	local_arg.EOP.EOP_val[0].xpole    = (0.19744);
	local_arg.EOP.EOP_val[0].ypole    = (0.24531);
	
	local_arg.EOP.EOP_val[1].ut1_utc  = (0.283381);
	local_arg.EOP.EOP_val[1].xpole    = (0.19565);
	local_arg.EOP.EOP_val[1].ypole    = (0.24256);
	
	local_arg.EOP.EOP_val[2].ut1_utc  = (0.281678);
	local_arg.EOP.EOP_val[2].xpole    = (0.19400);
	local_arg.EOP.EOP_val[2].ypole    = (0.24000);
	
	local_arg.EOP.EOP_val[3].ut1_utc  = (0.280121);
	local_arg.EOP.EOP_val[3].xpole    = (0.19244);
	local_arg.EOP.EOP_val[3].ypole    = (0.23700);
	
	local_arg.EOP.EOP_val[4].ut1_utc  = (0.278435);
	local_arg.EOP.EOP_val[4].xpole    = (0.19016);
	local_arg.EOP.EOP_val[4].ypole    = (0.23414);
	return 0;
}

int_fast32_t DelayHandlerDiFXDelayServer::test_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t t) throw()
{
	if(local_res.this_error != 0)
	{
		return -1;
	}
	const struct DIFX_DELAY_SERVER_1_res* const local_resp = &(local_res.getDIFX_DELAY_SERVER_1_res_u.response);
	if((local_resp->delay_server_error < 0)
	  || (local_resp->server_error < 0)
	  || (local_resp->model_error < 0)
	   )
	{
		return -2;
	}
	switch(t) {
	case 0:
		{
			if(std::fabs(local_resp->result.result_val[1].delay - (-2.04212341289221e-02)) >= 1.0e-13)
			{
				return -3;
			}
		}
		break;
	case 1:
		{
			if(local_resp->server_struct_setup_code < 0x0510)
			{
				return -3;
			}
		}
		break;
	case 2:
		{
			if(std::fabs(local_resp->result.result_val[1].delay - (-2.04212341289221e-02)) >= 1.0e-13)
			{
				return -3;
			}
		}
		break;
	case 3:
		{
			if(std::fabs(local_resp->result.result_val[1].delay - (-2.3306155663663350e-01)) >= 1.0e-13)
			{
				return -3;
			}
		}
		break;
	default:
		return -4;
	}
	return 0;
}



int_fast32_t DelayHandlerDiFXDelayServer::test_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t t) throw()
{
	if(t < 0)
	{
		return -1;
	}
	else if(t >= 4)
	{
		return +1;
	}

	switch(t) {
	case 0:
		local_parg.request_id = long(150);
		local_parg.delay_server = u_long(delayServerTypeIds[CALCServer]);
		local_parg.server_struct_setup_code = long(0);
		break;
	case 1:
		local_parg.request_id = long(150);
		local_parg.delay_server = u_long(delayServerTypeIds[CALC_9_1_RA_Server]);
		local_parg.server_struct_setup_code = long(0x0);
		break;
	case 2:
		local_parg.request_id = long(150);
		local_parg.delay_server = u_long(delayServerTypeIds[CALC_9_1_RA_Server]);
		local_parg.server_struct_setup_code = long(150);
		break;
	case 3:
		local_parg.request_id = long(150);
		local_parg.delay_server = u_long(delayServerTypeIds[CALC_9_1_RA_Server]);
		local_parg.server_struct_setup_code = long(0x0510);
		break;
	default:
		return -2;
	}
	local_parg.verbosity = int(verbose);
	return 0;
}

int_fast32_t DelayHandlerDiFXDelayServer::test_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t t) throw()
{
	if(local_pres.this_error != 0)
	{
		return -1;
	}
	const struct DIFX_DELAY_SERVER_PARAMETERS_1_res* const local_resp = &(local_pres.getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.response);
	if((local_resp->delay_server_error < 0)
	  || (local_resp->server_error < 0)
	  || (local_resp->model_error < 0)
	   )
	{
		return -2;
	}
	switch(t) {
	case 0:
	case 1:
	case 2:
	case 3:
		{
			if(std::fabs(local_resp->vlight - (299792458.0)) >= 1.0e-3)
			{
				return -3;
			}
		}
		break;
	default:
		return -4;
	}
	return 0;
}

int_fast32_t DelayHandlerDiFXDelayServer::convert_to_local_SERVER_MODEL_DELAY_ARGUMENT() throw()
{
	const struct SERVER_MODEL_DELAY_ARGUMENT* const arg(reinterpret_cast<const struct SERVER_MODEL_DELAY_ARGUMENT*>(data_input));

	local_arg.request_id = long(arg->request_id);
	local_arg.delay_server = u_long(delayServer);
	local_arg.server_struct_setup_code = long(arg->server_struct_setup_code);
	local_arg.date = (arg->date);
	local_arg.ref_frame = long(arg->ref_frame);
	local_arg.verbosity = int(verbose);
	for(uint_fast16_t k=0; k < NUM_DIFX_DELAY_SERVER_1_KFLAGS; ++k)
	{
		local_arg.kflags[k] = short(arg->kflags[k]);
	}
	local_arg.time = (arg->time);
	local_arg.sky_frequency = (arg->sky_frequency);
	local_arg.Use_Server_Station_Table = int(arg->Use_Server_Station_Table);
	local_arg.Use_Server_Source_Table  = int(arg->Use_Server_Source_Table);
	local_arg.Use_Server_EOP_Table     = int(arg->Use_Server_EOP_Table);
	local_arg.Num_Stations = u_int(arg->Num_Stations);
	if(arg->Num_Stations > local_Num_Stations)
	{
		local_station = reinterpret_cast<DIFX_DELAY_SERVER_1_station*>(realloc(local_station, arg->Num_Stations*sizeof(DIFX_DELAY_SERVER_1_station)));
		if(local_station == NULL)
		{
			return -1;
		}
		local_Num_Stations = arg->Num_Stations;
	}
	local_arg.station.station_len = u_int(arg->Num_Stations);
	local_arg.station.station_val = local_station;
	for(uint_fast32_t s=0; s < arg->Num_Stations; ++s)
	{
		difx_strlcpy(local_arg.station.station_val[s].station_name, arg->station[s].station_name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[s].antenna_name, arg->station[s].antenna_name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[s].site_name, arg->station[s].site_name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[s].site_ID = u_short(arg->station[s].site_ID);
		difx_strlcpy(local_arg.station.station_val[s].site_type, arg->station[s].site_type, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[s].axis_type, arg->station[s].axis_type, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[s].station_pos.x = arg->station[s].station_pos[0];
		local_arg.station.station_val[s].station_pos.y = arg->station[s].station_pos[1];
		local_arg.station.station_val[s].station_pos.z = arg->station[s].station_pos[2];
		local_arg.station.station_val[s].station_vel.x = arg->station[s].station_vel[0];
		local_arg.station.station_val[s].station_vel.y = arg->station[s].station_vel[1];
		local_arg.station.station_val[s].station_vel.z = arg->station[s].station_vel[2];
		local_arg.station.station_val[s].station_acc.x = arg->station[s].station_acc[0];
		local_arg.station.station_val[s].station_acc.y = arg->station[s].station_acc[1];
		local_arg.station.station_val[s].station_acc.z = arg->station[s].station_acc[2];
		local_arg.station.station_val[s].station_pointing_dir.x  = arg->station[s].station_pointing_dir[0];
		local_arg.station.station_val[s].station_pointing_dir.y  = arg->station[s].station_pointing_dir[1];
		local_arg.station.station_val[s].station_pointing_dir.z  = arg->station[s].station_pointing_dir[2];
		local_arg.station.station_val[s].station_reference_dir.x = arg->station[s].station_reference_dir[0];
		local_arg.station.station_val[s].station_reference_dir.y = arg->station[s].station_reference_dir[1];
		local_arg.station.station_val[s].station_reference_dir.z = arg->station[s].station_reference_dir[2];
		difx_strlcpy(local_arg.station.station_val[s].station_coord_frame, sourceCoordinateFrameTypeNames[arg->station[s].station_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.station.station_val[s].pointing_coord_frame, sourceCoordinateFrameTypeNames[arg->station[s].pointing_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[s].pointing_corrections_applied  = int(arg->station[s].pointing_corrections_applied);
		local_arg.station.station_val[s].station_position_delay_offset = (arg->station[s].station_position_delay_offset);
		local_arg.station.station_val[s].axis_off            = (arg->station[s].axis_off);
		local_arg.station.station_val[s].primary_axis_wrap   = int(arg->station[s].primary_axis_wrap);
		local_arg.station.station_val[s].secondary_axis_wrap = int(arg->station[s].secondary_axis_wrap);
		difx_strlcpy(local_arg.station.station_val[s].receiver_name, arg->station[s].receiver_name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.station.station_val[s].pressure         = (arg->station[s].pressure);
		local_arg.station.station_val[s].antenna_pressure = (arg->station[s].antenna_pressure);
		local_arg.station.station_val[s].temperature      = (arg->station[s].temperature);
		local_arg.station.station_val[s].wind_speed       = (arg->station[s].wind_speed);
		local_arg.station.station_val[s].wind_direction   = (arg->station[s].wind_direction);
		local_arg.station.station_val[s].antenna_phys_temperature = (arg->station[s].antenna_phys_temperature);
	}
	local_arg.Num_Sources = u_int(arg->Num_Sources);
	if(arg->Num_Sources > local_Num_Sources)
	{
		local_source = reinterpret_cast<DIFX_DELAY_SERVER_1_source*>(realloc(local_source, arg->Num_Sources*sizeof(DIFX_DELAY_SERVER_1_source)));
		if(local_source == NULL)
		{
			return -2;
		}
		local_Num_Sources = arg->Num_Sources;
	}
	local_arg.source.source_len = u_int(arg->Num_Sources);
	local_arg.source.source_val = local_source;
	for(uint_fast32_t s=0; s < arg->Num_Sources; ++s)
	{
		difx_strlcpy(local_arg.source.source_val[s].source_name, arg->source[s].source_name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.source.source_val[s].IAU_name, arg->source[s].IAU_name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		difx_strlcpy(local_arg.source.source_val[s].source_type, arg->source[s].source_type, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.source.source_val[s].ra       = (arg->source[s].ra);
		local_arg.source.source_val[s].dec      = (arg->source[s].dec);
		local_arg.source.source_val[s].dra      = (arg->source[s].dra);
		local_arg.source.source_val[s].ddec     = (arg->source[s].ddec);
		local_arg.source.source_val[s].depoch   = (arg->source[s].depoch);
		local_arg.source.source_val[s].parallax = (arg->source[s].parallax);
		difx_strlcpy(local_arg.source.source_val[s].coord_frame, sourceCoordinateFrameTypeNames[arg->source[s].coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
		local_arg.source.source_val[s].source_pos.x = (arg->source[s].source_pos[0]);
		local_arg.source.source_val[s].source_pos.y = (arg->source[s].source_pos[1]);
		local_arg.source.source_val[s].source_pos.z = (arg->source[s].source_pos[2]);
		local_arg.source.source_val[s].source_vel.x = (arg->source[s].source_vel[0]);
		local_arg.source.source_val[s].source_vel.y = (arg->source[s].source_vel[1]);
		local_arg.source.source_val[s].source_vel.z = (arg->source[s].source_vel[2]);
		local_arg.source.source_val[s].source_acc.x = (arg->source[s].source_acc[0]);
		local_arg.source.source_val[s].source_acc.y = (arg->source[s].source_acc[1]);
		local_arg.source.source_val[s].source_acc.z = (arg->source[s].source_acc[2]);
		local_arg.source.source_val[s].source_pointing_dir.x           = (arg->source[s].source_pointing_dir[0]);
		local_arg.source.source_val[s].source_pointing_dir.y           = (arg->source[s].source_pointing_dir[1]);
		local_arg.source.source_val[s].source_pointing_dir.z           = (arg->source[s].source_pointing_dir[2]);
		local_arg.source.source_val[s].source_pointing_reference_dir.x = (arg->source[s].source_pointing_reference_dir[0]);
		local_arg.source.source_val[s].source_pointing_reference_dir.y = (arg->source[s].source_pointing_reference_dir[1]);
		local_arg.source.source_val[s].source_pointing_reference_dir.z = (arg->source[s].source_pointing_reference_dir[2]);
	}
	local_arg.Num_EOPs = u_int(arg->Num_EOPs);
	if(arg->Num_EOPs > local_Num_EOPs)
	{
		local_EOP = reinterpret_cast<DIFX_DELAY_SERVER_1_EOP*>(realloc(local_EOP, arg->Num_EOPs*sizeof(DIFX_DELAY_SERVER_1_EOP)));
		if(local_EOP == NULL)
		{
			return -3;
		}
		local_Num_EOPs = arg->Num_EOPs;
	}
	local_arg.EOP.EOP_len = u_int(arg->Num_EOPs);
	local_arg.EOP.EOP_val = local_EOP;
	for(uint_fast32_t e=0; e < arg->Num_EOPs; ++e)
	{
		local_arg.EOP.EOP_val[e].EOP_time = (arg->EOP[e].EOP_time);
		local_arg.EOP.EOP_val[e].tai_utc  = (arg->EOP[e].tai_utc);
		local_arg.EOP.EOP_val[e].ut1_utc  = (arg->EOP[e].ut1_utc);
		local_arg.EOP.EOP_val[e].xpole    = (arg->EOP[e].xpole);
		local_arg.EOP.EOP_val[e].ypole    = (arg->EOP[e].ypole);
	}
	return 0;
}

int_fast32_t DelayHandlerDiFXDelayServer::convert_from_local_SERVER_MODEL_DELAY_RESPONSE() throw()
{
	struct SERVER_MODEL_DELAY_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE*>(data_output));

	if(local_res.this_error != 0)
	{
		return -1;
	}
	const struct DIFX_DELAY_SERVER_1_res* const local_resp = &(local_res.getDIFX_DELAY_SERVER_1_res_u.response);
	res->rpc_handler_error        = local_resp->delay_server_error;
	res->server_error             = local_resp->server_error;
	res->model_error              = local_resp->model_error;
	res->request_id               = local_resp->request_id;
	res->server_struct_setup_code = local_resp->server_struct_setup_code;
	res->server_version           = local_resp->server_version;
	res->date = local_resp->date;
	res->time = local_resp->time;
	DiFX::Delay::Time::DelayTimestamp t(res->date,res->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
	res->utc_second = t.i();
	res->utc_second_fraction = t.f();
	DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
	res->tai_second = t_TAI.i();
	res->tai_second_fraction = t_TAI.f();
	if(res->Num_Stations != local_resp->Num_Stations)
	{
		// Caller did not set up return area properly
		return -2;
	}
	if(res->Num_Sources != local_resp->Num_Sources)
	{
		// Caller did not set up return area properly
		return -3;
	}
	const uint_fast32_t NUM = res->Num_Stations*res->Num_Sources;
	for(uint_fast32_t s=0; s < NUM; ++s)
	{
		res->result[s].delay      = local_resp->result.result_val[s].delay;
		res->result[s].dry_atmos  = local_resp->result.result_val[s].dry_atmos;
		res->result[s].wet_atmos  = local_resp->result.result_val[s].wet_atmos;
		res->result[s].iono_atmos = local_resp->result.result_val[s].iono_atmos;
		res->result[s].az_corr    = local_resp->result.result_val[s].az_corr;
		res->result[s].el_corr    = local_resp->result.result_val[s].el_corr;
		res->result[s].az_geom    = local_resp->result.result_val[s].az_geom;
		res->result[s].el_geom    = local_resp->result.result_val[s].el_geom;
		res->result[s].primary_axis_angle    = local_resp->result.result_val[s].primary_axis_angle;
		res->result[s].secondary_axis_angle  = local_resp->result.result_val[s].secondary_axis_angle;
		res->result[s].mount_source_angle    = local_resp->result.result_val[s].mount_source_angle;
		res->result[s].station_antenna_theta = local_resp->result.result_val[s].station_antenna_theta;
		res->result[s].station_antenna_phi   = local_resp->result.result_val[s].station_antenna_phi;
		res->result[s].source_antenna_theta  = local_resp->result.result_val[s].source_antenna_theta;
		res->result[s].source_antenna_phi    = local_resp->result.result_val[s].source_antenna_phi;
		res->result[s].UVW[0]           = local_resp->result.result_val[s].UVW.x;
		res->result[s].UVW[1]           = local_resp->result.result_val[s].UVW.y;
		res->result[s].UVW[2]           = local_resp->result.result_val[s].UVW.z;
		res->result[s].baselineP2000[0] = local_resp->result.result_val[s].baselineP2000.x;
		res->result[s].baselineP2000[1] = local_resp->result.result_val[s].baselineP2000.y;
		res->result[s].baselineP2000[2] = local_resp->result.result_val[s].baselineP2000.z;
		res->result[s].baselineV2000[0] = local_resp->result.result_val[s].baselineV2000.x;
		res->result[s].baselineV2000[1] = local_resp->result.result_val[s].baselineV2000.y;
		res->result[s].baselineV2000[2] = local_resp->result.result_val[s].baselineV2000.z;
		res->result[s].baselineA2000[0] = local_resp->result.result_val[s].baselineA2000.x;
		res->result[s].baselineA2000[1] = local_resp->result.result_val[s].baselineA2000.y;
		res->result[s].baselineA2000[2] = local_resp->result.result_val[s].baselineA2000.z;
	}
	return 0;
}



int_fast32_t DelayHandlerDiFXDelayServer::convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT() throw()
{
	const struct SERVER_MODEL_PARAMETERS_ARGUMENT* const arg(reinterpret_cast<const struct SERVER_MODEL_PARAMETERS_ARGUMENT*>(data_input));

	local_parg.request_id = long(arg->request_id);
	local_parg.delay_server = u_long(delayServer);
	local_parg.server_struct_setup_code = long(arg->server_struct_setup_code);
	local_parg.verbosity = int(verbose);
	return 0;
}


int_fast32_t DelayHandlerDiFXDelayServer::convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE() throw()
{
	struct SERVER_MODEL_PARAMETERS_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_PARAMETERS_RESPONSE*>(data_output));

	if(local_pres.this_error != 0)
	{
		return -1;
	}
	const struct DIFX_DELAY_SERVER_PARAMETERS_1_res* const local_resp = &(local_pres.getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.response);
	res->rpc_handler_error        = local_resp->delay_server_error;
	res->server_error             = local_resp->server_error;
	res->model_error              = local_resp->model_error;
	res->request_id               = local_resp->request_id;
	res->server_struct_setup_code = local_resp->server_struct_setup_code;
	res->server_version           = local_resp->server_version;
	res->accelgrv  = local_resp->accelgrv;
	res->e_flat    = local_resp->e_flat;
	res->earthrad  = local_resp->earthrad;
	res->mmsems    = local_resp->mmsems;
	res->ephepoc   = local_resp->ephepoc;
	res->gauss     = local_resp->gauss;
	res->u_grv_cn  = local_resp->u_grv_cn;
	res->gmsun     = local_resp->gmsun;
	res->gmmercury = local_resp->gmmercury;
	res->gmvenus   = local_resp->gmvenus;
	res->gmearth   = local_resp->gmearth;
	res->gmmoon    = local_resp->gmmoon;
	res->gmmars    = local_resp->gmmars;
	res->gmjupiter = local_resp->gmjupiter;
	res->gmsaturn  = local_resp->gmsaturn;
	res->gmuranus  = local_resp->gmuranus;
	res->gmneptune = local_resp->gmneptune;
	res->etidelag  = local_resp->etidelag;
	res->love_h    = local_resp->love_h;
	res->love_l    = local_resp->love_l;
	res->pre_data  = local_resp->pre_data;
	res->rel_data  = local_resp->rel_data;
	res->tidalut1  = local_resp->tidalut1;
	res->au        = local_resp->au;
	res->tsecau    = local_resp->tsecau;
	res->vlight    = local_resp->vlight;
	return 0;
}











void DelayHandlerDiFXDelayServer::print_local_SERVER_MODEL_DELAY_ARGUMENT() throw()
{
	unsigned int s, e;
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:Delay Argument\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: request_id=0x%lX delay_server=0x%lX server_struct_setup_code=0x%lX\n", local_arg.request_id, local_arg.delay_server, local_arg.server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: date=%ld time=%16.12f ref_frame=%ld verbosity=%d\n", local_arg.date, local_arg.time, local_arg.ref_frame, local_arg.verbosity);
	if(verbose >= 4) {
		unsigned int k;
		for(k=0; k < NUM_DIFX_DELAY_SERVER_1_KFLAGS; k++) {
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: kflag[%02u]=%hd\n", k, local_arg.kflags[k]);
		}
	}
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: sky_frequency = %E\n", local_arg.sky_frequency);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: Station information\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: Use_Server_Station_Table=%d Num_Stations=%u (%u)\n", local_arg.Use_Server_Station_Table, local_arg.Num_Stations, local_arg.station.station_len);
	for(s=0; s < local_arg.Num_Stations; s++) {
		char ID0, ID1;
		struct DIFX_DELAY_SERVER_vec v;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_name='%s'\n", s, local_arg.station.station_val[s].station_name);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u antenna_name='%s'\n", s, local_arg.station.station_val[s].antenna_name);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u site_name=   '%s'\n", s, local_arg.station.station_val[s].site_name);
		ID0 = (char)(local_arg.station.station_val[s].site_ID&0xFF);
		ID1 = (char)(local_arg.station.station_val[s].site_ID>>8);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u site_ID=     '%c%c' 0x%04hX\n", s, ID0, ID1, local_arg.station.station_val[s].site_ID);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u site_type=   '%s'\n", s, local_arg.station.station_val[s].site_type);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u axis_type=   '%s'\n", s, local_arg.station.station_val[s].axis_type);
		v = local_arg.station.station_val[s].station_pos;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_pos= [%14.4f, %14.4f, %14.4f]\n", s, v.x, v.y, v.z);
		v = local_arg.station.station_val[s].station_vel;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_vel= [%14.9E, %14.9E, %14.9E]\n", s, v.x, v.y, v.z);
		v = local_arg.station.station_val[s].station_acc;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_acc= [%14.9E, %14.9E, %14.9E]\n", s, v.x, v.y, v.z);
		v = local_arg.station.station_val[s].station_pointing_dir;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_pointing_dir=  [%14.9E, %14.9E, %14.9E]\n", s, v.x, v.y, v.z);
		v = local_arg.station.station_val[s].station_reference_dir;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_reference_dir= [%14.9E, %14.9E, %14.9E]\n", s, v.x, v.y, v.z);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_coord_frame='%s'\n", s, local_arg.station.station_val[s].station_coord_frame);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u pointing_coord_frame='%s'\n", s, local_arg.station.station_val[s].pointing_coord_frame);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u pointing_corrections_applied=%d\n", s, local_arg.station.station_val[s].pointing_corrections_applied);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u station_position_delay_offset=%E\n", s, local_arg.station.station_val[s].station_position_delay_offset);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u axis_off=%7.4f primary_axis_wrap=%2d secondary_axis_wrap=%2d\n", s, local_arg.station.station_val[s].axis_off, local_arg.station.station_val[s].primary_axis_wrap, local_arg.station.station_val[s].secondary_axis_wrap);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u receiver_name='%s'\n", s, local_arg.station.station_val[s].receiver_name);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u pressure=%12.3E antenna_pressure=%12.3E temperature=%6.1f\n", s, local_arg.station.station_val[s].pressure, local_arg.station.station_val[s].antenna_pressure, local_arg.station.station_val[s].temperature);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: station=%02u wind_speed=%6.1f wind_direction=%7.2f antenna_phys_temperature=%6.1f\n", s, local_arg.station.station_val[s].wind_speed, local_arg.station.station_val[s].wind_direction, local_arg.station.station_val[s].antenna_phys_temperature);
	}
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: Source information\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: Use_Server_Source_Table=%d Num_Sources=%u (%u)\n", local_arg.Use_Server_Source_Table, local_arg.Num_Sources, local_arg.source.source_len);
	for(s=0; s < local_arg.Num_Sources; s++) {
		struct DIFX_DELAY_SERVER_vec v;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_name='%s'\n", s, local_arg.source.source_val[s].source_name);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u IAU_name=   '%s'\n", s, local_arg.source.source_val[s].IAU_name);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_type='%s'\n", s, local_arg.source.source_val[s].source_type);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u ra=           %20.16f\n", s, local_arg.source.source_val[s].ra);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u dec=          %20.16f\n", s, local_arg.source.source_val[s].dec);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u dra=          %20.10f\n", s, local_arg.source.source_val[s].dra);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u ddec=         %20.10f\n", s, local_arg.source.source_val[s].ddec);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u depoch=       %20.16f\n", s, local_arg.source.source_val[s].depoch);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u parallax=     %20.3f\n", s, local_arg.source.source_val[s].parallax);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u coord_frame= '%s'\n", s, local_arg.source.source_val[s].coord_frame);
		v = local_arg.source.source_val[s].source_pos;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_pos= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
		v = local_arg.source.source_val[s].source_vel;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_vel= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
		v = local_arg.source.source_val[s].source_acc;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_acc= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
		v = local_arg.source.source_val[s].source_pointing_dir;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_pointing_dir=           [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
		v = local_arg.source.source_val[s].source_pointing_reference_dir;
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: source=%02u source_pointing_reference_dir= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
	}
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: EOP information\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: Use_Server_EOP_Table=%d Num_EOPs=%u (%u)\n", local_arg.Use_Server_EOP_Table, local_arg.Num_EOPs, local_arg.EOP.EOP_len);
	for(e=0; e < local_arg.Num_EOPs; e++) {
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: EOP=%02u EOP_time=  %20.11f\n", e, local_arg.EOP.EOP_val[e].EOP_time);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: EOP=%02u tai_utc=   %20.12f\n", e, local_arg.EOP.EOP_val[e].tai_utc);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: EOP=%02u ut1_utc=   %20.12f\n", e, local_arg.EOP.EOP_val[e].ut1_utc);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: EOP=%02u xpole=     %10.6f\n", e, local_arg.EOP.EOP_val[e].xpole);
		DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: EOP=%02u ypole=     %10.6f\n", e, local_arg.EOP.EOP_val[e].ypole);
	}
	return;
}

void DelayHandlerDiFXDelayServer::print_local_SERVER_MODEL_DELAY_RESPONSE() throw()
{
	unsigned int st, so;
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:Delay Response\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: delay_server_error=%d server_error=%d model_error=%d\n", local_res.getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error, local_res.getDIFX_DELAY_SERVER_1_res_u.response.server_error, local_res.getDIFX_DELAY_SERVER_1_res_u.response.model_error);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: request_id=%ld delay_server=0x%lX server_struct_setup_code=0x%lX\n", local_res.getDIFX_DELAY_SERVER_1_res_u.response.request_id, local_res.getDIFX_DELAY_SERVER_1_res_u.response.delay_server, local_res.getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: server_version=0x%lX\n", local_res.getDIFX_DELAY_SERVER_1_res_u.response.server_version);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: date=%ld time=%.16f\n", local_res.getDIFX_DELAY_SERVER_1_res_u.response.date, local_res.getDIFX_DELAY_SERVER_1_res_u.response.time);
	for(st=0; st < local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Stations; ++st)
	{
		for(so=0; so < local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources; ++so)
		{
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u delay=                %24.16E\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].delay);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u dry_atmos=            %24.16E\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].dry_atmos);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u wet_atmos=            %24.16E\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].wet_atmos);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u iono_atmos=           %24.16E\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].iono_atmos);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u az_corr=              %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].az_corr);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u el_corr=              %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].el_corr);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u az_geom=              %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].az_geom);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u el_geom=              %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].el_geom);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u primary_axis_angle=   %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].primary_axis_angle);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u secondary_axis_angle= %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].secondary_axis_angle);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u mount_source_angle=   %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].mount_source_angle);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u station_antenna_theta=%10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].station_antenna_theta);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u station_antenna_phi=  %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].station_antenna_phi);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u source_antenna_theta= %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].source_antenna_theta);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u source_antenna_phi=   %10.6f\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].source_antenna_phi);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u UVW =           [%24.16E, %24.16E, %24.16E]\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].UVW.x, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].UVW.y, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].UVW.z);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u baselineP2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineP2000.x, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineP2000.y, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineP2000.z);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u baselineV2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineV2000.x, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineV2000.y, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineV2000.z);
			DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:response res: station=%02u source=%02u baselineA2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineA2000.x, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineA2000.y, local_res.getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*local_res.getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineA2000.z);
		}
	}
	return;
}

void DelayHandlerDiFXDelayServer::print_local_SERVER_MODEL_PARAMETERS_ARGUMENT() throw()
{
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:Parameter Argument\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: request_id=0x%"PRIX64" delay_server=0x%"PRIX64" server_struct_setup_code=0x%"PRIX64"\n", local_parg.request_id, local_parg.delay_server, local_parg.server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: request arg: verbosity=%"PRId32"\n", local_parg.verbosity);
	fflush(stdout);
	return;
}
void DelayHandlerDiFXDelayServer::print_local_SERVER_MODEL_PARAMETERS_RESPONSE() throw()
{
	const DIFX_DELAY_SERVER_PARAMETERS_1_res* const res = &(local_pres.getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.response);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer:Parameter Response\n");
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: result: this_error = %d\n", local_pres.this_error);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: delay_server_error=%d server_error=%d model_error=%d\n", res->delay_server_error, res->server_error, res->model_error);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: request_id=%ld delay_server=0x%lX\n", res->request_id, res->delay_server);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: server_struct_setup_code=0x%lX server_version=0x%lX\n", res->server_struct_setup_code, res->server_version);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: accelgrv= %25.16E\n", res->accelgrv);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: e_flat=   %25.16E\n", res->e_flat);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: earthrad= %25.16E\n", res->earthrad);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: mmsems=   %25.16E\n", res->mmsems);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: ephepoc=  %25.16E\n", res->ephepoc);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gauss=    %25.16E\n", res->gauss);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: u_grv_cn= %25.16E\n", res->u_grv_cn);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmsun=    %25.16E\n", res->gmsun);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmmercury=%25.16E\n", res->gmmercury);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmvenus=  %25.16E\n", res->gmvenus);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmearth=  %25.16E\n", res->gmearth);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmmoon=   %25.16E\n", res->gmmoon);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmmars=   %25.16E\n", res->gmmars);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmjupiter=%25.16E\n", res->gmjupiter);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmsaturn= %25.16E\n", res->gmsaturn);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmuranus= %25.16E\n", res->gmuranus);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: gmneptune=%25.16E\n", res->gmneptune);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: etidelag= %25.16E\n", res->etidelag);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: love_h=   %25.16E\n", res->love_h);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: love_l=   %25.16E\n", res->love_l);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: pre_data= %25.16E\n", res->pre_data);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: rel_data= %25.16E\n", res->rel_data);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: tidalut1= %25.16E\n", res->tidalut1);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: au=       %25.16E\n", res->au);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: tsecau=   %25.16E\n", res->tsecau);
	DHBfprintf(stdout, "DelayHandlerDiFXDelayServer: results: vlight=   %25.16E\n", res->vlight);
	fflush(stdout);
	return;
}



}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX


