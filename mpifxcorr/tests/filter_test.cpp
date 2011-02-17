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
#include "filterhelpers.h"
#include <sys/time.h>
#include <iostream>
#include <iomanip>
#include <fstream>
using std::cout;
using std::endl;

#define VERIFY1 0 // verification test with input file
#define VERIFY2 1 // verification test with fixed DC-level '1.0'
#define BENCH   0 // benchmarking test

class Timing {
   public:
     Timing()  { start(); }
     ~Timing() { cout << "dT=" << stop() << endl << std::flush; };
   public:
     void start() { gettimeofday(&tstart, NULL); }
     double stop() { 
         gettimeofday(&tstop, NULL);
         return (tstop.tv_sec - tstart.tv_sec) + 1e-6*(tstop.tv_usec - tstart.tv_usec);
     }
   private:
     struct timeval tstart;
     struct timeval tstop;
};

int main(int argc, char** argv)
{
    const int Nch = 1024;
    const int NI = 100;
    const int Nsamps = 80000;

    ippStaticInit();

    cout << "---- Test helper class" << endl;
    std::vector<double> vec;
    Helpers::parse_numerics_file(vec, "commentfile.txt");
    for (int i=0; i<vec.size(); i++) { cout << " " << vec.at(i); }
    cout << endl;

    cout << "---- Test filter builder" << endl;
    Ipp32fc* userY = ippsMalloc_32fc(Nch);
    ippsZero_32fc(userY, Nch);
    Ipp32fc* tvec = ippsMalloc_32fc(Nch);
    Filter* f = Filter::getFilter(FLT_AVERAGING);
    f->init(0.01, Nch);
    f->clear();
    for (int i=0; i<NI; i++) {
        f->filter(tvec);
    }
    delete f;
    ippsFree(tvec);

    cout << "---- Test filter chain loader <iirsoschain.txt>" << endl;
    FilterChain fc;
    fc.buildFromFile("iirsoschain.txt", /*channels:*/Nch);
    fc.summary(cout);

    Ipp32fc* oneChVec = ippsMalloc_32fc(Nsamps);
    Ipp32fc* allChVec = ippsMalloc_32fc(Nch);
    ippsSet_32f(1.0f, (Ipp32f*)allChVec, 2*Nch);

    cout << "---- Reading samples from <pulse_32fc.raw>" << endl;
    std::ifstream ifile("pulse_32fc.raw");
    ifile.read((char*)oneChVec, Nsamps*sizeof(Ipp32fc));
    cout << "Read " << (ifile.gcount()/sizeof(Ipp32fc)) << " complex samples." << endl;

#if VERIFY1
  {
    fc.setUserOutbuffer(userY);
    for (int i=0; i<Nsamps; i++) {
        ippsSet_32fc(oneChVec[i], allChVec, Nch);
        fc.filter(allChVec);
        Ipp32fc* y = fc.y();
        for (int j=0; j<Nch; j++) {
            if ((y[j].re != y[0].re) && (y[j].im != y[0].im)) {
                cout << "Filter out y[chN]!=y[ch0] : FAIL" << endl;
                break;
            }
        }
    }
    Ipp32fc* y = fc.y();
    cout << "Final filter value: " << std::setprecision(17) << y[0].re << " + i" << y[0].im << endl;
    Ipp32fc sum = {0.0f, 0.0f};
    Ipp32fc sum2 = {0.0f, 0.0f};
    for (int i=0; i<Nsamps; i++) {
        sum.re += oneChVec[i].re;
        sum.im += oneChVec[i].im;
        sum2.re += sum.re;
        sum2.im += sum.im;
    }
    cout << "Expected integrated value: " << sum.re << " + i" << sum.im << endl;
    cout << "Expected double-integrated value: " << sum2.re << " + i" << sum2.im << endl;
  }
#endif

#ifdef VERIFY2
 {
    int NfixSamps = 32000;
    if (argc == 2) {
       NfixSamps = atof(argv[1]);
    }
    Ipp32fc cval = {1.5f, 2.0f};
    fc.setUserOutbuffer(userY);
    fc.clear();
    cout << "---- Filtering " << NfixSamps << " 1.5+i2.0-valued samples at " << Nch << " parallel channels (" << NfixSamps*Nch << " complex samples)" << endl;
    Timing timing;
    for (int i=0; i<NfixSamps; i++) {
        ippsSet_32fc(cval, allChVec, Nch);
        fc.filter(allChVec);
    }
    double dT = timing.stop();
    Ipp32fc* y = userY; //fc.y();
    cout << "Final filter value ch0: " << std::setprecision(17) << y[0].re << " + i" << y[0].im << endl;
    cout << "Final filter value ch1: " << std::setprecision(17) << y[1].re << " + i" << y[1].im << endl;
    cout << "Final filter value chN: " << std::setprecision(17) << y[Nch-1].re << " + i" << y[Nch-1].im << endl;
    cout << "Filter speed: " << 1e-6*((double)NfixSamps*Nch)/dT << " Msamp/sec (*2 if not complex)" << endl;
  }
#endif

#if BENCH
  {
    cout << "---- Filtering speed test with <pulse_32fc.raw> with chain and " << Nch << " parallel channels" << endl;
    fc.clear();
    Timing timing;
    for (int i=0; i<Nsamps; i++) {
        // ippsSet_32fc(oneChVec[i], allChVec, Nch);
        allChVec[0].re = oneChVec[i].re;
        allChVec[0].im = oneChVec[i].im;
        fc.filter(allChVec);
    }
    double dT = timing.stop();
    cout << "Filter speed: " << 1e-6*((double)Nsamps*Nch)/dT << " Mcomplex/sec" << endl;
    Ipp32fc* y = fc.y();
    cout << "Final filter value: " << std::setprecision(17) << y[0].re << " + i" << y[0].im << endl;
  }
#endif

    ippsFree(oneChVec);
    ippsFree(allChVec);
    cout << "---- END" << endl;
    return 0;
}
