/********************************************************************
 * Ultra-narrow filters for VLBI software correlation
 * (C) 2010 Jan Wagner MPIfR
 ********************************************************************
 *
 * Filter input is always an array of single samples from multiple
 * channels. With N channel data, you can think of it as a group
 * of N parallel filters. Filtering output is the output of these N
 * parallel filters.
 * The filter is implemented as an IIR biquad/second-order-structure
 * series of filters. Coefficients are fixed...
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

// Notes:
// http://www.cs.uiuc.edu/class/sp08/cs232/section/Discussion13/disc13.pdf
// http://gcc.gnu.org/onlinedocs/gcc/Vector-Extensions.html
// http://gcc.gnu.org/onlinedocs/gcc/X86-Built_002din-Functions.html

#include "filters.h"

#include <cassert>
#include <iostream>
using std::cerr;
using std::endl;
using std::flush;

#include <memory.h>
#include <malloc.h>

////////////////////////////////////////////////
// IIR IN CUSTOM SOS/BIQUAD not covered by IPP
///////////////////////////////////////////////

/**
 * Private data class
 */
struct IIRSOSFilter::P {
   public:
    int N;
    IppsIIRState_32fc** iirstates;
    Ipp32fc* y;
    Ipp32fc* tapsBA;
    const int order;
   public:
    P() : order(8) {}
};

/**
 * Prepare private data
 */
IIRSOSFilter::IIRSOSFilter() {
    pD = new IIRSOSFilter::P();
    pD->N = -1;
    pD->iirstates = 0;
    pD->tapsBA = 0;
}

/**
 * Release private data
 */
IIRSOSFilter::~IIRSOSFilter() {
    if (pD->iirstates != 0) {
        for (int i=0; i<pD->N; i++) {
            ippsIIRFree_32fc(pD->iirstates[i]);
        }
        delete[] pD->iirstates;
    }
    delete pD;
}

/**
 * Reset filter state
 */
void IIRSOSFilter::clear() {
}

/**
 * Re-initialize filter. Note that the input argument
 * will be ignored. Filter cutoff depends on how
 * many input samples are added for averaging.
 * 
 * @arg w_cutoff Normalized cutoff frequency (ignored!)
 * @arg N Number of channels i.e. parallel filters
 */
void IIRSOSFilter::init(double w_cutoff, size_t N) {
    IppStatus s;
    // no coefficients etc to compute
    if ((N==pD->N) && (pD->iirstates!=0)) {
        clear();
    } else {
        pD->N = N;
        // alloc outputs
        pD->y = ippsMalloc_32fc(pD->N);
        // let IPP design a filter?
        Ipp64f tapsBAlp[2*(pD->order + 1)];
        s = ippsIIRGenLowpass_64f(w_cutoff/2, 1.0, pD->order, tapsBAlp, ippChebyshev1);
        if (s == ippStsNoErr) {
            for (int i = 0; i < (pD->order+1); i++) {
                fprintf(stderr, "b[%d]=%e a[%d]=%e\n", i, tapsBAlp[i], i, tapsBAlp[i+pD->order+1]);
            }
        }
        // or use Matlab IIR filter (note: direct form, no SOS/biquad)
        // Num = [0.028  0.053 0.071  0.053 0.028]
        // Den = [1.000 -2.026 2.148 -1.159 0.279]
        pD->tapsBA = new Ipp32fc[2*(pD->order + 1)];
        //        pD->tabsBA = {
        pD->iirstates = new IppsIIRState_32fc*[pD->N];
        for (int i=0; i<pD->N; i++) {
            s = ippsIIRInitAlloc_32fc(&(pD->iirstates[i]), pD->tapsBA, pD->order, NULL);
            if (s != ippStsNoErr) {
                fprintf(stderr, "ippsIIRInitAlloc_32fc error %d\n", s);
            }
        }
    }
}

/**
 * Pass new data to filter
 * @arg x array of single samples from multiple channels
 */
void IIRSOSFilter::filter(Ipp32fc* freqbins) {
    assert((pD!=0) && (pD->iirstates!=0));
    for (int i=0; i<pD->N; i++) {
        ippsIIROne_32fc(freqbins[i], &(pD->y[i]), pD->iirstates[i]);
    }
}

/**
 * Return current states
 */
Ipp32fc* IIRSOSFilter::y() {
    return pD->y;
}

