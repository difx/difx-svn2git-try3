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


#ifndef DelayHandlerBase_h
#define DelayHandlerBase_h

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
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include <difxio.h>
#include "DelayHandlerDistributorInterface.h"


namespace DiFX {
namespace Delay {
namespace Handler {

enum delayHandlerThreadStatusEnum
    {
        delayHandlerStatus_Initializing=0,
        delayHandlerStatus_Sleeping,
        delayHandlerStatus_BusyProcessing,
        delayHandlerStatus_FinishedProcessing,
        delayHandlerStatus_HandlerStopped,
        delayHandlerStatus_HandlerError,
        delayHandlerStatus_ThreadJoined,
        delayHandlerStatus_Unknown
    };
enum delayHandlerThreadActionEnum
    {
        delayHandlerAction_Sleep=0,
        delayHandlerAction_Create,
        delayHandlerAction_Init,
        delayHandlerAction_TestDelays,
        delayHandlerAction_TestParameters,
        delayHandlerAction_GetDelays,
        delayHandlerAction_GetParameters,
        delayHandlerAction_Stop,
        delayHandlerAction_Panic,
        delayHandlerAction_Unknown
    };







class DelayHandlerBase {

public:
    DelayHandlerBase(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_) throw();
    virtual ~DelayHandlerBase();

    int_fast32_t test_delay_service(const int_fast32_t verbose_) throw();
    int_fast32_t test_parameter_service(const int_fast32_t verbose_) throw();
    int_fast32_t process_delay_service(const DifxInput* const D_, const DifxJob* const job_, const DifxScan* const scan_, const struct SERVER_MODEL_DELAY_ARGUMENT* const argument_, struct SERVER_MODEL_DELAY_RESPONSE* const response_, const int_fast32_t verbose_) throw();
    int_fast32_t process_parameter_service(const DifxInput* const D_, const DifxJob* const job_, const DifxScan* const scan_, const struct SERVER_MODEL_PARAMETERS_ARGUMENT* const argument_, struct SERVER_MODEL_PARAMETERS_RESPONSE* const response_, const int_fast32_t verbose_) throw();

    enum delayHandlerThreadStatusEnum check_status() const throw() {return overall_status;}
	uint64_t get_detailed_version_number() const throw() {return detailed_version_number;}
    // start_thread() is not allowed to say throw() because g++ throws
    // an exception within pthread_exit() to clean up the thread
    // memory stack...
    void* start_thread();


protected:
    uint_fast64_t commands_processed;
    int_least64_t parent_pid;
    const int_fast32_t verbosity;

    // dinfx_input data
    const DifxInput* D;
    const DifxJob* job;
    const DifxScan* scan;
    const char* data_input;
    char* data_output;
    int_fast32_t verbose;

    // Here are the variables to control the threads
    pthread_mutex_t thread_mutex;
    pthread_mutex_t fprintf_mutex;
    pthread_cond_t work_condition;
    pthread_cond_t sleeping_condition;

    int_fast32_t NUM_THREADS;
    int_fast32_t num_sleeping;
    int_fast32_t num_running;
    enum delayHandlerThreadStatusEnum* thread_status;
    uint_fast32_t* stationStart;
    uint_fast32_t* stationEnd;
    pthread_t* thread;
    enum delayHandlerThreadActionEnum action;
    bool thread_failure;

    // General information
    enum delayHandlerThreadStatusEnum overall_status;

    // Child process information, one child per thread
    pid_t* child_process;
    FILE** child_stdin;
    FILE** child_stdout;

    // Where should temporary files go?
    static const char* TMP_DIRECTORY;

	// Information about server
	uint64_t detailed_version_number;

