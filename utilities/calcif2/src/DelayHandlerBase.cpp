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
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <limits>
#include <math.h>

#include "DelayHandlerBase.h"




namespace DiFX {
namespace Delay {
namespace Handler {

namespace {
extern "C" void* thread_entry(void* pthis)
{
	DelayHandlerBase* object = reinterpret_cast<DelayHandlerBase*>(pthis);
	void* retval =  object->start_thread();
	fprintf(stderr, "Error in thread_entry: strat_thread returned!\n");
	return retval;
}
}





// GLOBALS
const char* DelayHandlerBase::TMP_DIRECTORY = "DiFX_DelayHandler_tmp";

// FUNCTIONS

DelayHandlerBase::DelayHandlerBase(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_) throw()
		:
		commands_processed(0),
		parent_pid(0),
		verbosity(verbosity_),
		D(NULL),
		job(NULL),
		scan(NULL),
		data_input(NULL),
		data_output(NULL),
		verbose(0),
		// uncomment the following when finally using a C++11 compiler
		// thread_mutex(PTHREAD_MUTEX_INITIALIZER),
		// fprintf_mutex(PTHREAD_MUTEX_INITIALIZER),
		// work_condition(PTHREAD_COND_INITIALIZER),
		// sleeping_condition(PTHREAD_COND_INITIALIZER),
		NUM_THREADS(NUM_THREADS_),
		num_sleeping(0),
		num_running(0),
		thread_status(NULL),
		stationStart(NULL),
		stationEnd(NULL),
		thread(NULL),
		action(delayHandlerAction_Unknown),
		thread_failure(false),
		overall_status(delayHandlerStatus_Initializing),
		child_process(NULL),
		child_stdin(NULL),
		child_stdout(NULL)
{
	handle_SIGPIPE();
	int_fast32_t retval = init_mutexes();
	if((retval))
	{
		overall_status=delayHandlerStatus_HandlerError;
		return;
	}
	parent_pid = static_cast<int_least64_t>(getpid());
	retval = init_NUM_THREADS();
	if((retval))
	{
		overall_status=delayHandlerStatus_HandlerError;
		return;
	}
	checkForTmpDirectory();
	retval = allocate_storage();
	if((retval))
	{
		overall_status=delayHandlerStatus_HandlerError;
		return;
	}
	retval = create_threads();
	if((retval))
	{
		overall_status=delayHandlerStatus_HandlerError;
		return;
	}
	overall_status=delayHandlerStatus_Sleeping;
	return;
}

    
DelayHandlerBase::~DelayHandlerBase()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running ~DelayHandlerBase\n");
	}
	// stop_threads();  This must be called by the derived class desctructor
	panic_stop_children();
	delete_storage();
	delete_mutexes();
	overall_status=delayHandlerStatus_HandlerStopped;
	return;
}


void DelayHandlerBase::handle_SIGPIPE() throw()
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, 0) == -1) {
		perror("sigaction");
		fprintf(stderr, "Error: unable to have sigaction ignore SIGPIPE\n");
		exit(EXIT_FAILURE);
	}
	return;
}


int_fast32_t DelayHandlerBase::init_mutexes() throw()
{
	int retval = pthread_mutex_init(&thread_mutex, NULL);
	if((retval)) {
		perror("initializing thread_mutex failed");
		fprintf(stderr, "ERROR: %s:%d: initializing thread_mutex failed with %d\n", __FILE__, __LINE__, retval);
		overall_status=delayHandlerStatus_HandlerError;
		return -1;
	}
	retval = pthread_mutex_init(&fprintf_mutex, NULL);
	if((retval)) {
		perror("initializing fprintf_mutex failed");
		fprintf(stderr, "ERROR: %s:%d: initializing fprintf_mutex failed with %d\n", __FILE__, __LINE__, retval);
		overall_status=delayHandlerStatus_HandlerError;
		return -2;
	}
	retval = pthread_cond_init(&work_condition, NULL);
	if((retval)) {
		perror("initializing work_condition failed");
		fprintf(stderr, "ERROR: %s:%d: initializing work_condition failed with %d\n", __FILE__, __LINE__, retval);
		overall_status=delayHandlerStatus_HandlerError;
		return -3;
	}
	retval = pthread_cond_init(&sleeping_condition, NULL);
	if((retval)) {
		perror("initializing sleeping_condition failed");
		fprintf(stderr, "ERROR: %s:%d: initializing sleeping_condition failed with %d\n", __FILE__, __LINE__, retval);
		overall_status=delayHandlerStatus_HandlerError;
		return -4;
	}
	return 0;
}
void DelayHandlerBase::delete_mutexes() throw()
{
	pthread_mutex_destroy(&thread_mutex);
	pthread_mutex_destroy(&fprintf_mutex);
	pthread_cond_destroy(&work_condition);
	pthread_cond_destroy(&sleeping_condition);
	return;
}


int_fast32_t DelayHandlerBase::init_NUM_THREADS() throw()
{
	if(NUM_THREADS <= 0)
	{
		if(verbosity >= 5)
		{
			DHBfprintf(stdout, "Input NUM_THREADS=%"PRIdFAST32", checking for DIFX_CALCIF2_NUM_CALC_THREADS\n", NUM_THREADS);
		}
		const char* const cp = getenv("DIFX_CALCIF2_NUM_CALC_THREADS");
		if(cp != NULL)
		{
			if(verbosity >= 5)
			{
				DHBfprintf(stdout, "Found DIFX_CALCIF2_NUM_CALC_THREADS=%s\n", cp);
			}
			NUM_THREADS = atoi(cp);
		}
		else
		{
			if(verbosity >= 5)
			{
				DHBfprintf(stdout, "Could not find DIFX_CALCIF2_NUM_CALC_THREADS\n");
			}
		}
	}
	if(NUM_THREADS <= 0)
	{
		if(verbosity >= 5)
		{
			DHBfprintf(stdout, "NUM_THREADS=%"PRIdFAST32", setting to hardcoded value\n", NUM_THREADS);
		}
		NUM_THREADS = 2;
		if(verbosity >= 5)
		{
			DHBfprintf(stdout, "NUM_THREADS=%"PRIdFAST32"\n", NUM_THREADS);
		}
	}
	if(NUM_THREADS <= 0)
	{
		DHBfprintf(stderr, "ERROR: %s:%d: NUM_THREADS=%"PRIdFAST32" is invalid\n", __FILE__, __LINE__, NUM_THREADS);
		overall_status=delayHandlerStatus_HandlerError;
		return -1;
	}
	return 0;
}
                      

