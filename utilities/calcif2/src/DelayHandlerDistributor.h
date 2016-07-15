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


#ifndef DelayHandlerDistributor_h
#define DelayHandlerDistributor_h

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

#include <difxio.h>
#include "DelayHandlerBase.h"
#include "DelayHandlerDistributorInterface.h"


namespace DiFX {
namespace Delay {
namespace Handler {








class DelayHandlerDistributor {

public:
	DelayHandlerDistributor(const uint_fast32_t NUM_THREADS_, const char* const delayServerHost, const enum DelayServerHandlerType handler_, enum DelayServerType server_, const int_fast32_t verbosity_) throw();

	~DelayHandlerDistributor();

    int_fast32_t test_delay_service(const int_fast32_t verbose_) throw()
	{
		if((delayHandler))
		{
			return delayHandler->test_delay_service(verbose_);
		}
		return -10000;
	}
    int_fast32_t test_parameter_service(const int_fast32_t verbose_) throw()
	{
		if((delayHandler))
		{
			return delayHandler->test_parameter_service(verbose_);
		}
		return -10000;
	}
    int_fast32_t process_delay_service(const DifxInput* const D_, const DifxJob* const job_, const DifxScan* const scan_, const struct SERVER_MODEL_DELAY_ARGUMENT* const argument_, struct SERVER_MODEL_DELAY_RESPONSE* const response_, const int_fast32_t verbose_) throw()
	{
		if((delayHandler))
		{
			return delayHandler->process_delay_service(D_, job_, scan_, argument_, response_, verbose_);
		}
		return -10000;
	}
    int_fast32_t process_parameter_service(const DifxInput* const D_, const DifxJob* const job_, const DifxScan* const scan_, const struct SERVER_MODEL_PARAMETERS_ARGUMENT* const argument_, struct SERVER_MODEL_PARAMETERS_RESPONSE* const response_, const int_fast32_t verbose_) throw()
	{
		if((delayHandler))
		{
			return delayHandler->process_parameter_service(D_, job_, scan_, argument_, response_, verbose_);
		}
		return -10000;
	}
	uint64_t get_detailed_version_number() const throw() {
		if((delayHandler))
		{
			return delayHandler->get_detailed_version_number();
		}
		return 0;
	}
	
    int_fast32_t check_status() const throw()
	{
		if((delayHandler))
		{
			return delayHandler->check_status();
		}
		return -10000;
	}
    
protected:
	DelayHandlerBase* delayHandler;
private:
	uint_fast32_t get_best_NUM_THREADS(const uint_fast32_t NUM_THREADS_, const int_fast32_t verbosity) const throw();
	

	
    // prevent copying
    DelayHandlerDistributor(const DelayHandlerDistributor& a);
    DelayHandlerDistributor& operator=(const DelayHandlerDistributor& a);
};


// CLASS FUNCTIONS



// HELPER FUNCTIONS




}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX

#endif // DelayHandlerDistributor_h
