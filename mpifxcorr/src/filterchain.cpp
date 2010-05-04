/********************************************************************
 * Ultra-narrow filters for VLBI software correlation
 * (C) 2010 Jan Wagner MPIfR
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

#include "filterchain.h"
#include "filters.h"

/** C'stor clear the chain */
FilterChain::FilterChain() { 
    fchain.clear();
    rchain.clear();
    countchain.clear();
}

/** D'stor free interal allocs */
FilterChain::~FilterChain() {
    std::vector<int*>::iterator citr = countchain.begin();
    int len = countchain.size();
    while ((len--)>0) {
        delete (*citr);
        citr++;
    }
    countchain.clear();
    rchain.clear();
    fchain.clear();
}

/**
 * Append filter to end of chain.
 * @arg fp Pointer to filter object to add
 * @arg R  Decimation factor to apply to filter input (R=0,R=1: no decimation)
 */
void FilterChain::appendFilter(Filter* fp, int R) {
    int* Rp = new int(0);
    fchain.push_back(fp);
    rchain.push_back(R);
    countchain.push_back(Rp);
}

/**
 * Clear filters in the chain.
 */
void FilterChain::clear() {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    std::vector<int*>::iterator citr = countchain.begin();
    int len = fchain.size();
    while ((len--)>0) {
        (*fitr)->clear();
        *(*citr) = 0;
        fitr++;
        citr++;
    }
}

/**
 * Init filters in the chain.
 */
void FilterChain::init(double w_cutoff, size_t N) {
    this->clear();
    std::vector<Filter*>::iterator fitr = fchain.begin();
    int len = fchain.size();
    while ((len--)>0) {
        (*fitr)->init(w_cutoff, N);
        fitr++;
     }
}

/**
 * Process data along the filter chain,
 * taking into account also the decimation factors.
 */
void FilterChain::filter(Ipp32fc* in) {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    std::vector<int>::iterator ritr = rchain.begin();
    std::vector<int*>::iterator citr = countchain.begin();
    int len = fchain.size();
    for (int i=0; i<len; i++) {
        // Increment R-counter
        *(*citr)++;
        if (*(*citr) >= *ritr) {
            // Target R reached, take output of prev filter as input of curr filter
            Ipp32fc *yy = (i==0) ? in : (*(fitr-1))->y();
            (*fitr)->filter(yy);
            *(*citr) = 0;
        }
        fitr++;
        citr++;
        ritr++;
    }

}

/**
 * Return pointer to output of last filter.
 */
Ipp32fc* FilterChain::y() {
    std::vector<Filter*>::iterator fitr = fchain.end() - 1;
    return (*fitr)->y();
}

