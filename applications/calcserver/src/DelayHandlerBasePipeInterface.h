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



#ifndef DelayHandlerBasePipeInterface_h
#define DelayHandlerBasePipeInterface_h

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


#ifdef __cplusplus
namespace DiFX {
namespace Delay {
namespace Handler {
extern "C" {
#endif

#ifndef DU32NUM
#  ifdef DelayHandlerBasePipeInterface_cpp
#    define DU32NUM(x) = UINT32_C(x)
#  else
#    define DU32NUM(x)
#  endif
#endif

extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_START    DU32NUM(0xDE1ABABE);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_END      DU32NUM(0xDE1AD1);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOINIT   DU32NUM(0xD057A127);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DODELAY  DU32NUM(0xD0DE1A);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOPARAM  DU32NUM(0xD0CACA0);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DOSTOP   DU32NUM(0xD1EBABE);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATA     DU32NUM(0xDE1AF00D);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_COMMAND_DATADONE DU32NUM(0xF00DD1);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_STATUS_GOOD      DU32NUM(0xBABE600D);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_STATUS_BAD       DU32NUM(0xBABEBAAD);
extern const uint32_t DIFX_DELAYHANDLERBASEPIPEINTERFACE_STATUS_DONE      DU32NUM(0xBABEDEAD);

#undef DU32NUM


#ifdef __cplusplus
}  // extern "C"
}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX
#endif


// CLASS FUNCTIONS



// HELPER FUNCTIONS



#endif // DelayHandlerBasePipeInterface_h
