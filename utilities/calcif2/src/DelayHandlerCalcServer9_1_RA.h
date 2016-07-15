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


#ifndef DelayHandlerCalcServer9_1_RA_h
#define DelayHandlerCalcServer9_1_RA_h

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
#include <pthread.h>
#include <rpc/rpc.h>
#include "DelayHandlerBase.h"
#include "DelayHandlerDistributorInterface.h"
#include "DelayHandlerCalcServer9_1_RAInterface.h"


namespace DiFX {
namespace Delay {
namespace Handler {

class DelayHandlerCalcServer9_1_RA : public DelayHandlerBase{

public:
	DelayHandlerCalcServer9_1_RA(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_) throw();
    virtual ~DelayHandlerCalcServer9_1_RA();




protected:
	struct CALCSERVER9_1_RA_MODEL_DELAY_ARGUMENT** local_arg;
	struct CALCSERVER9_1_RA_MODEL_DELAY_RESPONSE** local_res;
	struct CALCSERVER9_1_RA_MODEL_PARAMETERS_ARGUMENT local_parg;
	struct CALCSERVER9_1_RA_MODEL_PARAMETERS_RESPONSE local_pres;
	uint_fast32_t* local_Num_Stations_Sources;
	uint_fast32_t* local_current_Num;

    int_fast32_t do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw();

private:
	static const char* server_command_name;
	int_fast32_t do_thread_init(const int_fast32_t tid) throw();
	int_fast32_t do_thread_test_delays(const int_fast32_t tid) throw();
	int_fast32_t do_thread_test_parameters(const int_fast32_t tid) throw();
	int_fast32_t do_thread_get_delays(const int_fast32_t tid) throw();
	int_fast32_t do_thread_get_parameters(const int_fast32_t tid) throw();
	int_fast32_t do_thread_stop(const int_fast32_t tid) throw();
	int_fast32_t do_thread_panic(const int_fast32_t tid) throw();




	// Helper functions for constructors and destructor
	int_fast32_t allocate_memory() throw();
	void dealloc_memory() throw();


	// print stuff
	void print_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t tid) throw();
	void print_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t tid) throw();
	void print_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t tid) throw();
	void print_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t tid) throw();

	// test stuff
	int_fast32_t test_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t tid) throw();
	int_fast32_t test_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t tid) throw();
	int_fast32_t test_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t tid) throw();
	int_fast32_t test_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t tid) throw();

	// convert stuff
	int_fast32_t convert_to_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t tid) throw();
	int_fast32_t convert_from_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t tid) throw();
	int_fast32_t convert_from_local_SERVER_MODEL_DELAY_RESPONSE_0(const int_fast32_t tid, const uint_fast32_t so) throw();
	int_fast32_t convert_from_local_SERVER_MODEL_DELAY_RESPONSE_N(const int_fast32_t tid, const uint_fast32_t sr, const uint_fast32_t sl) throw();
	int_fast32_t convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t tid) throw();
	int_fast32_t convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t tid) throw();

	// communicate through pipe stuff
	int_fast32_t callServerForInit(const int_fast32_t tid) throw();
	int_fast32_t callServerForDelay(const int_fast32_t tid) throw();
	int_fast32_t callServerForParameters(const int_fast32_t tid) throw();
	int_fast32_t callServerForStop(const int_fast32_t tid) throw();
	int_fast32_t callServerForPanic(const int_fast32_t tid) throw();
	int_fast32_t freeServerDelayPointers(const int_fast32_t tid) throw();
	int_fast32_t freeServerParametersPointers(const int_fast32_t tid) throw();
	
	
    // prevent copying
    DelayHandlerCalcServer9_1_RA(const DelayHandlerCalcServer9_1_RA& a);
    DelayHandlerCalcServer9_1_RA& operator=(const DelayHandlerCalcServer9_1_RA& a);
};


// CLASS FUNCTIONS



// HELPER FUNCTIONS




}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX

#endif // DelayHandlerCalcServer9_1_RA_h
