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


#ifndef DelayHandlerDiFXDelayServer_h
#define DelayHandlerDiFXDelayServer_h

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
#include <pthread.h>
#include <rpc/rpc.h>
#include "DiFX_Delay_Server.h"
#include "DelayHandlerDistributorInterface.h"
#include "DelayHandlerBase.h"


namespace DiFX {
namespace Delay {
namespace Handler {

class DelayHandlerDiFXDelayServer : public DelayHandlerBase{

public:
	DelayHandlerDiFXDelayServer(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_, const char* const delayServerHost, const uint64_t delayServer_) throw();
    virtual ~DelayHandlerDiFXDelayServer();




protected:
	CLIENT *clnt;
	uint64_t delayServer;
	struct getDIFX_DELAY_SERVER_1_arg local_arg;
	struct getDIFX_DELAY_SERVER_1_res local_res;
	struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg local_parg;
	struct getDIFX_DELAY_SERVER_PARAMETERS_1_res local_pres;
	uint32_t local_Num_Stations;
	uint32_t local_Num_Sources;
	uint32_t local_Num_EOPs;
	DIFX_DELAY_SERVER_1_station* local_station;
	DIFX_DELAY_SERVER_1_source* local_source;
	DIFX_DELAY_SERVER_1_EOP* local_EOP;

    int_fast32_t do_work(const int_fast32_t tid, const enum delayHandlerThreadActionEnum action_copy) throw();

    virtual int_fast32_t init_NUM_THREADS() throw();
private:
	static const unsigned long delayHandler=DIFX_DELAY_SERVER_PROG;
	static const unsigned long delayVersion=DIFX_DELAY_SERVER_VERS_1;
	int_fast32_t do_thread_init(const int_fast32_t tid) throw();
	int_fast32_t do_thread_test_delays(const int_fast32_t tid) throw();
	int_fast32_t do_thread_test_parameters(const int_fast32_t tid) throw();
	int_fast32_t do_thread_get_delays(const int_fast32_t tid) throw();
	int_fast32_t do_thread_get_parameters(const int_fast32_t tid) throw();
	int_fast32_t do_thread_stop(const int_fast32_t tid) throw();
	int_fast32_t do_thread_panic(const int_fast32_t tid) throw();




	// Helper functions for constructors and destructor
	int_fast32_t create_RPC_client(const char* const delayServerHost) throw();
	int_fast32_t destroy_RPC_client() throw();
	void dealloc_memory() throw();


	// print stuff
	void print_local_SERVER_MODEL_DELAY_ARGUMENT() throw();
	void print_local_SERVER_MODEL_DELAY_RESPONSE() throw();
	void print_local_SERVER_MODEL_PARAMETERS_ARGUMENT() throw();
	void print_local_SERVER_MODEL_PARAMETERS_RESPONSE() throw();

	// test stuff
	int_fast32_t test_local_SERVER_MODEL_DELAY_ARGUMENT(const int_fast32_t t) throw();
	int_fast32_t test_local_SERVER_MODEL_DELAY_RESPONSE(const int_fast32_t t) throw();
	int_fast32_t test_local_SERVER_MODEL_PARAMETERS_ARGUMENT(const int_fast32_t t) throw();
	int_fast32_t test_local_SERVER_MODEL_PARAMETERS_RESPONSE(const int_fast32_t t) throw();

	// convert stuff
	int_fast32_t convert_to_local_SERVER_MODEL_DELAY_ARGUMENT() throw();
	int_fast32_t convert_from_local_SERVER_MODEL_DELAY_RESPONSE() throw();
	int_fast32_t convert_to_local_SERVER_MODEL_PARAMETERS_ARGUMENT() throw();
	int_fast32_t convert_from_local_SERVER_MODEL_PARAMETERS_RESPONSE() throw();

	// RPC stuff
	int_fast32_t callCalcDelay() throw();
	int_fast32_t callCalcParameters() throw();
	int_fast32_t freeCalcDelayResults() throw();
	int_fast32_t freeCalcParametersResults() throw();
	
	
    // prevent copying
    DelayHandlerDiFXDelayServer(const DelayHandlerDiFXDelayServer& a);
    DelayHandlerDiFXDelayServer& operator=(const DelayHandlerDiFXDelayServer& a);
};


// CLASS FUNCTIONS



// HELPER FUNCTIONS




}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX

#endif // DelayHandlerDiFXDelayServer_h
