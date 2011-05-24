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

#include <cmath>
#include <cassert>
#include <iostream>
using std::cerr;
using std::endl;
using std::flush;

#include <memory.h>
#include <malloc.h>

#define MAX_WINDOW_LEN 10000 // 10e4*8Byte*#channels : 10e4*8*512 / 2^20 = 390 MB

#ifndef DEBUG_V
#define DEBUG_V 0
#endif

////////////////////////////////////////////////
// Digital state variable filter
///////////////////////////////////////////////

/**
 * Private data class
 */
struct MAvgFilter::P {
   public:
    P();
    ~P();
    void zero();
    void alloc(size_t channels, size_t windowsize);
    void dealloc();
   private:
    MAvgFilter::P& operator= (const MAvgFilter::P& o);
    P(const MAvgFilter::P& o);
   public:
    int numchannels, N;
    int historysize;
    int current_index;
    Ipp32f    prescaling;
    Ipp32f    scaling;
    // data is complex, but can treat as pairs of reals
    Ipp32f*   histories;
    Ipp32f*   outputs;
    bool user_output;
};

/** Private data c'stor */
MAvgFilter::P::P() {
   N = numchannels = current_index = 0;
   histories = 0;
   outputs = 0;
   prescaling = 1.0f;
   scaling = 1.0f;
   user_output = false;
}

/** Private data d'stor */
MAvgFilter::P::~P() {
   this->dealloc();
}

/** Deallocate internal state */
void MAvgFilter::P::dealloc() {
    if (histories != 0) {
        ippsFree(histories);
        if (!user_output) { ippsFree(outputs); }
        histories = 0;
    }
}

/** Allocate memory for filter state */
void MAvgFilter::P::alloc(size_t channels, size_t windowsize) {
    if (channels < 1) { return; }
    if (histories != 0) { this->dealloc(); }
    numchannels = int(channels);
    N = int(2*channels);
    if (windowsize > MAX_WINDOW_LEN || windowsize < 1) {
        std::cerr << "Moving Average filter: specified window size " << windowsize 
                  << " will be truncated to hardcoded maximum size " << MAX_WINDOW_LEN 
                  << std::endl;
        windowsize = MAX_WINDOW_LEN;
    }
    historysize = int(windowsize);
    histories = ippsMalloc_32f(N*historysize);
    outputs = ippsMalloc_32f(N);
    scaling = double(prescaling) / double(historysize);
    zero();
}

/** Clear internal filter state */
void MAvgFilter::P::zero() {
    ippsZero_32f(histories, N*historysize);
    ippsZero_32f(outputs, N);
}


/**
 * Prepare private data
 */
MAvgFilter::MAvgFilter() {
    pD = new MAvgFilter::P();
}

/**
 * Release private data
 */
MAvgFilter::~MAvgFilter() {
    delete pD;
    pD = 0;
}

/**
 * Reset filter state
 */
void MAvgFilter::clear() {
    pD->zero();
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg order Filter "order" is the size of the moving window
 * @arg N Number of channels i.e. parallel filters
 */
void MAvgFilter::init(size_t order, size_t N) {
    if (pD!=0) { delete pD; }
    pD = new MAvgFilter::P();
    pD->alloc(N, order);
}

/**
 * Set filter gain coefficient.
 */
void MAvgFilter::set_prescaling(double g) {
    pD->prescaling = float(g);
    pD->scaling = double(pD->prescaling) / double(pD->historysize);
}

/**
 * Get filter gain coefficient.
 */
double MAvgFilter::get_prescaling() {
    return pD->prescaling;
}

/**
 * Set filter coefficient at given index to new value.
 */
void MAvgFilter::set_coeff(int index, double c) {
    if (index == 0) {
        Ipp32f oldpresc = pD->prescaling;
        this->init(size_t(c), pD->numchannels); // re-allocates D
        pD->prescaling = oldpresc;
        pD->scaling = double(oldpresc) / double(pD->historysize);
    }
    return;
}

/**
 * Return filter coefficient at index.
 */
double MAvgFilter::get_coeff(int index) {
    if (index == 0) return pD->historysize;
    return 0.0f;
}

/**
 * Create and apply filter coefficients to get the
 * specified corner frequency (wc=2pi*fcut/fsampling).
 */
void MAvgFilter::generate_coeffs(double wcutoff) {
    if (pD == NULL) return;
    double fcn = wcutoff / (2.0*M_PI);
    int len = 2 * int( std::ceil(1.0 / fcn) );
    this->set_coeff(0, len);
}

/**
 * Return number of coefficients used by the filter.
 */
int MAvgFilter::get_num_coeffs() { 
    return 1;
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
size_t MAvgFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->numchannels>0));

    // Treat data as reals
    Ipp32f* input = (Ipp32f*)freqbins;
    if (DEBUG_V) { cerr << flush << "filter() in[0]=" << freqbins[0].re << endl; }

    // Moving sum: unity coefficients for history and sample     : pD->scaling = prescaler/windowsize
    // Moving average: 1/len coefficients for history and sample : pD->scaling = prescaler

    // Method:
    //   1) output <= output - oldest
    //   2) newest = overwrite oldest <= input*scaling
    //   3) output = output + newest
    //   4) oldest_ptr++
    //   history array format is { 0 ... , newest, oldest, ... N-1 }

    Ipp32f* entry = pD->histories + (pD->current_index*pD->N);
    ippsSub_32f_I(entry, pD->outputs, pD->N);
    ippsMulC_32f (input, pD->scaling, entry, pD->N);
    ippsAdd_32f_I(entry, pD->outputs, pD->N);
    pD->current_index = (pD->current_index + 1) % pD->historysize;

    return pD->numchannels;
}

/**
 * Return current states
 */
Ipp32fc* MAvgFilter::y() {
    return (Ipp32fc*)(pD->outputs);
}

/**
 * Allow user to specify own result buffer.
 */
void MAvgFilter::setUserOutbuffer(Ipp32fc* userY) {
    if (!pD->user_output) { ippsFree(pD->outputs); }
    pD->user_output = true;
    pD->outputs = (Ipp32f*)userY;
}
