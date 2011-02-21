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
struct IIRFilter::P {
   public:
    int N;
    Ipp32fc* accu;
};

/**
 * Prepare private data
 */
IIRFilter::IIRFilter() {
    pD = new IIRFilter::P();
    pD->N = -1;
    pD->accu = 0;
}

/**
 * Release private data
 */
IIRFilter::~IIRFilter() {
    if (pD->accu != 0) {
        ippsFree (pD->accu);
        pD->accu = 0;
    }
    delete pD;
    pD = 0;
}

/**
 * Reset filter state
 */
void IIRFilter::clear() {
    if (pD->N > 0) {
        ippsZero_32fc(pD->accu, pD->N);
    }
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg order Filter order
 * @arg N Number of channels i.e. parallel filters
 */
void IIRFilter::init(size_t order, size_t N) {
    // no coefficients etc to compute
    if ((N==pD->N) && (pD->accu!=0)) {
        clear();
    } else {
        ippsFree(pD->accu);
        pD->N = N;
        pD->accu = ippsMalloc_32fc(pD->N);
    }
}

/**
 * Set filter gain coefficient.
 */
void IIRFilter::set_prescaling(double g) {
    return;
}

/**
 * Get filter gain coefficient.
 */
double IIRFilter::get_prescaling() {
    return 1.0f;
}

/**
 * Set filter coefficient at given index to new value.
 */
void IIRFilter::set_coeff(int index, double c) {
   return;
}

/**
 * Return filter coefficient at index.
 */
double IIRFilter::get_coeff(int index) {
    return (index < 2) ? 1.0f : 0.0f;
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
size_t IIRFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->accu!=0));
    ippsAdd_32fc_I(freqbins, pD->accu, pD->N);
    return pD->N;
}

int IIRFilter::get_num_coeffs() { return 6; } // TODO

/**
 * Return current states
 */
Ipp32fc* IIRFilter::y() {
    return pD->accu;
}

void IIRFilter::setUserOutbuffer(Ipp32fc* userY) {
}

