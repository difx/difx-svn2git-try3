IPP_LIBS = -L/cluster/intel/ipp/6.1.2.051/em64t/sharedlib -lippsem64t -lguide -lippvmem64t -lippcoreem64t -lpthread
CXXFLAGS = -g -pthread -O2 -I/home/swcorel/DiFX-trunk-rfi/compiled/include   -I/home/swcorel/DiFX-trunk-rfi/compiled/include/vdifio   -I/cluster/intel/ipp/6.1.2.051/em64t/include   -I/home/swcorel/DiFX-trunk-rfi/compiled/include   -I/home/swcorel/DiFX-trunk-rfi/compiled/include
#  -MD -MP -MF
CXXLDFLAGS = -v $(IPP_LIBS)

all: fma_test

fma_test: fma_test.o
	g++ $(IPP_LIBS) fma_test.o -o fma_test

clean:
	rm -f fma_test.o fma_test

