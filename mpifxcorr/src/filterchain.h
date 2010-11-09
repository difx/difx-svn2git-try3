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
#ifndef _FILTERCHAIN_H
#define _FILTERCHAIN_H

#include <ipps.h>
#include <ippm.h>
#include <ippvm.h>
#include <ippcore.h>

#include <cstring>
#include <vector>
#include <iostream>

#include "filter.h" 
//class Filter;

/**
 * FilterChain class
 */
class FilterChain : public Filter {
    public:
        FilterChain();
        ~FilterChain();
    public:
        const char* name() { return "Filterchain"; }
        // initialize filter chain from config file
        void buildFromFile(const char*, int);
        // chain access
        void appendFilter(Filter*, int);
        // funcs from the Filter class
        void clear();
        void init(size_t, size_t);
        void   set_prescaling(double) { };
        double get_prescaling() { return 1.0f; }
        void   set_coeff(int, double);
        double get_coeff(int);
        int get_num_coeffs();
    public:
        void filter(Ipp32fc*);
        Ipp32fc* y();
    public:
        void summary(std::ostream& o);
    private:
        std::vector<Filter*> fchain;
        std::vector<int> rchain;
        std::vector<int*> countchain;
};

#endif // _FILTERCHAIN_H

