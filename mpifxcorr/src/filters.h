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
#ifndef _FILTERS_H
#define _FILTERS_H

#include <cstring>
#include <ipps.h>
#include <ippm.h>
#include <ippvm.h>
#include <ippcore.h>

#include "filter.h"

/**
 * Standard averaging filter
 */
class IntFilter : public Filter {
    // C'stors
    public:
        IntFilter();
        ~IntFilter();
    // No copy c'stors
    private:
        IntFilter& operator= (const IntFilter& o);
        IntFilter(const IntFilter& o);
    public:
        const char* name() { return "Averaging"; }
        void clear();
        void init(size_t, size_t);
        void   set_prescaling(double) { return; }
        double get_prescaling() { return 1.0f; }
        void   set_coeff(int, double);
        double get_coeff(int);
        int get_num_coeffs();
   public:
        void filter(Ipp32fc*);
        Ipp32fc* y();
    // Data
    private:
        struct P;
        P* pD;
};

/**
 * IIR filter, hardcoded order
 */
class IIRFilter : public Filter {
    // C'stors
    public:
        IIRFilter();
        ~IIRFilter();
    // No copy c'stors
    private:
        IIRFilter& operator= (const IIRFilter& o);
        IIRFilter(const IIRFilter& o);
    public:
        const char* name() { return "IIR"; }
        void clear();
        void init(size_t, size_t);
        void   set_prescaling(double);
        double get_prescaling();
        void   set_coeff(int, double);
        double get_coeff(int);
        int get_num_coeffs();
   public:
        void filter(Ipp32fc*);
        Ipp32fc* y();
    // Data
    private:
        struct P;
        P* pD;
};

/**
 * IIR SOS filter
 */
class IIRSOSFilter : public Filter {
    // C'stors
    public:
        IIRSOSFilter();
        ~IIRSOSFilter();
    // No copy c'stors
    private:
        IIRSOSFilter& operator= (const IIRSOSFilter& o);
        IIRSOSFilter(const IIRSOSFilter& o);
    public:
        const char* name() { return "BiQuadIIR"; }
        void clear();
        void init(size_t, size_t);
        void   set_prescaling(double);
        double get_prescaling();
        void   set_coeff(int, double);
        double get_coeff(int);
        int get_num_coeffs();
   public:
        void filter(Ipp32fc*);
        Ipp32fc* y();
    // Data
    private:
        struct P;
        P* pD;
};

#endif // _FILTERS_H
