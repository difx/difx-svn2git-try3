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
#define DEBUG_V 0

////////////////////////////////////////////////
// IIR Second-order-structures filter
///////////////////////////////////////////////

/**
 * Private data class
 */
struct IIRSOSFilter::P {
   public:
    P();
    ~P();
    void zero();
    void alloc(int stages, int channels);
    void dealloc();
   private:
    IIRSOSFilter::P& operator= (const IIRSOSFilter::P& o);
    P(const IIRSOSFilter::P& o);
   public:
    int numchannels, N;
    int numstages;
    Ipp64f    prescaling;
    Ipp64f*   coeffs;
    Ipp32f    prescaling_f32;
    Ipp32f*   coeffs_f32;
    // vars store Ipp32fc but due to real-val filter coeffs, for performance we can use 2*Ipp32f
    Ipp32f*  z1; // N*stages of z^-1
    Ipp32f*  z2; // N*stages of z^-2
    Ipp32f*  pre_sum;
    Ipp32f*  outputs;
    bool user_output;
};

/** Private data c'stor */
IIRSOSFilter::P::P() {
   N = numchannels = numstages = 0;
   coeffs = 0;
   coeffs_f32 = 0;
   z1 = z2 = 0;
   pre_sum = outputs = 0;
   prescaling = 1.0f;
   prescaling_f32 = 1.0f;
   user_output = false;
}

/** Private data d'stor */
IIRSOSFilter::P::~P() {
   this->dealloc();
}

/** Deallocate internal state */
void IIRSOSFilter::P::dealloc() {
    if (coeffs != 0) {
        ippsFree(coeffs);
        ippsFree(coeffs_f32);
        ippsFree(pre_sum);
        if (!user_output) { ippsFree(outputs); }
        ippsFree(z1); 
        ippsFree(z2); 
    }
}

/** Allocate memory for filter state */
void IIRSOSFilter::P::alloc(int stages, int channels) {
    if (stages < 1 || channels < 1) { return; }
    if (coeffs != 0) { this->dealloc(); }
    numchannels = channels;
    N = 2*channels;
    numstages = stages;
    coeffs = ippsMalloc_64f(6 * numstages);
    coeffs_f32 = ippsMalloc_32f(6 * numstages);
    z1 = ippsMalloc_32f(N*numstages);
    z2 = ippsMalloc_32f(N*numstages);
    pre_sum = ippsMalloc_32f(N);
    outputs = ippsMalloc_32f(N);
    ippsZero_64f(coeffs, 6 * numstages);
    zero();
}

/** Clear internal filter state */
void IIRSOSFilter::P::zero() {
    ippsZero_32f(z1, N*numstages);
    ippsZero_32f(z2, N*numstages);
    ippsZero_32f(pre_sum, N);
    ippsZero_32f(outputs, N);
}


/**
 * Prepare private data
 */
IIRSOSFilter::IIRSOSFilter() {
    pD = new IIRSOSFilter::P();
}

/**
 * Release private data
 */
IIRSOSFilter::~IIRSOSFilter() {
    delete pD;
    pD = 0;
}

/**
 * Reset filter state
 */
