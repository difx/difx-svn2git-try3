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
struct IntFilter::P {
   public:
    int N;
    Ipp32fc* accu;
    bool user_output;
};

/**
 * Prepare private data
 */
IntFilter::IntFilter() {
    pD = new IntFilter::P();
    pD->N = -1;
    pD->accu = 0;
    pD->user_output = false;
}

/**
 * Release private data
 */
IntFilter::~IntFilter() {
    if (!pD->user_output) { ippsFree(pD->accu); }
    delete pD;
    pD = 0;
}


/**
 * Reset filter state
 */
void IntFilter::clear() {
    if (pD->N > 0) {
        ippsZero_32fc(pD->accu, pD->N);
    }
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg order Ignored
 * @arg N     Number of channels i.e. parallel filters
 */
void IntFilter::init(size_t order, size_t N) {
    pD->N = N;
    if (!pD->user_output) { 
        ippsFree(pD->accu);
        pD->accu = 0;
        if (N > 0) {
            pD->accu = ippsMalloc_32fc(pD->N);
        }
    }
    if (N > 0) { clear(); }
}

/**
 * Set filter coefficient at given index to new value.
 */
void IntFilter::set_coeff(int, double) {
   return;
}

/**
 * Return filter coefficient at index.
 */
double IntFilter::get_coeff(int index) {
    return (index < 2) ? 1.0f : 0.0f;
}

void IntFilter::generate_coeffs(double wcutoff) {
   // does nothing
}

int IntFilter::get_num_coeffs() { return 1; }

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
size_t IntFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->accu!=0));
    ippsAdd_32fc_I(freqbins, pD->accu, pD->N);
    return pD->N;
}

/**
 * Return current states
 */
Ipp32fc* IntFilter::y() {
    return pD->accu;
}

/**
 * Allow user to specify own result buffer.
 */
void IntFilter::setUserOutbuffer(Ipp32fc* userY) {
    if (!pD->user_output) { ippsFree(pD->accu); }
    pD->user_output = true;
    pD->accu = (Ipp32fc*)userY;
}

