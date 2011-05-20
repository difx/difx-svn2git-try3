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

#define SAMPLE_AND_HOLD 0 // 1=allocate memory for output, 0=pass input to output

////////////////////////////////////////////////
// Decimator - does not scale nor filter data
// Takes first sample, bypasses R-1 samples
///////////////////////////////////////////////

/**
 * Private data class
 */
struct DecFilter::P {
   public:
    P();
    ~P();
    void zero();
    void alloc(int channels);
    void dealloc();
   private:
    DecFilter::P& operator= (const DecFilter::P& o);
    P(const DecFilter::P& o);
   public:
    int numchannels;
    int R, Rcount;
    Ipp32fc* outputs;
    bool user_output;
};

/** Private data c'stor */
DecFilter::P::P() {
   numchannels = R = Rcount = 0;
   outputs = 0;
   user_output = false;
}

/** Private data d'stor */
DecFilter::P::~P() {
   this->dealloc();
}

/** Deallocate internal state */
void DecFilter::P::dealloc() {
    if (SAMPLE_AND_HOLD && outputs!=0 && !user_output) {
        ippsFree(outputs);
        outputs = 0;
    }
}

/** Allocate memory for filter state */
void DecFilter::P::alloc(int channels) {
    if (channels < 1) { return; }
    numchannels = channels;
    if (SAMPLE_AND_HOLD) {
        outputs = ippsMalloc_32fc(numchannels);
    }
    zero();
}

/** Clear internal filter state */
void DecFilter::P::zero() {
    if (SAMPLE_AND_HOLD) {
        ippsZero_32fc(outputs, numchannels);
    }
    Rcount = 0;
}


/**
 * Prepare private data
 */
DecFilter::DecFilter() {
    pD = new DecFilter::P();
}

/**
 * Release private data
 */
DecFilter::~DecFilter() {
    delete pD;
    pD = 0;
}

/**
 * Reset filter state
 */
void DecFilter::clear() {
    pD->zero();
}

/**
 * Re-initialize filter. 
 * 
 * @arg order Filter order, ignored
 * @arg N Number of channels i.e. parallel filters
 */
void DecFilter::init(size_t order, size_t N) {
    if (pD!=0) { delete pD; }
    pD = new DecFilter::P();
    pD->alloc(N);
}

/**
 * Set filter gain coefficient.
 */
void DecFilter::set_prescaling(double g) { }

/**
 * Get filter gain coefficient.
 */
double DecFilter::get_prescaling() { return 1.0f; }

/**
 * Set filter coefficient at given index to new value.
 */
void DecFilter::set_coeff(int index, double c) {
    if (index == 0) pD->R = int(c);
    if (index == 1) pD->Rcount = int(c);
    return;
}

/**
 * Return filter coefficient at index.
 */
double DecFilter::get_coeff(int index) {
    if (index == 0) return pD->R;
    if (index == 1) return pD->Rcount;
    return 0.0f;
}

void DecFilter::generate_coeffs(double wcutoff) {
   // does nothing
}

/**
 * Return number of coefficients used by the filter.
 */
int DecFilter::get_num_coeffs() { 
    return 2;
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
size_t DecFilter::filter(Ipp32fc* freqbins) {
    int oldcount = pD->Rcount;
    pD->Rcount = (pD->Rcount + 1) % pD->R;

    if (oldcount == 0) { 
        if (SAMPLE_AND_HOLD) {
            ippsCopy_32fc(pD->outputs, freqbins, pD->numchannels);
        } else {
            pD->outputs = freqbins; // out==in
        }
        return pD->numchannels;
    }
    return 0;
}

/**
 * Return current states
 */
Ipp32fc* DecFilter::y() {
    return (Ipp32fc*)(pD->outputs);
}

/**
 * Allow user to specify own result buffer.
 */
void DecFilter::setUserOutbuffer(Ipp32fc* userY) {
    if (!pD->user_output && SAMPLE_AND_HOLD) { 
       ippsFree(pD->outputs); 
    }
    pD->user_output = true;
    pD->outputs = userY;
}

