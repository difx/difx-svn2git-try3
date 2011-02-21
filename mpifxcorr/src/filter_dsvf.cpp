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

#define COEFFS_PER_STAGE 6
#ifndef DEBUG_V
#define DEBUG_V 0
#endif

////////////////////////////////////////////////
// Digital state variable filter
///////////////////////////////////////////////

/**
 * Private data class
 */
struct DSVFFilter::P {
   public:
    P();
    ~P();
    void zero();
    void alloc(int channels);
    void dealloc();
   private:
    DSVFFilter::P& operator= (const DSVFFilter::P& o);
    P(const DSVFFilter::P& o);
   public:
    int numchannels, N;
    Ipp32f    prescaling;
    Ipp32f    f; // 2*sin(pi*fcutoff/fsampling)
    Ipp32f    q; // inverse of quality factor 1/Q
    // data is comples, but due to real-valued filter coeffs we can use 2*Ipp32f
    Ipp32f*   z1;
    Ipp32f*   z2;
    Ipp32f*   sum1;
    Ipp32f*   sum2;
    Ipp32f*   sum3;
    Ipp32f*   outputs;
    bool user_output;
};

/** Private data c'stor */
DSVFFilter::P::P() {
   N = numchannels = 0;
   f = 1e-3;
   q = 1/2.0;
   z1 = z2 = 0;
   sum1 = sum2 = sum3 = outputs = 0;
   prescaling = 1.0f;
   user_output = false;
}

/** Private data d'stor */
DSVFFilter::P::~P() {
   this->dealloc();
}

/** Deallocate internal state */
void DSVFFilter::P::dealloc() {
    if (z1 != 0) {
        ippsFree(z1);
        ippsFree(z2);
        ippsFree(sum1);
        ippsFree(sum2);
        ippsFree(sum3);
        if (!user_output) { ippsFree(outputs); }
        z1 = 0;
    }
}

/** Allocate memory for filter state */
void DSVFFilter::P::alloc(int channels) {
    if (channels < 1) { return; }
    if (z1 != 0) { this->dealloc(); }
    numchannels = channels;
    N = 2*channels;
    z1 = ippsMalloc_32f(N);
    z2 = ippsMalloc_32f(N);
    sum1 = ippsMalloc_32f(N);
    sum2 = ippsMalloc_32f(N);
    sum3 = ippsMalloc_32f(N);
    outputs = ippsMalloc_32f(N);
    zero();
}

/** Clear internal filter state */
void DSVFFilter::P::zero() {
    ippsZero_32f(z1, N);
    ippsZero_32f(z2, N);
    ippsZero_32f(sum1, N);
    ippsZero_32f(sum2, N);
    ippsZero_32f(sum3, N);
    ippsZero_32f(outputs, N);
}


/**
 * Prepare private data
 */
DSVFFilter::DSVFFilter() {
    pD = new DSVFFilter::P();
}

/**
 * Release private data
 */
DSVFFilter::~DSVFFilter() {
    delete pD;
    pD = 0;
}

/**
 * Reset filter state
 */
void DSVFFilter::clear() {
    pD->zero();
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg order Filter order, ignored, always 2nd order
 * @arg N Number of channels i.e. parallel filters
 */
void DSVFFilter::init(size_t order, size_t N) {
    if (pD!=0) { delete pD; }
    pD = new DSVFFilter::P();
    pD->alloc(N);
}

/**
 * Set filter gain coefficient.
 */
void DSVFFilter::set_prescaling(double g) {
    pD->prescaling = float(g);
}

/**
 * Get filter gain coefficient.
 */
double DSVFFilter::get_prescaling() {
    return pD->prescaling;
}

/**
 * Set filter coefficient at given index to new value.
 */
void DSVFFilter::set_coeff(int index, double c) {
    if (index == 0) pD->f = float(c);
    if (index == 1) pD->q = float(c);
    return;
}

/**
 * Return filter coefficient at index.
 */
double DSVFFilter::get_coeff(int index) {
    if (index == 0) return pD->f;
    if (index == 1) return pD->q;
    return 0.0f;
}

/**
 * Return number of coefficients used by the filter.
 */
int DSVFFilter::get_num_coeffs() { 
    return 2;
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
size_t DSVFFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->numchannels>0));

    /* --Matlab--
     * for ii=1:N,
     *   % move new data in
     *   input = single(gain * unfiltered(ii));
     *   sum1 = input + (-fb1 * q) + (-output);
     *     % sum1 = (-output) + (-fb1 * q) + (gain*unfiltered(ii));
     *   sum2 = f*sum1 + fb1;
     *   sum3 = f*fb1 + fb2;
     *   output = sum3;
     *   fb1 = sum2;
     *   fb2 = sum3;
     *   filtered(:, ii) = output;
     * end
     */

    // Filter coefficients are real
    // For performance we can thus use <real,real> operations.
    Ipp32f* input = (Ipp32f*)freqbins;
    if (DEBUG_V) { cerr << flush << "filter() in[0]=" << freqbins[0].re << endl; }

    // rearranging:
    //   -sum1 = output + fb1*q - gain*input
    //   sum2 = -f*(-sum1) + fb1
    //   sum3 = output = f*fb1 + fb2
    //   fb1 = copy2
    //   fb2 = copy output

    // -sum1 = output + z1*q + (-gain)*input
    ippsCopy_32f(pD->outputs, /*dst*/pD->sum1, pD->N);
    ippsAddProductC_32f(/*src*/pD->z1, /*const*/pD->q, /*srcdst*/pD->sum1, pD->N);
    if (pD->prescaling != float(1.0f)) {
       ippsAddProductC_32f(/*src*/input, /*const*/-(pD->prescaling), /*srcdst*/pD->sum1, pD->N);
    } else {
       ippsSub_32f_I(/*src*/input, /*srcdst*/pD->sum1, pD->N);
    }

    // sum2 = -f*(-sum1) + fb1
    ippsCopy_32f(pD->z1, /*dst*/pD->sum2, pD->N);
    ippsAddProductC_32f(/*src*/pD->sum1, /*const*/-(pD->f), /*srcdst*/ pD->sum2, pD->N);

    // output = f*fb1 + fb2
    ippsCopy_32f(pD->z2, /*dst*/pD->outputs, pD->N);
    ippsAddProductC_32f(/*src*/pD->z1, /*const*/pD->f, /*srcdst*/pD->outputs, pD->N);

    // fb1 = sum2
    // fb2 = output
    ippsCopy_32f(pD->sum2, /*dst*/pD->z1, pD->N);
    ippsCopy_32f(pD->outputs, /*dst*/pD->z2, pD->N);

    return pD->numchannels;
}

/**
 * Return current states
 */
Ipp32fc* DSVFFilter::y() {
    return (Ipp32fc*)(pD->outputs);
}

/**
 * Allow user to specify own result buffer.
 */
void DSVFFilter::setUserOutbuffer(Ipp32fc* userY) {
    if (!pD->user_output) { ippsFree(pD->outputs); }
    pD->user_output = true;
    pD->outputs = (Ipp32f*)userY;
}
 
