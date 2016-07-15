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
#include <string.h>
#include <limits>

#include "DelayHandlerTest.h"

namespace DiFX {
namespace Delay {
namespace Handler {






// GLOBALS
const char* DelayHandlerTest::server_command_name = "./cat";

// FUNCTIONS

DelayHandlerTest::DelayHandlerTest(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_) throw()
		:
		DelayHandlerBase(NUM_THREADS_, verbosity_)
{
	int_fast32_t retval;
	retval = init_threads();
	if((retval))
	{
		overall_status = delayHandlerStatus_HandlerError;
		return;
	}
	return;
}

    
DelayHandlerTest::~DelayHandlerTest()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running ~DelayHandlerTest\n");
	}
	stop_threads();
	return;
}





                      


int_fast32_t DelayHandlerTest::do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw()
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



int_fast32_t DelayHandlerTest::do_thread_init(const int_fast32_t tid) throw()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_init\n", tid);
	}
	int_fast32_t retval = 0;
	const char* argv[3];
	const char arg1[] = "-";
	argv[0] = server_command_name;
	argv[1] = arg1;
	argv[2] = NULL;
	retval = start_child_process(server_command_name, argv, server_command_name, tid);
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_init with retval=%"PRIdFAST32"\n", tid, retval);
	}
    return retval;
}