void IIRSOSFilter::clear() {
    pD->zero();
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg order Filter order, must be a multiple of 2 for 2nd-order-sections
 * @arg N Number of channels i.e. parallel filters
 */
void IIRSOSFilter::init(size_t order, size_t N) {
    if (pD!=0) { delete pD; }
    pD = new IIRSOSFilter::P();
    pD->alloc(order/2, N); // order/2 = nr of second order sections
}

/**
 * Set filter gain coefficient.
 */
void IIRSOSFilter::set_prescaling(double g) {
    pD->prescaling = g;
    pD->prescaling_f32 = (float)g;
}

/**
 * Get filter gain coefficient.
 */
double IIRSOSFilter::get_prescaling() {
    return pD->prescaling;
}

/**
 * Set filter coefficient at given index to new value.
 */
void IIRSOSFilter::set_coeff(int index, double c) {
    if ((index < (6 * pD->numstages)) && (pD->coeffs != 0)) {
        pD->coeffs[index] = c;
        pD->coeffs_f32[index] = (float)c;
    }
    return;
}

/**
 * Return filter coefficient at index.
 */
double IIRSOSFilter::get_coeff(int index) {
    if ((index < (6 * pD->numstages)) && (pD->coeffs != 0)) {
        return pD->coeffs[index];
    }
    return 0.0f;
}

/**
 * Return number of coefficients used by the filter.
 */
int IIRSOSFilter::get_num_coeffs() { 
    if (pD->coeffs != 0) {
        return (6 * pD->numstages);
    }
    return 0;
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
void IIRSOSFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->coeffs!=0) && (pD->numchannels>0));

    /* --Matlab--
     * input = Giir * channels(:, ii);
     * for jj=1:
     *   presum = input - Ciir(jj,5)*storagemid(:,jj,1) - Ciir(jj,6)*storagemid(:,jj,2);          
     *   output = Ciir(jj,1)*presum + Ciir(jj,2)*storagemid(:,jj,1) + Ciir(jj,3)*storagemid(:,jj,2);
     *   storagemid(:,jj,2) = storagemid(:,jj,1);
     *   storagemid(:,jj,1) = presum;
     *   input = output;
     * end;
     * % When normalized: Ciir(jj,1)==1
     * % Always: Ciir(jj,4)==1
     */

    // Filter coefficients are real
    // For performance we can thus use <real,real> operations.
    Ipp32f* input = (Ipp32f*)freqbins;
    Ipp32f* out   = (Ipp32f*)pD->outputs;
    Ipp32f* t1    = (Ipp32f*)pD->z1;
    Ipp32f* t2    = (Ipp32f*)pD->z2;
    Ipp32f* Ciir  = pD->coeffs_f32;

    if (DEBUG_V) { cerr << flush << "filter() in[0]=" << freqbins[0].re << endl; }

    for (int jj=0; jj<(pD->numstages); jj++) {

        // presum = input - Ciir(jj,5)*storagemid(:,jj,1) - Ciir(jj,6)*storagemid(:,jj,2);
        ippsMulC_32f(t1, -Ciir[4], /*dst*/pD->pre_sum, pD->N);
        ippsAddProductC_32f(t2, -Ciir[5], /*srcdst*/pD->pre_sum, pD->N);
        if (jj==0) {
            ippsAddProductC_32f(input, pD->prescaling_f32, /*srcdst*/pD->pre_sum, pD->N);
        } else {
            ippsAdd_32f_I(input, /*srcdst*/pD->pre_sum, pD->N);
        }

        // output = Ciir(jj,1)*presum + Ciir(jj,2)*storagemid(:,jj,1) + Ciir(jj,3)*storagemid(:,jj,2);
        if (Ciir[0] == 1) {
            ippsCopy_32f(pD->pre_sum, out, pD->N);
        } else {
            ippsMulC_32f(pD->pre_sum, Ciir[0], out, pD->N);
        }
        ippsAddProductC_32f(t1, Ciir[1], out, pD->N);
        ippsAddProductC_32f(t2, Ciir[2], out, pD->N);

        if (DEBUG_V) { cerr << std::flush << "jj=" << jj << " in=" << input[0]<< " out=" << out[0] << " sum=" << pD->pre_sum[0] << " s[0]=" << t1[1] << " s[1]=" << t2[0] << endl; }

        // memory shifting
        ippsCopy_32f(t1,  /*dst*/t2, pD->N);
        ippsCopy_32f(pD->pre_sum, /*dst*/t1, pD->N);

        // pointers for next stage
        input = out;
        t1 += pD->N;
        t2 += pD->N;
        Ciir += COEFFS_PER_STAGE;
    }
}

/**
 * Return current states
 */
Ipp32fc* IIRSOSFilter::y() {
    return (Ipp32fc*)(pD->outputs);
}

/**
 * Allow user to specify own result buffer.
 */
void IIRSOSFilter::setUserOutbuffer(Ipp32fc* userY) {
    if (!pD->user_output) { ippsFree(pD->outputs); }
    pD->user_output = true;
    pD->outputs = (Ipp32f*)userY;
    clear();
}
 