int_fast32_t DelayHandlerBase::allocate_storage() throw()
{
	// Allocate array space
	thread_status = reinterpret_cast<enum delayHandlerThreadStatusEnum*>(malloc(NUM_THREADS*sizeof(enum delayHandlerThreadStatusEnum)));
	stationStart = reinterpret_cast<uint_fast32_t*>(malloc(NUM_THREADS*sizeof(uint_fast32_t)));
	stationEnd = reinterpret_cast<uint_fast32_t*>(malloc(NUM_THREADS*sizeof(uint_fast32_t)));
	thread = reinterpret_cast<pthread_t*>(malloc(NUM_THREADS*sizeof(pthread_t)));
	child_process = reinterpret_cast<pid_t*>(malloc(NUM_THREADS*sizeof(pid_t)));
	child_stdin = reinterpret_cast<FILE**>(malloc(NUM_THREADS*sizeof(FILE*)));
	child_stdout = reinterpret_cast<FILE**>(malloc(NUM_THREADS*sizeof(FILE*)));

	if((thread_status == NULL) || (stationStart == NULL) || (stationEnd == NULL) || (thread == NULL) || (child_process==NULL) || (child_stdin==NULL) || (child_stdout==NULL))
	{
		DHBfprintf(stderr, "ERROR: %s:%d: malloc failure\n", __FILE__, __LINE__);
		overall_status=delayHandlerStatus_HandlerError;
		return -1;
	}
	for(int_fast32_t thread_id=0; thread_id < NUM_THREADS; ++thread_id)
	{
		thread_status[thread_id] = delayHandlerStatus_Unknown;
		stationStart[thread_id] = -1;
		stationEnd[thread_id] = -1;
		child_process[thread_id] = -1;
		child_stdin[thread_id] = NULL;
		child_stdout[thread_id] = NULL;
	}
	return 0;
}
void DelayHandlerBase::delete_storage() throw()
{
    free(reinterpret_cast<void*>(thread_status)); thread_status=NULL;
	free(reinterpret_cast<void*>(stationStart)); stationStart=NULL;
	free(reinterpret_cast<void*>(stationEnd)); stationEnd=NULL;
	free(reinterpret_cast<void*>(thread)); thread=NULL;
	free(reinterpret_cast<void*>(child_process)); child_process=NULL;
	
	for(int_fast32_t thread_id=0; thread_id < NUM_THREADS; ++thread_id)
	{
		if((child_stdin[thread_id]))
		{
			fclose(child_stdin[thread_id]); child_stdin[thread_id] = NULL;
		}
		if((child_stdout[thread_id]))
		{
			fclose(child_stdout[thread_id]); child_stdout[thread_id] = NULL;
		}
	}
	free(reinterpret_cast<void*>(child_stdin)); child_stdin=NULL;
	free(reinterpret_cast<void*>(child_stdout)); child_stdout=NULL;
	return;
}


int_fast32_t DelayHandlerBase::create_threads() throw()
{
	num_sleeping=0;
	num_running=0;
	action=delayHandlerAction_Create;
	for(int_fast32_t thread_id=0; thread_id < NUM_THREADS; ++thread_id)
	{
		int retval = pthread_create(thread + thread_id, NULL, thread_entry, this);
		if((retval))
		{
			DHBperror(errno, "creating thread failed");
			DHBfprintf(stderr, "ERROR: %s:%d: creating thread %"PRIdFAST32" failed with %d\n", __FILE__, __LINE__, thread_id, retval);
			overall_status=delayHandlerStatus_HandlerError;
			return -1;
		}
	}
	int_fast32_t retval = wait_for_threads_sleeping();
	if((retval))
	{
		DHBfprintf(stderr, "ERROR: %s:%d: wait_for_threads_sleeping failed with %"PRIdFAST32"\n", __FILE__, __LINE__, retval);
		overall_status=delayHandlerStatus_HandlerError;
		return -2;
	}
	return 0;
}

void DelayHandlerBase::panic_stop_child(const int_fast32_t tid) throw()
{
	if((child_process))
	{
		if(child_process[tid] > 0)
		{
			kill(child_process[tid],SIGTERM);
			waitpid(child_process[tid],NULL,0);
			child_process[tid] = -1;
		}
	}
	if((child_stdin))
	{
		if((child_stdin[tid]))
		{
			fclose(child_stdin[tid]); child_stdin[tid] = NULL;
		}
	}
	if((child_stdout))
	{
		if((child_stdout[tid]))
		{
			fclose(child_stdout[tid]); child_stdout[tid] = NULL;
		}
	}
}

void DelayHandlerBase::panic_stop_children() throw()
{
	for(int_fast32_t tid=0; tid < NUM_THREADS; ++tid)
	{
		panic_stop_child(tid);
	}
	return;
}



    
int_fast32_t DelayHandlerBase::check_status_before_working() throw()
{
	int_fast32_t return_code = 0;
	if(overall_status==delayHandlerStatus_HandlerStopped)
	{
		return -1;
	}
	if(overall_status==delayHandlerStatus_HandlerError)
	{
		return -2;
	}
	lock_mutex(&thread_mutex,__FILE__,__LINE__);
	if(thread_failure)
	{
		overall_status = delayHandlerStatus_HandlerError;
		return_code = -3;
	}
	unlock_mutex(&thread_mutex,__FILE__,__LINE__);
	return return_code;
}




int_fast32_t DelayHandlerBase::wait_for_threads_sleeping() throw()
{
	int_fast32_t return_code = 0;
	if(overall_status==delayHandlerStatus_HandlerStopped)
	{
		return -1;
	}
	if(overall_status==delayHandlerStatus_HandlerError)
	{
		return -2;
	}
	lock_mutex(&thread_mutex,__FILE__,__LINE__);
	while(num_running < NUM_THREADS)
	{
		unlock_mutex(&thread_mutex,__FILE__,__LINE__);
		yield(__FILE__,__LINE__);
		lock_mutex(&thread_mutex,__FILE__,__LINE__);
	}
	// All threads running.  Now tell them to sleep
	action=delayHandlerAction_Sleep;
	while(num_sleeping < NUM_THREADS) {
		condition_wait(&sleeping_condition, &thread_mutex, __FILE__, __LINE__);
	}
	if(thread_failure)
	{
		overall_status = delayHandlerStatus_HandlerError;
		return_code = -3;
	}
	unlock_mutex(&thread_mutex,__FILE__,__LINE__);
	return return_code;
}

int_fast32_t DelayHandlerBase::go_to_work(enum delayHandlerThreadActionEnum action_) throw()
{
	{
		int_fast32_t retval = check_status_before_working();
		if((retval)) {
			return retval;
		}
		if(num_running < NUM_THREADS)
		{
			DHBfprintf(stderr, "Error: num_running (%"PRIdFAST32") < NUM_THREADS (%"PRIdFAST32") in DelayHandlerBase::go_to_work\n", num_running, NUM_THREADS);
			return -101;
		}
		if(num_sleeping < NUM_THREADS)
		{
			DHBfprintf(stderr, "Error: num_sleeping (%"PRIdFAST32") < NUM_THREADS (%"PRIdFAST32") in DelayHandlerBase::go_to_work\n", num_sleeping, NUM_THREADS);
			return -102;
		}
	}
	if(verbosity >= 5)
	{
		DHBfprintf(stderr, "Starting DelayHandlerBase work command#=%"PRIuFAST64" with action=%d\n", commands_processed, int(action_));
	}
	lock_mutex(&thread_mutex,__FILE__,__LINE__);
	action=action_;
	num_running=0;
	num_sleeping=0;
	broadcast_condition(&work_condition,__FILE__,__LINE__);
	unlock_mutex(&thread_mutex,__FILE__,__LINE__);
	{
		int_fast32_t retval = wait_for_threads_sleeping();
		if((retval)) {
			if(verbosity >= 3)
			{
				DHBfprintf(stderr, "Error encountered during DelayHandlerBase work command#=%"PRIuFAST64" with action=%d\n", commands_processed, int(action_));
			}
			overall_status=delayHandlerStatus_HandlerError;
			return retval;
		}
	}
	if(verbosity >= 5)
	{
		DHBfprintf(stderr, "Finished DelayHandlerBase work command#=%"PRIuFAST64" with action=%d\n", commands_processed, int(action_));
	}
	++commands_processed;
	return 0;
}


int_fast32_t DelayHandlerBase::init_threads() throw()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running init_threads, overall_status=%d thread_failure=%d\n", int(overall_status), int(thread_failure));
	}
	{
		int_fast32_t retval = check_status_before_working();
		if((retval)) {
			return retval;
		}
	}
	verbose = verbosity;
	assign_antennas(0);
	int_fast32_t retval = go_to_work(delayHandlerAction_Init);
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Finished init_threads with retval=%"PRIdFAST32", overall_status=%d thread_failure=%d\n", retval, int(overall_status), int(thread_failure));
	}
	return retval;
}
int_fast32_t DelayHandlerBase::test_delay_service(const int_fast32_t verbose_) throw()
{
	int_fast32_t retval = 0;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running DelayHandlerBase::test_delay_service, overall_status=%d thread_failure=%d\n", int(overall_status), int(thread_failure));
	}
	{
		int_fast32_t retval = check_status_before_working();
		if((retval)) {
			return retval;
		}
	}
	verbose = verbose_;
	assign_antennas(2);
	retval = go_to_work(delayHandlerAction_TestDelays);
	if(retval < 0)
	{
		goto test_delay_service_error;
	}
test_delay_service_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Finished DelayHandlerBase::test_delay_service with retval=%"PRIdFAST32", overall_status=%d thread_failure=%d\n", retval, int(overall_status), int(thread_failure));
	}
	return retval;
}
int_fast32_t DelayHandlerBase::test_parameter_service(const int_fast32_t verbose_) throw()
{
	int_fast32_t retval = 0;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running DelayHandlerBase::test_parameter_service, overall_status=%d thread_failure=%d\n", int(overall_status), int(thread_failure));
	}
	{
		int_fast32_t retval = check_status_before_working();
		if((retval)) {
			return retval;
		}
	}
	verbose = verbose_;
	assign_antennas(2);
	retval = go_to_work(delayHandlerAction_TestParameters);
	if(retval < 0)
	{
		goto test_parameter_service_error;
	}
test_parameter_service_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Finished DelayHandlerBase::test_parameter_service with retval=%"PRIdFAST32", overall_status=%d thread_failure=%d\n", retval, int(overall_status), int(thread_failure));
	}
	return retval;
}
int_fast32_t DelayHandlerBase::process_delay_service(const DifxInput* const D_, const DifxJob* const job_, const DifxScan* const scan_, const struct SERVER_MODEL_DELAY_ARGUMENT* const argument_, struct SERVER_MODEL_DELAY_RESPONSE* const response_, const int_fast32_t verbose_) throw()
{
	int_fast32_t retval = 0;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running DelayHandlerBase::process_delay_service, overall_status=%d thread_failure=%d\n", int(overall_status), int(thread_failure));
	}
	if(argument_->Num_Stations < 2)
	{
		DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: Number of stations (%"PRIu32") is not at least 2\n", argument_->Num_Stations);
        retval = -400;
        goto process_delay_service_error;
	}
	if((argument_->Num_Stations != response_->Num_Stations) || (argument_->Num_Sources != response_->Num_Sources))
	{
		DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: Input number of stations/sources (%"PRIu32"/%"PRIu32") does not match output (%"PRIu32"/%"PRIu32")\n", argument_->Num_Stations, argument_->Num_Sources, response_->Num_Stations, response_->Num_Sources);
        retval = -401;
        goto process_delay_service_error;
	}
	{
		int_fast32_t retval = check_status_before_working();
		if((retval)) {
			return retval;
		}
	}
	D = D_;
	job = job_;
	scan = scan_;
	data_input = reinterpret_cast<const char*>(argument_);
	data_output = reinterpret_cast<char*>(response_);
	verbose = verbose_;
	assign_antennas(argument_->Num_Stations);
	if(verbose >= 3)
	{
		print_SERVER_MODEL_DELAY_ARGUMENT();
	}
	retval = go_to_work(delayHandlerAction_GetDelays);
	response_->handler_error = int32_t(retval);
	if(retval < 0)
	{
		goto process_delay_service_error;
	}
    /* Sanity check */
    if((response_->handler_error < 0)
           || (response_->rpc_handler_error < 0)
           || (response_->server_error < 0)
           || (response_->model_error < 0)
            )
    {
            DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: fatal error from delay server (%"PRId32" %"PRId32" %"PRId32" %"PRId32")\n", response_->handler_error, response_->rpc_handler_error, response_->server_error, response_->model_error);
        retval = -300;
        goto process_delay_service_error;
    }
    else if((response_->date != argument_->date)
           || (response_->time != argument_->time)
           || (response_->utc_second != argument_->utc_second)
           || (response_->utc_second_fraction != argument_->utc_second_fraction)
           || (response_->tai_second != argument_->tai_second)
           || (response_->tai_second_fraction != argument_->tai_second_fraction)
            )
    {
        DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: response date does not match argument (%"PRId64" %.16E    %"PRId64" %.16E)\n", response_->date, response_->time, argument_->date, argument_->time);
        DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: response date does not match argument (%"PRId64" %.16E    %"PRId64" %.16E)\n", response_->utc_second, response_->utc_second_fraction, argument_->utc_second, argument_->utc_second_fraction);
        DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: response date does not match argument (%"PRId64" %.16E    %"PRId64" %.16E)\n", response_->tai_second, response_->tai_second_fraction, argument_->tai_second, argument_->tai_second_fraction);
        retval = -301;
        goto process_delay_service_error;
    }
    else if((response_->Num_Stations != argument_->Num_Stations)
      || (response_->Num_Sources != argument_->Num_Sources))
    {
        DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: response number stations/sources does not match argument\n");
        retval = -302;
        goto process_delay_service_error;
    }
    else if(response_->request_id != argument_->request_id)
    {
        DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: response request_id (%"PRId64") does not match expected value (%"PRId64")\n", response_->request_id, argument_->request_id);
        retval = -303;
        goto process_delay_service_error;
    }
    else if(response_->server_struct_setup_code != argument_->server_struct_setup_code)
    {
        DHBfprintf(stderr, "Error: DelayHandlerBase::process_delay_service: Unknown server_struct_setup_code, wanted 0x%"PRIX64" got 0x%"PRIX64"\n", argument_->server_struct_setup_code, response_->server_struct_setup_code);
        retval = -304;
        goto process_delay_service_error;
    }
	if(verbose >= 3)
	{
		print_SERVER_MODEL_DELAY_RESPONSE();
	}
process_delay_service_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Finished DelayHandlerBase::process_delay_service with retval=%"PRIdFAST32", overall_status=%d thread_failure=%d\n", retval, int(overall_status), int(thread_failure));
	}
	return retval;
}
int_fast32_t DelayHandlerBase::process_parameter_service(const DifxInput* const D_, const DifxJob* const job_, const DifxScan* const scan_, const struct SERVER_MODEL_PARAMETERS_ARGUMENT* const argument_, struct SERVER_MODEL_PARAMETERS_RESPONSE* const response_, const int_fast32_t verbose_) throw()
{
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Running process_parameter_service, overall_status=%d thread_failure=%d\n", int(overall_status), int(thread_failure));
	}
	{
		int_fast32_t retval = check_status_before_working();
		if((retval)) {
			return retval;
		}
	}
	D = D_;
	job = job_;
	scan = scan_;
	data_input = reinterpret_cast<const char*>(argument_);
	data_output = reinterpret_cast<char*>(response_);
	verbose = verbose_;
	assign_antennas(2);
	if(verbose >= 3)
	{
		print_SERVER_MODEL_PARAMETERS_ARGUMENT();
	}
	int_fast32_t retval = go_to_work(delayHandlerAction_GetParameters);
	response_->handler_error = int32_t(retval);
	if(retval < 0)
	{
		goto process_parameter_service_error;
	}
    /* Sanity check */
    if((response_->handler_error < 0)
           || (response_->rpc_handler_error < 0)
           || (response_->server_error < 0)
           || (response_->model_error < 0)
            )
    {
            DHBfprintf(stderr, "Error: DelayHandlerBase::process_parameter_service: fatal error from delay server (%"PRId32" %"PRId32" %"PRId32" %"PRId32")\n", response_->handler_error, response_->rpc_handler_error, response_->server_error, response_->model_error);
        retval = -300;
        goto process_parameter_service_error;
    }
    if(fabs(response_->vlight - 299792458.0) > 1E-3)
    {
	    DHBfprintf(stderr, "ERROR : DelayHandlerBase::process_parameter_service: Delay Server is returning BAD PARAMETER DATA.\n");
        retval = -301;
        goto process_parameter_service_error;
    }
	if(verbose >= 3)
	{
		print_SERVER_MODEL_PARAMETERS_RESPONSE();
	}
process_parameter_service_error:
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Finished process_parameter_service with retval=%"PRIdFAST32", overall_status=%d thread_failure=%d\n", retval, int(overall_status), int(thread_failure));
	}
	return retval;
}












void* DelayHandlerBase::start_thread()
{
	lock_mutex(&thread_mutex,__FILE__,__LINE__);
	if(action != delayHandlerAction_Create)
	{
		unlock_mutex(&thread_mutex,__FILE__,__LINE__);
		return NULL;
	}
	const int_fast32_t tid = num_running;
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "DelayHandlerBase thread %"PRIdFAST32" has started\n", tid);
	}
	thread_status[tid] = delayHandlerStatus_Initializing;
	++num_running;
	while(action != delayHandlerAction_Sleep)
	{
		unlock_mutex(&thread_mutex,__FILE__,__LINE__);
		yield(__FILE__,__LINE__);
		lock_mutex(&thread_mutex,__FILE__,__LINE__);
		if(thread_failure)
		{
			break;
		}
	}	
	thread_status[tid] = delayHandlerStatus_Sleeping;
	++num_sleeping;
	if(num_sleeping>=NUM_THREADS)
	{
		signal_condition(&sleeping_condition,__FILE__,__LINE__);
	}
	return run_thread(tid);
}

void* DelayHandlerBase::run_thread(const int_fast32_t tid)
{
	while(!thread_failure) {
		while(action == delayHandlerAction_Sleep) {
			condition_wait(&work_condition,&thread_mutex,__FILE__,__LINE__);
		}
		enum delayHandlerThreadActionEnum action_copy = action;
		thread_status[tid]=delayHandlerStatus_BusyProcessing;
		++num_running;
		unlock_mutex(&thread_mutex,__FILE__,__LINE__);
		int_fast32_t return_code = do_work(tid, action_copy);
		lock_mutex(&thread_mutex,__FILE__,__LINE__);
		thread_status[tid] = delayHandlerStatus_FinishedProcessing;
		++num_sleeping;
		if(num_sleeping>=NUM_THREADS)
		{
			signal_condition(&sleeping_condition,__FILE__,__LINE__);
		}
		thread_status[tid] = delayHandlerStatus_Sleeping;
		if(return_code < 0)
		{
			thread_failure = true;
			thread_status[tid] = delayHandlerStatus_HandlerError;
		}
		if(action_copy==delayHandlerAction_Stop) {
			break;
		}
		if(action_copy==delayHandlerAction_Panic) {
			break;
		}
		while(action == action_copy)
		{
			unlock_mutex(&thread_mutex,__FILE__,__LINE__);
			yield(__FILE__,__LINE__);
			lock_mutex(&thread_mutex,__FILE__,__LINE__);
			if(thread_failure)
			{
				break;
			}
		}
	}
	thread_status[tid] = delayHandlerStatus_HandlerStopped;
	unlock_mutex(&thread_mutex,__FILE__,__LINE__);
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "Thread %"PRIdFAST32" will terminate itself\n", tid);
	}
	pthread_exit(NULL);
// never reached
	return NULL;
}
// int_fast32_t DelayHandlerBase::do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw()
// {
// 	DHBfprintf(stderr, "ERROR: %s:%d: DelayHandlerBase is intended only as a base class.  Use a derived class instead.  (Function called by thread %"PRIdFAST32".)\n", __FILE__, __LINE__,tid);
// 	exit(EXIT_FAILURE);
// 	return -1;
// }



    






int_fast32_t DelayHandlerBase::stop_threads() throw()
{
	int_fast32_t return_code = go_to_work(delayHandlerAction_Stop);
	for(int_fast32_t thread_id=0; thread_id < NUM_THREADS; ++thread_id)
	{
		if(verbosity >= 5)
		{
			DHBfprintf(stdout, "stopping DelayHandlerBase thread %"PRIdFAST32"\n", thread_id);
		}
		void* join_retval;
		if(thread_status[thread_id] != delayHandlerStatus_ThreadJoined)
		{
			int retval = pthread_join(thread[thread_id], &join_retval);
			if((retval)) {
				DHBperror(errno, "joining thread failed");
				DHBfprintf(stderr, "ERROR: %s:%d: joining thread %"PRIdFAST32" failed with %d\n", __FILE__,__LINE__, thread_id, retval);
			}
			if(join_retval != NULL) {
				DHBfprintf(stderr, "WARNING: %s:%d: thread %"PRIdFAST32" pthread_join returned %p\n", __FILE__,__LINE__, thread_id, join_retval);
			}
			if(verbosity >= 5)
			{
				DHBfprintf(stdout, "thread %"PRIdFAST32" stopped and joined\n", thread_id);
			}
			if(thread_status[thread_id] != delayHandlerStatus_HandlerStopped) {
				return_code = -101;
			}
			thread_status[thread_id] = delayHandlerStatus_ThreadJoined;
		}
		else
		{
			if(verbosity >= 5)
			{
				DHBfprintf(stdout, "thread %"PRIdFAST32" was previously joined\n", thread_id);
			}
		}
	}
	return return_code;
}




void DelayHandlerBase::assign_antennas(const uint32_t NUM_ANT) throw()
{
	if(NUM_ANT == 0)
	{
		for(int_fast32_t tid=0; tid < NUM_THREADS; ++tid)
		{
			stationStart[tid] = 0;
			stationEnd[tid] = 0;
		}
		return;
	}			
	uint_fast32_t NUM_BLOCK(ceil(double(NUM_ANT-1)/NUM_THREADS));
	uint_fast32_t index = 1;
	for(int_fast32_t tid=0; tid < NUM_THREADS; ++tid)
	{
		uint_fast32_t s;
		uint_fast32_t e;
		s = index;
		if(s < NUM_ANT)
		{
			e = s + NUM_BLOCK;
			if(e > NUM_ANT)
			{
				e = NUM_ANT;
			}
		}
		else
		{
			s = 0;
			e = 0;
		}
		stationStart[tid] = s;
		stationEnd[tid] = e;
		index += NUM_BLOCK;
	}
	return;
}




void DelayHandlerBase::DHBfprintf(FILE* fp, const char* const format, ...) throw()
{
	pthread_mutex_lock(&fprintf_mutex);
	va_list args;
	va_start(args, format);
	vfprintf(fp, format, args);
	va_end(args);
	pthread_mutex_unlock(&fprintf_mutex);
	return;
}

void DelayHandlerBase::DHBperror(int errnum, const char* const text) throw()
{
	lock_mutex(&thread_mutex,__FILE__,__LINE__);
	int old = errno;
	errno = errnum;
	perror(text);
	errno = old;
	unlock_mutex(&thread_mutex,__FILE__,__LINE__);
	return;
}


int_fast32_t DelayHandlerBase::start_child_process(const char *command, const char* const* argv, const char* const log_base_name, const int_fast32_t tid) throw()
{
	// Handle broken pipes where the problem happens rather than in
	// a handler.
	int fd_log = open_child_stderr_logfile(log_base_name, tid);
	if(fd_log < 0)
	{
		DHBfprintf(stderr, "Error: %s:%d: call to open_child_stderr_logfile failed\n", __FILE__, __LINE__);
		return -1;
	}
	int fd_write;
	int fd_read;
	pid_t child = pipe_writer_popen(command, argv, fd_log, &fd_write, &fd_read);
	if(child < 0)
	{
		DHBfprintf(stderr, "Error: %s:%d: call to pipe_writer_popen failed\n", __FILE__, __LINE__);
		return -2;
	}
	child_process[tid] = child;
	child_stdin[tid] = fdopen(fd_write, "wb");
	child_stdout[tid] = fdopen(fd_read, "rb");
	if((child_stdin[tid] == NULL) || (child_stdout[tid] == NULL))
	{
		DHBfprintf(stderr, "Error: %s:%d: call to fdopen failed for fds %d %d\n", __FILE__, __LINE__, fd_write, fd_read);
		return -3;
	}
	return 0;
}








int DelayHandlerBase::open_child_stdout_logfile(const char* const name, const int_fast32_t tid) throw()
{
	time_t now = time(NULL);
	if(now == (time_t)-1)
	{
		DHBfprintf(stderr, "Error: %s:%d: call to time failed\n", __FILE__, __LINE__);
		now=0;
	}
	struct tm br_time;
	if(localtime_r(&now, &br_time) == NULL)
	{
		DHBfprintf(stderr, "Error: %s:%d: call to gmtime_r failed\n", __FILE__, __LINE__);
		br_time.tm_year=100;
		br_time.tm_mon=0;
		br_time.tm_mday=1;
		br_time.tm_hour=0;
		br_time.tm_min=0;
		br_time.tm_sec=0;
	}
	const size_t DATE_SIZE=32;
	char date_str[DATE_SIZE];
	size_t date_retval = strftime(date_str, DATE_SIZE, "%FT%H:%M:%S", &br_time);
	if(date_retval == 0)
	{
		DHBfprintf(stderr, "Error: %s:%d: invalid strftime call\n", __FILE__, __LINE__);
		date_str[0] = 0;
	}

    // do a test print to see how many characters we need
    int snprintf_size;
    char dummy_buffer[1];
	snprintf_size = snprintf(dummy_buffer,1,"%s/%s_%s_%06"PRIdLEAST64"_%03"PRIdFAST32"_stdout.log", TMP_DIRECTORY, name, date_str, parent_pid, tid);
    if(snprintf_size < 0) {
        DHBfprintf(stderr, "Error: %s:%d: could not expand logfile name\n", __FILE__, __LINE__);
		return -1;
    }
    size_t BYTES_NEEDED = snprintf_size+1;
    char* logfile = reinterpret_cast<char*>(malloc(BYTES_NEEDED));
    if(logfile == NULL) {
	    DHBfprintf(stderr, "ERROR: %s:%d: could not malloc %zu bytes for logfile\n", __FILE__, __LINE__, BYTES_NEEDED);
		return -1;
    }
    // now re-print command line
	snprintf_size = snprintf(logfile,BYTES_NEEDED,"%s/%s_%s_%06"PRIdLEAST64"_%03"PRIdFAST32"_stdout.log", TMP_DIRECTORY, name, date_str, parent_pid, tid);
    if(snprintf_size < ssize_t(BYTES_NEEDED)-1) {
        DHBfprintf(stderr, "Error: %s:%d: could not expand logfile string\n", __FILE__, __LINE__);
		free(reinterpret_cast<void*>(logfile));
		return -1;
    }
    else if(snprintf_size >= ssize_t(BYTES_NEEDED)) {
        DHBfprintf(stderr, "Error: %s:%d: error in second string expansion\n", __FILE__, __LINE__);
		free(reinterpret_cast<void*>(logfile));
		return -1;
    }
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" has child process expanded logfile '%s'\n", tid, logfile);
	}
	
    // Now open the file for append
    int fd = open(logfile, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if(fd < 0) {
        DHBperror(errno, "could not open child process logfile");
        DHBfprintf(stderr, "could not open child process logfile '%s'\n", logfile);
        free(reinterpret_cast<void*>(logfile)); logfile=0;
        return fd;
    };
    free(reinterpret_cast<void*>(logfile)); logfile=0;
	
	return fd;
}


int DelayHandlerBase::open_child_stderr_logfile(const char* const name, const int_fast32_t tid) throw()
{
	time_t now = time(NULL);
	if(now == (time_t)-1)
	{
		DHBfprintf(stderr, "Error: %s:%d: call to time failed\n", __FILE__, __LINE__);
		now=0;
	}
	struct tm br_time;
	if(localtime_r(&now, &br_time) == NULL)
	{
		DHBfprintf(stderr, "Error: %s:%d: call to gmtime_r failed\n", __FILE__, __LINE__);
		br_time.tm_year=100;
		br_time.tm_mon=0;
		br_time.tm_mday=1;
		br_time.tm_hour=0;
		br_time.tm_min=0;
		br_time.tm_sec=0;
	}
	const size_t DATE_SIZE=32;
	char date_str[DATE_SIZE];
	size_t date_retval = strftime(date_str, DATE_SIZE, "%FT%H:%M:%S", &br_time);
	if(date_retval == 0)
	{
		DHBfprintf(stderr, "Error: %s:%d: invalid strftime call\n", __FILE__, __LINE__);
		date_str[0] = 0;
	}

    // do a test print to see how many characters we need
    int snprintf_size;
    char dummy_buffer[1];
	snprintf_size = snprintf(dummy_buffer,1,"%s/%s_%s_%06"PRIdLEAST64"_%03"PRIdFAST32"_stderr.log", TMP_DIRECTORY, name, date_str, parent_pid, tid);
    if(snprintf_size < 0) {
        DHBfprintf(stderr, "Error: %s:%d: could not expand logfile name\n", __FILE__, __LINE__);
		return -1;
    }
    size_t BYTES_NEEDED = snprintf_size+1;
    char* logfile = reinterpret_cast<char*>(malloc(BYTES_NEEDED));
    if(logfile == NULL) {
	    DHBfprintf(stderr, "ERROR: %s:%d: could not malloc %zu bytes for logfile\n", __FILE__, __LINE__, BYTES_NEEDED);
		return -1;
    }
    // now re-print command line
	snprintf_size = snprintf(logfile,BYTES_NEEDED,"%s/%s_%s_%06"PRIdLEAST64"_%03"PRIdFAST32"_stderr.log", TMP_DIRECTORY, name, date_str, parent_pid, tid);
    if(snprintf_size < ssize_t(BYTES_NEEDED)-1) {
        DHBfprintf(stderr, "Error: %s:%d: could not expand logfile string\n", __FILE__, __LINE__);
		free(reinterpret_cast<void*>(logfile));
		return -1;
    }
    else if(snprintf_size >= ssize_t(BYTES_NEEDED)) {
        DHBfprintf(stderr, "Error: %s:%d: error in second string expansion\n", __FILE__, __LINE__);
		free(reinterpret_cast<void*>(logfile));
		return -1;
    }
	if(verbosity >= 5)
	{
		DHBfprintf(stdout, "thread %"PRIdFAST32" has child process expanded logfile '%s'\n", tid, logfile);
	}
	
    // Now open the file for append
    int fd = open(logfile, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if(fd < 0) {
        DHBperror(errno, "could not open child process logfile");
        DHBfprintf(stderr, "could not open child process logfile '%s'\n", logfile);
    };
    free(reinterpret_cast<void*>(logfile)); logfile=0;
	
	return fd;
}

void DelayHandlerBase::checkForTmpDirectory() throw()
{
	int retval = mkdir(TMP_DIRECTORY, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if(retval == 0)
	{
		// Great!
	}
	else {
		if(errno == EEXIST)
		{
			// Already there!
		}
		else
		{
			DHBfprintf(stderr, "Error: unable to create directory '%s'\n", TMP_DIRECTORY);
		}
	}
	return;
}


void DelayHandlerBase::print_SERVER_MODEL_DELAY_ARGUMENT() throw()
{
	const struct SERVER_MODEL_DELAY_ARGUMENT* const arg(reinterpret_cast<const struct SERVER_MODEL_DELAY_ARGUMENT*>(data_input));
	
	DHBfprintf(stdout,"DelayHandlerBase:Delay Argument\n");
	DHBfprintf(stdout, "DelayHandlerBase: request arg: request_id=0x%"PRIX64" server_struct_setup_code=0x%"PRIX64"\n", arg->request_id, arg->server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerBase: request arg: date=%"PRId64" time=%16.12f\n", arg->date, arg->time);
	DHBfprintf(stdout, "DelayHandlerBase: request arg: utc_second=%"PRId64" utc_second_fraction=%18.16f\n", arg->utc_second, arg->utc_second_fraction);
	DHBfprintf(stdout, "DelayHandlerBase: request arg: tai_second=%"PRId64" tai_second_fraction=%18.16f\n", arg->tai_second, arg->tai_second_fraction);
	if(verbose >= 4) {
		for(uint_fast8_t k=0; k < NUM_DIFX_DELAYHANDLERDISTRIBUTOR_KFLAGS; k++) {
			DHBfprintf(stdout, "DelayHandlerBase: request arg: kflag[%02"PRIuFAST8"]=%"PRId16"\n", k, arg->kflags[k]);
		}
	}
	DHBfprintf(stdout, "DelayHandlerBase: request arg: verbosity=%"PRId32" ref_frame=%"PRId32"\n", arg->verbosity, arg->ref_frame);
	DHBfprintf(stdout, "DelayHandlerBase: request arg: sky_frequency = %E\n", arg->sky_frequency);
	DHBfprintf(stdout, "DelayHandlerBase: Station information\n");
	DHBfprintf(stdout, "DelayHandlerBase: request arg: Use_Server_Station_Table=%"PRId32" Num_Stations=%"PRIu32"\n", arg->Use_Server_Station_Table, arg->Num_Stations);
	for(uint_fast32_t s=0; s < arg->Num_Stations; s++) {
		char ID0, ID1;
		const double* v;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_name='%s'\n", s, arg->station[s].station_name);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" antenna_name='%s'\n", s, arg->station[s].antenna_name);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" site_name=   '%s'\n", s, arg->station[s].site_name);
		ID0 = (char)(arg->station[s].site_ID&0xFF);
		ID1 = (char)(arg->station[s].site_ID>>8);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" site_ID=     '%c%c' 0x%04"PRIX16"\n", s, ID0, ID1, arg->station[s].site_ID);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" site_type=   '%s'\n", s, arg->station[s].site_type);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" axis_type=   '%s'\n", s, arg->station[s].axis_type);
		v = arg->station[s].station_pos;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_pos= [%14.4f, %14.4f, %14.4f]\n", s, v[0], v[1], v[2]);
		v = arg->station[s].station_vel;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_vel= [%14.9E, %14.9E, %14.9E]\n", s, v[0], v[1], v[2]);
		v = arg->station[s].station_acc;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_acc= [%14.9E, %14.9E, %14.9E]\n", s, v[0], v[1], v[2]);
		v = arg->station[s].station_pointing_dir;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_pointing_dir=  [%14.9E, %14.9E, %14.9E]\n", s, v[0], v[1], v[2]);
		v = arg->station[s].station_reference_dir;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_reference_dir= [%14.9E, %14.9E, %14.9E]\n", s, v[0], v[1], v[2]);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_coord_frame=%"PRId32"\n", s, arg->station[s].station_coord_frame);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" pointing_coord_frame=%"PRId32"\n", s, arg->station[s].pointing_coord_frame);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" pointing_corrections_applied=%d\n", s, arg->station[s].pointing_corrections_applied);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" station_position_delay_offset=%E\n", s, arg->station[s].station_position_delay_offset);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" axis_off=%7.4f primary_axis_wrap=%2d secondary_axis_wrap=%2d\n", s, arg->station[s].axis_off, arg->station[s].primary_axis_wrap, arg->station[s].secondary_axis_wrap);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" receiver_name='%s'\n", s, arg->station[s].receiver_name);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" pressure=%12.3E antenna_pressure=%12.3E temperature=%6.1f\n", s, arg->station[s].pressure, arg->station[s].antenna_pressure, arg->station[s].temperature);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: station=%02"PRIuFAST32" wind_speed=%6.1f wind_direction=%7.2f antenna_phys_temperature=%6.1f\n", s, arg->station[s].wind_speed, arg->station[s].wind_direction, arg->station[s].antenna_phys_temperature);
	}
	DHBfprintf(stdout, "DelayHandlerBase: Source information\n");
	DHBfprintf(stdout, "DelayHandlerBase: request arg: Use_Server_Source_Table=%"PRId32" Num_Sources=%"PRIu32"\n", arg->Use_Server_Source_Table, arg->Num_Sources);
	for(uint_fast32_t s=0; s < arg->Num_Sources; s++) {
		const double* v;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_name='%s'\n", s, arg->source[s].source_name);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" IAU_name=   '%s'\n", s, arg->source[s].IAU_name);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_type='%s'\n", s, arg->source[s].source_type);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" ra=           %20.16f\n", s, arg->source[s].ra);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" dec=          %20.16f\n", s, arg->source[s].dec);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" dra=          %20.10f\n", s, arg->source[s].dra);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" ddec=         %20.10f\n", s, arg->source[s].ddec);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" depoch=       %20.16f\n", s, arg->source[s].depoch);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" parallax=     %20.3f\n", s, arg->source[s].parallax);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" coord_frame=%"PRId32"\n", s, arg->source[s].coord_frame);
		v = arg->source[s].source_pos;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_pos= [%24.16E, %24.16E, %24.16E]\n", s, v[0], v[1], v[2]);
		v = arg->source[s].source_vel;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_vel= [%24.16E, %24.16E, %24.16E]\n", s, v[0], v[1], v[2]);
		v = arg->source[s].source_acc;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_acc= [%24.16E, %24.16E, %24.16E]\n", s, v[0], v[1], v[2]);
		v = arg->source[s].source_pointing_dir;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_pointing_dir=           [%24.16E, %24.16E, %24.16E]\n", s, v[0], v[1], v[2]);
		v = arg->source[s].source_pointing_reference_dir;
		DHBfprintf(stdout, "DelayHandlerBase: request arg: source=%02"PRIuFAST32" source_pointing_reference_dir= [%24.16E, %24.16E, %24.16E]\n", s, v[0], v[1], v[2]);
	}
	DHBfprintf(stdout, "DelayHandlerBase: EOP information\n");
	DHBfprintf(stdout, "DelayHandlerBase: request arg: Use_Server_EOP_Table=%"PRId32" Num_EOPs=%"PRIu32"\n", arg->Use_Server_EOP_Table, arg->Num_EOPs);
	for(uint_fast32_t e=0; e < arg->Num_EOPs; e++) {
		DHBfprintf(stdout, "DelayHandlerBase: request arg: EOP=%02"PRIuFAST32" EOP_time=  %20.11f\n", e, arg->EOP[e].EOP_time);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: EOP=%02"PRIuFAST32" tai_utc=   %20.12f\n", e, arg->EOP[e].tai_utc);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: EOP=%02"PRIuFAST32" ut1_utc=   %20.12f\n", e, arg->EOP[e].ut1_utc);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: EOP=%02"PRIuFAST32" xpole=     %10.6f\n", e, arg->EOP[e].xpole);
		DHBfprintf(stdout, "DelayHandlerBase: request arg: EOP=%02"PRIuFAST32" ypole=     %10.6f\n", e, arg->EOP[e].ypole);
	}
	return;
}

void DelayHandlerBase::print_SERVER_MODEL_DELAY_RESPONSE() throw()
{
	const struct SERVER_MODEL_DELAY_RESPONSE* const res(reinterpret_cast<const struct SERVER_MODEL_DELAY_RESPONSE*>(data_output));

	DHBfprintf(stdout, "DelayHandlerBase:Delay Response\n");
	DHBfprintf(stdout, "DelayHandlerBase:response res: handler_error=%"PRId32"  rpc_handler_error=%"PRId32" server_error=%"PRId32" model_error=%"PRId32"\n", res->handler_error, res->rpc_handler_error, res->server_error, res->model_error);
	DHBfprintf(stdout, "DelayHandlerBase:response res: request_id=%"PRId64" server_struct_setup_code=0x%"PRIX64"\n", res->request_id, res->server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerBase:response res: server_version=0x%"PRIX64"\n", res->server_version);
	DHBfprintf(stdout, "DelayHandlerBase: response res: date=%"PRId64" time=%16.12f\n", res->date, res->time);
	DHBfprintf(stdout, "DelayHandlerBase: response res: utc_second=%"PRId64" utc_second_fraction=%18.16f\n", res->utc_second, res->utc_second_fraction);
	DHBfprintf(stdout, "DelayHandlerBase: response res: tai_second=%"PRId64" tai_second_fraction=%18.16f\n", res->tai_second, res->tai_second_fraction);
	for(uint_fast32_t st=0; st < res->Num_Stations; ++st)
	{
		for(uint_fast32_t so=0; so < res->Num_Sources; ++so)
		{
			const struct SERVER_MODEL_DELAY_RESPONSE_DATA* result = &(res->result[st*res->Num_Sources+so]);
			const double* v;
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" delay=                %24.16E\n", st, so, result->delay);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" dry_atmos=            %24.16E\n", st, so, result->dry_atmos);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" wet_atmos=            %24.16E\n", st, so, result->wet_atmos);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" iono_atmos=           %24.16E\n", st, so, result->iono_atmos);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" az_corr=              %10.6f\n", st, so, result->az_corr);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" el_corr=              %10.6f\n", st, so, result->el_corr);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" az_geom=              %10.6f\n", st, so, result->az_geom);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" el_geom=              %10.6f\n", st, so, result->el_geom);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" primary_axis_angle=   %10.6f\n", st, so, result->primary_axis_angle);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" secondary_axis_angle= %10.6f\n", st, so, result->secondary_axis_angle);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" mount_source_angle=   %10.6f\n", st, so, result->mount_source_angle);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" station_antenna_theta=%10.6f\n", st, so, result->station_antenna_theta);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" station_antenna_phi=  %10.6f\n", st, so, result->station_antenna_phi);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" source_antenna_theta= %10.6f\n", st, so, result->source_antenna_theta);
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" source_antenna_phi=   %10.6f\n", st, so, result->source_antenna_phi);
			v = result->UVW;
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" UVW =           [%24.16E, %24.16E, %24.16E]\n", st, so, v[0], v[1], v[2]);
			v = result->baselineP2000;
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" baselineP2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, v[0], v[1], v[2]);
			v = result->baselineV2000;
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" baselineV2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, v[0], v[1], v[2]);
			v = result->baselineA2000;
			DHBfprintf(stdout, "DelayHandlerBase:request res: station=%02"PRIuFAST32" source=%02"PRIuFAST32" baselineA2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, v[0], v[1], v[2]);
		}
	}
	return;
}

void DelayHandlerBase::print_SERVER_MODEL_PARAMETERS_ARGUMENT() throw()
{
	const struct SERVER_MODEL_PARAMETERS_ARGUMENT* const arg(reinterpret_cast<const struct SERVER_MODEL_PARAMETERS_ARGUMENT*>(data_input));
	
	DHBfprintf(stdout, "DelayHandlerBase:Parameters Argument\n");
	DHBfprintf(stdout, "DelayHandlerBase: request arg: request_id=0x%"PRIX64" server_struct_setup_code=0x%"PRIX64"\n", arg->request_id, arg->server_struct_setup_code);
	DHBfprintf(stdout, "DelayHandlerBase: request arg: verbosity=%"PRId32"\n", arg->verbosity);
	return;
}
void DelayHandlerBase::print_SERVER_MODEL_PARAMETERS_RESPONSE() throw()
{
	const struct SERVER_MODEL_PARAMETERS_RESPONSE* const res(reinterpret_cast<const struct SERVER_MODEL_PARAMETERS_RESPONSE*>(data_output));

	DHBfprintf(stdout, "DelayHandlerBase:Parameters Response\n");
	DHBfprintf(stdout, "DelayHandlerBase: results: handler_error=%"PRId32" rpc_handler_error=%"PRId32" server_error=%"PRId32" model_error=%"PRId32"\n", res->handler_error, res->rpc_handler_error, res->server_error, res->model_error);
	DHBfprintf(stdout, "DelayHandlerBase: results: request_id=%"PRId64"\n", res->request_id);
	DHBfprintf(stdout, "DelayHandlerBase: results: server_struct_setup_code=0x%"PRIX64" server_version=0x%"PRIX64"\n", res->server_struct_setup_code, res->server_version);
	DHBfprintf(stdout, "DelayHandlerBase: results: accelgrv= %25.16E\n", res->accelgrv);
	DHBfprintf(stdout, "DelayHandlerBase: results: e_flat=   %25.16E\n", res->e_flat);
	DHBfprintf(stdout, "DelayHandlerBase: results: earthrad= %25.16E\n", res->earthrad);
	DHBfprintf(stdout, "DelayHandlerBase: results: mmsems=   %25.16E\n", res->mmsems);
	DHBfprintf(stdout, "DelayHandlerBase: results: ephepoc=  %25.16E\n", res->ephepoc);
	DHBfprintf(stdout, "DelayHandlerBase: results: gauss=    %25.16E\n", res->gauss);
	DHBfprintf(stdout, "DelayHandlerBase: results: u_grv_cn= %25.16E\n", res->u_grv_cn);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmsun=    %25.16E\n", res->gmsun);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmmercury=%25.16E\n", res->gmmercury);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmvenus=  %25.16E\n", res->gmvenus);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmearth=  %25.16E\n", res->gmearth);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmmoon=   %25.16E\n", res->gmmoon);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmmars=   %25.16E\n", res->gmmars);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmjupiter=%25.16E\n", res->gmjupiter);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmsaturn= %25.16E\n", res->gmsaturn);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmuranus= %25.16E\n", res->gmuranus);
	DHBfprintf(stdout, "DelayHandlerBase: results: gmneptune=%25.16E\n", res->gmneptune);
	DHBfprintf(stdout, "DelayHandlerBase: results: etidelag= %25.16E\n", res->etidelag);
	DHBfprintf(stdout, "DelayHandlerBase: results: love_h=   %25.16E\n", res->love_h);
	DHBfprintf(stdout, "DelayHandlerBase: results: love_l=   %25.16E\n", res->love_l);
	DHBfprintf(stdout, "DelayHandlerBase: results: pre_data= %25.16E\n", res->pre_data);
	DHBfprintf(stdout, "DelayHandlerBase: results: rel_data= %25.16E\n", res->rel_data);
	DHBfprintf(stdout, "DelayHandlerBase: results: tidalut1= %25.16E\n", res->tidalut1);
	DHBfprintf(stdout, "DelayHandlerBase: results: au=       %25.16E\n", res->au);
	DHBfprintf(stdout, "DelayHandlerBase: results: tsecau=   %25.16E\n", res->tsecau);
	DHBfprintf(stdout, "DelayHandlerBase: results: vlight=   %25.16E\n", res->vlight);
	return;
}









// The following popen code is taken from
// https://gist.github.com/nitrogenlogic/1022231
// with 0->STDIN_FILENO, 1->STDOUT_FILENO, 2->STDERR_FILENO
// /*
//  * This implementation of popen3() was created from scratch in June of 2011. It
//  * is less likely to leak file descriptors if an error occurs than the 2007
//  * version and has been tested under valgrind. It also differs from the 2007
//  * version in its behavior if one of the file descriptor parameters is NULL.
//  * Instead of closing the corresponding stream, it is left unmodified (typically
//  * sharing the same terminal as the parent process). It also lacks the
//  * non-blocking option present in the 2007 version.
//  *
//  * No warranty of correctness, safety, performance, security, or usability is
//  * given. This implementation is released into the public domain, but if used
//  * in an open source application, attribution would be appreciated.
//  *
//  * Mike Bourgeous
//  * https://github.com/nitrogenlogic
//  */
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <fcntl.h>

// /*
//  * Sets the FD_CLOEXEC flag. Returns 0 on success, -1 on error.
//  */
// static int set_cloexec(int fd)
// {
//     if(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) == -1) {
//         perror("Error setting FD_CLOEXEC flag");
//         return -1;
//     }

//     return 0;
// }

// /*
//  * Runs command in another process, with full remote interaction capabilities.
//  * Be aware that command is passed to sh -c, so shell expansion will occur.
//  * Writing to *writefd will write to the command's stdin. Reading from *readfd
//  * will read from the command's stdout. Reading from *errfd will read from the
//  * command's stderr. If NULL is passed for writefd, readfd, or errfd, then the
//  * command's stdin, stdout, or stderr will not be changed. Returns the child
//  * PID on success, -1 on error.
//  */
// pid_t popen3(char *command, int *writefd, int *readfd, int *errfd)
// {
//     int in_pipe[2] = {-1, -1};
//     int out_pipe[2] = {-1, -1};
//     int err_pipe[2] = {-1, -1};
//     pid_t pid;

// // 2011 implementation of popen3() by Mike Bourgeous
// // https://gist.github.com/1022231

//     if(command == NULL) {
//         fprintf(stderr, "Cannot popen3() a NULL command.\n");
//         goto error;
//     }

//     if(writefd && pipe(in_pipe)) {
//         perror("Error creating pipe for stdin");
//         goto error;
//     }
//     if(readfd && pipe(out_pipe)) {
//         perror("Error creating pipe for stdout");
//         goto error;
//     }
//     if(errfd && pipe(err_pipe)) {
//         perror("Error creating pipe for stderr");
//         goto error;
//     }

//     pid = fork();
//     switch(pid) {
//     case -1:
// // Error
//         perror("Error creating child process");
//         goto error;

//     case 0:
// // Child
//         if(writefd) {
//             close(in_pipe[1]);
//             if(dup2(in_pipe[0], STDIN_FILENO) == -1) {
//                 perror("Error assigning stdin in child process");
//                 exit(-1);
//             }
//             close(in_pipe[0]);
//         }
//         if(readfd) {
//             close(out_pipe[0]);
//             if(dup2(out_pipe[1], STDOUT_FILENO) == -1) {
//                 perror("Error assigning stdout in child process");
//                 exit(-1);
//             }
//             close(out_pipe[1]);
//         }
//         if(errfd) {
//             close(err_pipe[0]);
//             if(dup2(err_pipe[1], STDERR_FILENO) == -1) {
//                 perror("Error assigning stderr in child process");
//                 exit(-1);
//             }
//             close(err_pipe[1]);
//         }
//         execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
//         perror("Error executing command in child process");
//         exit(-1);

//     default:
// // Parent
//         break;
//     }

//     if(writefd) {
//         close(in_pipe[0]);
//         set_cloexec(in_pipe[1]);
//         *writefd = in_pipe[1];
//     }
//     if(readfd) {
//         close(out_pipe[1]);
//         set_cloexec(out_pipe[0]);
//         *readfd = out_pipe[0];
//     }
//     if(errfd) {
//         close(err_pipe[1]);
//         set_cloexec(out_pipe[0]);
//         *errfd = err_pipe[0];
//     }

//     return pid;

// error:
//     if(in_pipe[0] >= 0) {
//         close(in_pipe[0]);
//     }
//     if(in_pipe[1] >= 0) {
//         close(in_pipe[1]);
//     }
//     if(out_pipe[0] >= 0) {
//         close(out_pipe[0]);
//     }
//     if(out_pipe[1] >= 0) {
//         close(out_pipe[1]);
//     }
//     if(err_pipe[0] >= 0) {
//         close(err_pipe[0]);
//     }
//     if(err_pipe[1] >= 0) {
//         close(err_pipe[1]);
//     }

//     return -1;
// }
/*
 * Sets the FD_CLOEXEC flag. Returns 0 on success, -1 on error.
 */
namespace {
int set_cloexec(int fd) throw()
{
	if(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) == -1) {
		perror("Error setting FD_CLOEXEC flag");
		return -1;
	}

    return 0;
}
}

/*
 * Runs command in another process, with stdin and stdout interaction
 * through pipes, and the child stderr redirected to a file descriptor
 * provided by the parent process (so redirect stderr to a file).
 * Writing to *writefd will write to the command's stdin. Reading from *readfd
 * will read from the command's stdout. Returns the child
 * PID on success, -1 on error.
 */
pid_t DelayHandlerBase::pipe_writer_popen(const char *command, const char* const* argv, int child_stderr_fd, int *writefd, int *readfd) throw()
{
	int in_pipe[2] = {-1, -1};
	int out_pipe[2] = {-1, -1};
	pid_t pid;

    if(command == NULL) {
		DHBfprintf(stderr, "Cannot popen() a NULL command.\n");
		goto error;
	}
	if(argv == NULL) {
		DHBfprintf(stderr, "Cannot popen() a command with NULL argument list pointer.\n");
		goto error;
	}
	if(writefd == NULL) {
		DHBfprintf(stderr, "Will not popen() a command with NULL writefd pointer.\n");
		goto error;
	}
	if(readfd == NULL) {
		DHBfprintf(stderr, "Will not popen() a command with NULL readfd pointer.\n");
		goto error;
	}

    if(pipe(in_pipe)) {
		DHBperror(errno, "Error creating pipe for stdin");
		goto error;
	}
	if(pipe(out_pipe)) {
		DHBperror(errno, "Error creating pipe for stdout");
		goto error;
	}

    pid = fork();
	switch(pid) {
	case -1:
// Error
		perror("Error creating child process");
		goto error;

    case 0:
// Child
		{
			close(in_pipe[1]);
			if(dup2(in_pipe[0], STDIN_FILENO) == -1) {
				DHBperror(errno, "Error assigning stdin in child process");
				exit(EXIT_FAILURE);
			}
			close(in_pipe[0]);
			close(out_pipe[0]);
			if(dup2(out_pipe[1], STDOUT_FILENO) == -1) {
				DHBperror(errno, "Error assigning stdout in child process");
				exit(EXIT_FAILURE);
			}
			close(out_pipe[1]);
			if(child_stderr_fd >= 0) {
				if(dup2(child_stderr_fd, STDERR_FILENO) == -1) {
					DHBperror(errno, "Error assigning stderr in child process");
					exit(EXIT_FAILURE);
				}
				close(child_stderr_fd);
			}
		}
		execvp(command, const_cast<char* const*>(argv));
		DHBperror(errno, "Error executing command in child process");
		exit(EXIT_FAILURE);

    default:
// Parent
		break;
	}

    {
		close(in_pipe[0]);
		set_cloexec(in_pipe[1]);
		*writefd = in_pipe[1];

        close(out_pipe[1]);
		set_cloexec(out_pipe[0]);
		*readfd = out_pipe[0];
	}

    return pid;

error:
	if(in_pipe[0] >= 0) {
		close(in_pipe[0]);
	}
	if(in_pipe[1] >= 0) {
		close(in_pipe[1]);
	}
	if(out_pipe[0] >= 0) {
		close(out_pipe[0]);
	}
	if(out_pipe[1] >= 0) {
		close(out_pipe[1]);
	}
	if(child_stderr_fd >= 0) {
		close(child_stderr_fd);
	}

    return -1;
}





}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX


