/********************************************************************
 * Ultra-narrow filters for VLBI software correlation
 * (C) 2010 Jan Wagner MPIfR
 ********************************************************************
 *
 * Filter input is always an array of single samples from multiple
 * channels. With N channel data, you can think of it as a group
 * of N parallel filters. Filtering output is the output of these N
 * parallel filters.
 *
 ********************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *********************************************************************/
#ifndef _FILTER_H
#define _FILTER_H

#include <cstring>
#include <ipps.h>
#include <ippm.h>
#include <ippvm.h>
#include <ippcore.h>

/**
 * Filter types
 */

enum FltType { FLT_AVERAGING=0, FLT_CIC=1, FLT_IIR_SOS=2, FLT_FIR=3 };

/**
 * Base class for Filter
 */
class Filter {
    // C'stor
    public:
        static Filter* getFilter(FltType);
        Filter() {}
        ~Filter() {}
    // No copy c'stors
    private:
        Filter& operator= (const Filter& o);
        Filter(const Filter& o);
    public:
        virtual void clear() = 0;
        virtual void init(size_t, size_t) = 0;
        virtual void   set_prescaling(double) = 0;
        virtual double get_prescaling() = 0;
        virtual void   set_coeff(int, double) = 0;
        virtual double get_coeff(int) = 0;
        virtual int    get_num_coeffs() = 0;
        virtual const char* name() = 0;
    public:
        virtual void filter(Ipp32fc*) = 0;
        virtual Ipp32fc* y() = 0;
        virtual void setUserOutbuffer(Ipp32fc*) = 0;
};

#endif // _FILTER_H