    virtual int_fast32_t do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw() = 0;

// fprintf helper
// Alignment attributes
#if (defined __GNUC__) && (__GNUC__ >= 3)
#  define JMA_FUNCTION_IS_FORMAT(f_type,f_pos,arg_start)  __attribute__ ((format (f_type,f_pos,arg_start))) 
#else
#  define JMA_FUNCTION_IS_FORMAT(f_type,f_pos,arg_start)
#endif
    void DHBfprintf(FILE* fp, const char* const format, ...) throw() JMA_FUNCTION_IS_FORMAT(printf,3,4);
    void DHBperror(int errnum, const char* const text) throw();
// functions to help with debugging mutexes
    inline void lock_mutex(pthread_mutex_t* mutex_p,
                           const char* const FILE,
                           const int LINE) throw() {
        int retval = pthread_mutex_lock(mutex_p);
        if((retval)) {
            DHBperror(errno, "locking mutex failed");
			DHBfprintf(stderr, "ERROR: %s:%d: locking mutex failed with retval=%d\n", FILE, LINE, retval);
        }
        return;
    }
    inline void unlock_mutex(pthread_mutex_t* mutex_p,
                             const char* const FILE,
                             const int LINE) throw() {
        int retval = pthread_mutex_unlock(mutex_p);
        if((retval)) {
            DHBperror(errno, "unlocking mutex failed");
            DHBfprintf(stderr, "ERROR: %s:%d: unlocking mutex failed with retval=%d\n", FILE, LINE, retval);
        }
        return;
    }
    inline void broadcast_condition(pthread_cond_t* condition_p,
                                    const char* const FILE,
                                    const int LINE) throw() {
        int retval = pthread_cond_broadcast(condition_p);
        if((retval)) {
            DHBperror(errno, "broadcast_condition failed");
            DHBfprintf(stderr, "ERROR: %s:%d: broadcast_condition failed with %d\n", FILE, LINE, retval);
        }
        return;
    }
    inline void signal_condition(pthread_cond_t* condition_p,
                                 const char* const FILE,
                                 const int LINE) throw() {
        int retval = pthread_cond_signal(condition_p);
        if((retval)) {
            DHBperror(errno, "broadcast_signal failed");
            DHBfprintf(stderr, "ERROR: %s:%d: broadcast_signal failed with %d\n", FILE, LINE, retval);
        }
        return;
    }
    inline void condition_wait(pthread_cond_t* condition_p,
                               pthread_mutex_t* mutex_p,
                               const char* const FILE,
                               const int LINE) throw() {
        int retval = pthread_cond_wait(condition_p, mutex_p);
        if((retval)) {
            DHBperror(errno, "condition_wait failed");
            DHBfprintf(stderr, "ERROR: %s:%d: condition_wait failed with %d\n", FILE, LINE, retval);
        }
        return;
    }
    inline void yield(const char* const FILE,
                      const int LINE) throw() {
        int retval = sched_yield();
        if((retval)) {
            DHBperror(errno, "sched_yield failed");
            DHBfprintf(stderr, "ERROR: %s:%d: sched_yield failed with %d\n", FILE, LINE, retval);
        }
        return;
    }

    virtual int_fast32_t init_NUM_THREADS() throw();
    int_fast32_t init_threads() throw();
    int_fast32_t stop_threads() throw();
    int_fast32_t start_child_process(const char *command, const char* const* argv, const char* const log_base_name, const int_fast32_t tid) throw();
    void panic_stop_child(const int_fast32_t tid) throw();
private:
    void checkForTmpDirectory() throw();
    void handle_SIGPIPE() throw();
    int_fast32_t init_mutexes() throw();
    void delete_mutexes() throw();
    int_fast32_t allocate_storage() throw();
    void delete_storage() throw();
    int_fast32_t create_threads() throw();
    void panic_stop_children() throw();

    // run_thread() is not allowed to say throw() because g++ throws
    // an exception within pthread_exit() to clean up the thread
    // memory stack...
    void* run_thread(const int_fast32_t tid);
    int_fast32_t go_to_work(enum delayHandlerThreadActionEnum action_) throw();
    int_fast32_t wait_for_threads_sleeping() throw();
    int_fast32_t check_status_before_working() throw();

    
    void assign_antennas(const uint32_t NUM_ANT) throw();
    
    int open_child_stdout_logfile(const char* const name, const int_fast32_t tid) throw();
    int open_child_stderr_logfile(const char* const name, const int_fast32_t tid) throw();
    pid_t pipe_writer_popen(const char *command, const char* const* argv, int child_stderr_fd, int *writefd, int *readfd) throw();


    // Debug printing
    void print_SERVER_MODEL_DELAY_ARGUMENT() throw();
    void print_SERVER_MODEL_DELAY_RESPONSE() throw();
    void print_SERVER_MODEL_PARAMETERS_ARGUMENT() throw();
    void print_SERVER_MODEL_PARAMETERS_RESPONSE() throw();

    // prevent copying
    DelayHandlerBase(const DelayHandlerBase& a);
    DelayHandlerBase& operator=(const DelayHandlerBase& a);
};


// CLASS FUNCTIONS



// HELPER FUNCTIONS




}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX

#endif // DelayHandlerBase_h