int_fast32_t DelayHandlerTest::do_thread_test_delays(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	size_t LINE_MAX = 128;
	char line[LINE_MAX];
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_test_delays\n", tid);
	}
	if((child_process[tid]<=0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Bad child process setup for thread=%"PRIdFAST32"\n", tid);
		
		retval_final = -1;
		goto do_thread_get_delays_error;
	}
	if(ferror(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -2;
		goto do_thread_get_delays_error;
	}
	if(fprintf(child_stdin[tid], "Hello world! do_thread_test_delays\n") < 0)
	{
		DHBfprintf(stderr, "Error writing to child by process in thread=%"PRIdFAST32"\n", tid);
		retval_final = -3;
		goto do_thread_get_delays_error;
	}
	if(fflush(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process could not be flushed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -4;
		goto do_thread_get_delays_error;
	}
	if(ferror(child_stdout[tid]))
	{
		DHBfprintf(stderr, "Pipe from child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -5;
		goto do_thread_get_delays_error;
	}
	line[0] = 0;
	if(fgets(line, LINE_MAX, child_stdout[tid]) == NULL)
	{
		DHBfprintf(stderr, "Pipe from child process had read error for thread=%"PRIdFAST32"\n", tid);
		retval_final = -6;
		goto do_thread_get_delays_error;
	}
	DHBfprintf(stdout, "thread %"PRIdFAST32" got back '%s'\n", tid, line);
do_thread_get_delays_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_test_delays with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerTest::do_thread_test_parameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	size_t LINE_MAX = 128;
	char line[LINE_MAX];
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_test_parameters\n", tid);
	}
	if((child_process[tid]<=0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Bad child process setup for thread=%"PRIdFAST32"\n", tid);
		
		retval_final = -1;
		goto do_thread_get_parameters_error;
	}
	if(ferror(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -2;
		goto do_thread_get_parameters_error;
	}
	if(fprintf(child_stdin[tid], "Test Parameters\n") < 0)
	{
		DHBfprintf(stderr, "Error writing to child by process in thread=%"PRIdFAST32"\n", tid);
		retval_final = -3;
		goto do_thread_get_parameters_error;
	}
	if(fflush(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process could not be flushed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -4;
		goto do_thread_get_parameters_error;
	}
	if(ferror(child_stdout[tid]))
	{
		DHBfprintf(stderr, "Pipe from child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -5;
		goto do_thread_get_parameters_error;
	}
	line[0] = 0;
	if(fgets(line, LINE_MAX, child_stdout[tid]) == NULL)
	{
		DHBfprintf(stderr, "Pipe from child process had read error for thread=%"PRIdFAST32"\n", tid);
		retval_final = -6;
		goto do_thread_get_parameters_error;
	}
	DHBfprintf(stdout, "thread %"PRIdFAST32" got back '%s'\n", tid, line);
do_thread_get_parameters_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_test_parameters with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}




int_fast32_t DelayHandlerTest::do_thread_get_delays(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	size_t LINE_MAX = 128;
	char line[LINE_MAX];
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_get_delays\n", tid);
	}
	if((child_process[tid]<=0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Bad child process setup for thread=%"PRIdFAST32"\n", tid);
		
		retval_final = -1;
		goto do_thread_get_delays_error;
	}
	if(ferror(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -2;
		goto do_thread_get_delays_error;
	}
	if(fprintf(child_stdin[tid], "Hello world! do_thread_get_delays\n") < 0)
	{
		DHBfprintf(stderr, "Error writing to child by process in thread=%"PRIdFAST32"\n", tid);
		retval_final = -3;
		goto do_thread_get_delays_error;
	}
	if(fflush(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process could not be flushed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -4;
		goto do_thread_get_delays_error;
	}
	if(ferror(child_stdout[tid]))
	{
		DHBfprintf(stderr, "Pipe from child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -5;
		goto do_thread_get_delays_error;
	}
	line[0] = 0;
	if(fgets(line, LINE_MAX, child_stdout[tid]) == NULL)
	{
		DHBfprintf(stderr, "Pipe from child process had read error for thread=%"PRIdFAST32"\n", tid);
		retval_final = -6;
		goto do_thread_get_delays_error;
	}
	DHBfprintf(stdout, "thread %"PRIdFAST32" got back '%s'\n", tid, line);
do_thread_get_delays_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_get_delays with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerTest::do_thread_get_parameters(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	size_t LINE_MAX = 128;
	char line[LINE_MAX];
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_get_parameters\n", tid);
	}
	if((child_process[tid]<=0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Bad child process setup for thread=%"PRIdFAST32"\n", tid);
		
		retval_final = -1;
		goto do_thread_get_parameters_error;
	}
	if(ferror(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -2;
		goto do_thread_get_parameters_error;
	}
	if(fprintf(child_stdin[tid], "Get Parameters\n") < 0)
	{
		DHBfprintf(stderr, "Error writing to child by process in thread=%"PRIdFAST32"\n", tid);
		retval_final = -3;
		goto do_thread_get_parameters_error;
	}
	if(fflush(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process could not be flushed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -4;
		goto do_thread_get_parameters_error;
	}
	if(ferror(child_stdout[tid]))
	{
		DHBfprintf(stderr, "Pipe from child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -5;
		goto do_thread_get_parameters_error;
	}
	line[0] = 0;
	if(fgets(line, LINE_MAX, child_stdout[tid]) == NULL)
	{
		DHBfprintf(stderr, "Pipe from child process had read error for thread=%"PRIdFAST32"\n", tid);
		retval_final = -6;
		goto do_thread_get_parameters_error;
	}
	DHBfprintf(stdout, "thread %"PRIdFAST32" got back '%s'\n", tid, line);
	{
		if(tid == 0)
		{
			struct SERVER_MODEL_PARAMETERS_RESPONSE* const res(reinterpret_cast<struct SERVER_MODEL_PARAMETERS_RESPONSE*>(data_output));
			res->vlight                   = 299792458.0;
		}
	}
do_thread_get_parameters_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_get_parameters with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}




int_fast32_t DelayHandlerTest::do_thread_stop(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	size_t LINE_MAX = 128;
	char line[LINE_MAX];
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_stop\n", tid);
	}
	if((child_process[tid]<=0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Bad child process setup for thread=%"PRIdFAST32"\n", tid);
		
		retval_final = -1;
		goto do_thread_stop_error;
	}
	if(ferror(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -2;
		goto do_thread_stop_error;
	}
	if(fprintf(child_stdin[tid], "Stop!\n") < 0)
	{
		DHBfprintf(stderr, "Error writing to child by process in thread=%"PRIdFAST32"\n", tid);
		retval_final = -3;
		goto do_thread_stop_error;
	}
	if(fflush(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process could not be flushed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -4;
		goto do_thread_stop_error;
	}
	if(ferror(child_stdout[tid]))
	{
		DHBfprintf(stderr, "Pipe from child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -5;
		goto do_thread_stop_error;
	}
	line[0] = 0;
	if(fgets(line, LINE_MAX, child_stdout[tid]) == NULL)
	{
		DHBfprintf(stderr, "Pipe from child process had read error for thread=%"PRIdFAST32"\n", tid);
		retval_final = -6;
		goto do_thread_stop_error;
	}
	DHBfprintf(stdout, "thread %"PRIdFAST32" got back '%s'\n", tid, line);
	if(fclose(child_stdin[tid]))
	{
		// It is an error to try to close this again, so mark as freed.
		child_stdin[tid] = NULL;
		DHBfprintf(stderr, "Closing pipe to child process failed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -7;
		goto do_thread_stop_error;
	}
	child_stdin[tid] = NULL;
	if(fclose(child_stdout[tid]))
	{
		// It is an error to try to close this again, so mark as freed.
		child_stdout[tid] = NULL;
		DHBfprintf(stderr, "Closing pipe from child process failed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -8;
		goto do_thread_stop_error;
	}
	child_stdout[tid] = NULL;
do_thread_stop_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_stop with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}


int_fast32_t DelayHandlerTest::do_thread_panic(const int_fast32_t tid) throw()
{
	int_fast32_t retval_final = 0;
	size_t LINE_MAX = 128;
	char line[LINE_MAX];
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" inside do_thread_panic\n", tid);
	}
	if((child_process[tid]<=0) || (child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Bad child process setup for thread=%"PRIdFAST32"\n", tid);
		
		retval_final = -1;
		goto do_thread_panic_error;
	}
	if(ferror(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -2;
		goto do_thread_panic_error;
	}
	if(fprintf(child_stdin[tid], "Panic!\n") < 0)
	{
		DHBfprintf(stderr, "Error writing to child by process in thread=%"PRIdFAST32"\n", tid);
		retval_final = -3;
		goto do_thread_panic_error;
	}
	if(fflush(child_stdin[tid]))
	{
		DHBfprintf(stderr, "Pipe to child process could not be flushed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -4;
		goto do_thread_panic_error;
	}
	if(ferror(child_stdout[tid]))
	{
		DHBfprintf(stderr, "Pipe from child process broken for thread=%"PRIdFAST32"\n", tid);
		retval_final = -5;
		goto do_thread_panic_error;
	}
	line[0] = 0;
	if(fgets(line, LINE_MAX, child_stdout[tid]) == NULL)
	{
		DHBfprintf(stderr, "Pipe from child process had read error for thread=%"PRIdFAST32"\n", tid);
		retval_final = -6;
		goto do_thread_panic_error;
	}
	DHBfprintf(stdout, "thread %"PRIdFAST32" got back '%s'\n", tid, line);
	if(fclose(child_stdin[tid]))
	{
		// It is an error to try to close this again, so mark as freed.
		child_stdin[tid] = NULL;
		DHBfprintf(stderr, "Closing pipe to child process failed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -7;
		goto do_thread_panic_error;
	}
	child_stdin[tid] = NULL;
	if(fclose(child_stdout[tid]))
	{
		// It is an error to try to close this again, so mark as freed.
		child_stdout[tid] = NULL;
		DHBfprintf(stderr, "Closing pipe from child process failed for thread=%"PRIdFAST32"\n", tid);
		retval_final = -8;
		goto do_thread_panic_error;
	}
	child_stdout[tid] = NULL;
do_thread_panic_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" leaving do_thread_panic with retval=%"PRIdFAST32"\n", tid, retval_final);
	}
    return retval_final;
}







}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX


