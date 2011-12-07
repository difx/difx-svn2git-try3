/********************************************************************
 * Use filter chain library and external coeff file
 * to filter a test signal
 *
 * (C) 2011 Jan Wagner MPIfR
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
#include <cmath>
#include <cstdlib>
using std::cout;
using std::endl;
using std::flush;

#define N_CHANNELS  32

/*******************************************************************
 %% Example for showing log file as a plot in Octave/Matlab
 data   = load('-ascii', 'filter.log');
 sigin  = data(:,1); sigout = data(:,2);
 figure(1), clf;
 subplot(2,1,1), plot(sigin), title('Input waveform');
 subplot(2,1,2), plot(sigout), title('Filter output waveform');
*******************************************************************/

////////////////////////////////////////////////////////////////////
// Timing auto-class
////////////////////////////////////////////////////////////////////
class Timing {
   public:
     Timing() : _msg(std::string("")),_samples(8000*1024.0) 
        { start(); }
     Timing(const char* msg, double samples) : _msg(std::string(msg)),_samples(samples) 
        { start(); }
     ~Timing() { 
         double t = stop();
         double msps = _samples*1e-6 / t;
         cout << _msg << ": " << (1e6*t) << " usec  "
              << msps << " Ms/s " << endl << flush; 
     }
   public:
     void start() { gettimeofday(&_tstart, NULL); }
     double stop() { 
         double dsec;
         gettimeofday(&_tstop, NULL);
         dsec = (_tstop.tv_sec - _tstart.tv_sec);
         dsec += 1e-6*(_tstop.tv_usec - _tstart.tv_usec);
         return dsec;
     }
   private:
     std::string _msg;
     double _samples;
     struct timeval _tstart;
     struct timeval _tstop;
};


////////////////////////////////////////////////////////////////////
// Result comparison
////////////////////////////////////////////////////////////////////
void cout_precision_init() {
    //cout.setf(std::ios::scientific,std::ios::floatfield);
    cout.precision(10);
}
bool quite_equal(const double a, const double b) {
    double m = std::min(std::abs(a)+0.5, std::abs(b)+0.5);
    return (std::abs(a-b)<(m*1e-9));
}
bool quite_equal(const Ipp32fc a, const Ipp32fc b) {
    return (quite_equal((double)a.re,(double)b.re) && quite_equal((double)a.im,(double)b.im));
}

void compare_to_ref(const Ipp32fc act, const Ipp32fc ref)
{
    double e_re = std::abs(act.re - ref.re);
    double e_im = std::abs(act.im - ref.im);
    cout << "Final filter output   : {" << act.re << ", " << act.im << "}" << endl;
    cout << "Expected filter output: {" << ref.re << ", " << ref.im << "}" << endl;
    cout << "Error                 : {e=" << e_re   << ", e=" << e_im << "}" << endl;
    if (quite_equal(act,ref))
        cout << "PASS" << endl;
    else
        cout << "FAIL" << endl;
}

////////////////////////////////////////////////////////////////////
// MAIN choose tests
////////////////////////////////////////////////////////////////////
int main(int argc, const char** argv)
{
    ippStaticInit();
    cout_precision_init();

    FilterChain fc;
    if (argc < 4) {
       cout << "\nUsage: filter_data <filterchain.coeff> <Nsamples> <sigtype> [<param1>]\n\n"
            << "   Filter chain setup is loaded from the coeff file.\n"
            << "   Test data of length Nsamples is run through the filter.\n"
            << "   Test data generators are:\n"
            << "     sigtype   generated data\n"
            << "     0         impulse (x[n]={1 for n=1, 0 for n!=1})\n"
            << "     1         step    (x[n]=1 for all n)\n"
            << "     2         sine wave; param1=normalized frequency (0<(f/2fs)<1.0)\n"
            << "     3         noise   (pseudo-random, rand())\n"
            << "\n";
       return 0;
    }

    const int Nch = N_CHANNELS;
    const char* fn         = argv[1];
    unsigned long Nsamples = atoll(argv[2]);
    int sigtype            = atoi(argv[3]);
    double param1          = 0;
    if (argc==5) {
       param1 = atof(argv[4]);
    }

    Ipp32fc* in  = ippsMalloc_32fc(Nch);
    Ipp32fc* out = ippsMalloc_32fc(Nch);

    fc.buildFromFile(fn, Nch);
    fc.summary(cout);
    fc.setUserOutbuffer(out);

    std::ofstream logfile("filter.log");

    ippsZero_32fc(in,  Nch);
    ippsZero_32fc(out, Nch);

    double stat_mean_in = 0;
    double stat_mean_out = 0;
    double stat_min_out = 10;
    double stat_max_out = -10;

    for (unsigned long n=0; n<Nsamples; n++) {

       Ipp32fc xc;
       float x;

       switch (sigtype) {
          case 0:
             x = (n==0) ? 1.0f : 0.0f;
             break;
          case 1:
             x = 1.0f;
             break;
          case 2:
             x = std::sin(param1 * n);
             break;
          case 3:
             x = float(std::rand()) / float(RAND_MAX);
             break;
          default:
             x = 1.0f;
       }

       xc.re = x;
       xc.im = x;
       ippsSet_32fc(xc, in, Nch);

       fc.filter(in);

       float y = fc.y()->re;
       logfile << x << " " << y << "\n";

       if (1) {
          cout << "x[" << (n+1) << "] = " << x;
          cout << " ; y[" << (n+1) << "] = {" << y << "," << fc.y()->im << "}\n";
       }

       stat_mean_in  += x;
       stat_mean_out += y;
       if (y < stat_min_out) stat_min_out = y;
       if (y > stat_max_out) stat_max_out = y;
    }

    stat_mean_in  /= double(Nsamples);
    stat_mean_out /= double(Nsamples);
    cout << "Mean of input : " << stat_mean_in << "\n";
    cout << "Mean of output: " << stat_mean_out << "\n";
    cout << "Min  of output: " << stat_min_out << "\n";
    cout << "Max  of output: " << stat_max_out << "\n";

    ippsFree(in);
    ippsFree(out);
    logfile.close();

    return 0;
}
