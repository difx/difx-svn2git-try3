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

#include "filters.h"

#include <cassert>
#include <iostream>
using std::cerr;
using std::endl;
using std::flush;

#include <memory.h>
#include <malloc.h>

////////////////////////////////////////////////
// DEFAULT AVERAGING/INTEGRATING FILTER
///////////////////////////////////////////////

/**
 * Private data class
 */
struct AvgFilter::P {
   public:
    int N;
    Ipp32fc* accu;
};

/**
 * Prepare private data
 */
AvgFilter::AvgFilter() {
    pD = new AvgFilter::P();
    pD->N = -1;
    pD->accu = 0;
}

/**
 * Release private data
 */
AvgFilter::~AvgFilter() {
    if (pD->accu != 0) {
        free (pD->accu);
    }
    delete pD;
}

/**
 * Reset filter state
 */
void AvgFilter::clear() {
    if (pD->N > 0) {
        ippsZero_32fc(pD->accu, pD->N);
    }
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg w_cutoff Normalized cutoff frequency (ignored!)
 * @arg N Number of channels i.e. parallel filters
 */
void AvgFilter::init(double w_cutoff, size_t N) {
    // no coefficients etc to compute
    if ((N==pD->N) && (pD->accu!=0)) {
        clear();
    } else {
        free(pD->accu);
        pD->N = N;
        pD->accu = ippsMalloc_32fc(pD->N);
    }
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
void AvgFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->accu!=0));
    ippsAdd_32fc_I(freqbins, pD->accu, pD->N);
}

/**
 * Return current states
 */
Ipp32fc* AvgFilter::y() {
    return pD->accu;
}

