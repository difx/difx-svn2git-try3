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


#ifndef DelayHandlerTest_h
#define DelayHandlerTest_h

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
#include "DelayHandlerDistributorInterface.h"
#include "DelayHandlerBase.h"


namespace DiFX {
namespace Delay {
namespace Handler {

class DelayHandlerTest : public DelayHandlerBase{

public:
    DelayHandlerTest(const int_fast32_t NUM_THREADS_, int_fast32_t verbosity_) throw();
    virtual ~DelayHandlerTest();




protected:

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
	
    // prevent copying
    DelayHandlerTest(const DelayHandlerTest& a);
    DelayHandlerTest& operator=(const DelayHandlerTest& a);
};


// CLASS FUNCTIONS



// HELPER FUNCTIONS




}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX

#endif // DelayHandlerTest_h
