# Makefile
# Makefile for RadioAstron fringe finding tools
# 2013 Apr 18  JMA  --- copy from Eff_recording/src/Makefile


SHELL = /bin/bash

INSTALLDIR = ${DIFXROOT}/


# Version numbering for the libiri library
VER_MAJOR = 0
VER_MINOR = 0
VER_SUBSUB = 0





# do not use debugging by default
DEBUG=0
# do not make a shared library by default
SHARED = 0
EXT = a



# define the compiler
CC = gcc
F77 = gfortran
C++ = g++

CPP = cpp -P

INCLUDES = -I.  -I$(INSTALLDIR)/include -I/usr/include/cfitsio
DEFINES = 
LIBRARY_FLAGS = -L$(INSTALLDIR)/lib





CFLAGS = -march=native -O3 -ffast-math -funroll-loops -Wall $(INCLUDES) $(DEFINES) 
FFLAGS = -march=native -O3 -Wall $(INCLUDES) $(DEFINES) 
DEFINES = -D_FILE_OFFSET_BITS=64 -pthread

ifeq ($(DEBUG),0)
  CFLAGS += 
  FFLAGS += 
else
  # -pg for profiling
  #DEFINES += -DDEBUG=$(DEBUG) -DLOFAR_DEBUG=3
  #DEFINES += -DDEBUG=$(DEBUG) -DLOFAR_DEBUG=2
  DEFINES += -DDEBUG=$(DEBUG) -DLOFAR_DEBUG=1
  CFLAGS += 
  FFLAGS += 
endif

ifeq ($(SHARED),0)
	EXT = a
else
	CFLAGS += -fPIC
	FFLAGS += -fPIC
	EXT = so
endif

CXXFLAGS = $(CFLAGS) -std=c++0x







# set up a postfix to use for file and directory names
ifeq ($(DEBUG),0)
  POSTFIX=$(OSTYPE)
else
  POSTFIX=$(OSTYPE)_debug
endif







RM = /bin/rm -f
CP = /bin/cp 
MKDIR = /bin/mkdir -p
MV = /bin/mv
AR = ar



HEADERS = \







SRCS = \
	fft_fringe.cxx \
	join_fringes.cxx


OBJS = \


PROG = \
	dumpRadioAstronBaseline \
	fft_fringe \
	find_RadioAstron_Fringe \
	join_fringes

LIBS = \

PYTHONPROG = \





all: $(PROG)
.PHONY : all



#find_line_max: $(OBJS) find_line_max.o
#	$(CC) $(CFLAGS) -o $@ $(OBJS) find_line_max.o -lm -lsupc++

dumpRadioAstronBaseline: dumpRadioAstronBaseline.o
	$(C++) $(CFLAGS) -o $@ $@.o -lcfitsio -lm

fft_fringe: fft_fringe.o
	$(C++) $(CFLAGS) -o $@ $@.o $(LIBRARY_FLAGS) -lfftw3f -lm

find_RadioAstron_Fringe:

join_fringes: join_fringes.o
	$(C++) $(CFLAGS) -o $@ $@.o $(LIBRARY_FLAGS) -lfftw3f -lm





# This will probably work best with C and C++ code.  Modify
# If you want to do this for FORTRAN too.
.PHONY : clean distclean install


clean:
	$(RM) $(OBJS)
	$(RM) *.o

distclean: clean
	$(RM) *~
	$(RM) $(PROG) $(LIBS)


install: $(LIBS) $(PROG)
	$(CP) $(PROG) $(PYTHONPROG) $(INSTALLDIR)/bin
#	$(CP) $(LIBS) $(INSTALLDIR)/lib
#	$(CP) $(HEADERS) $(INSTALLDIR)/include




# make sure we can compile C++ stuff
.SUFFIXES:
.SUFFIXES: .cxx $(SUFFIXES)









# declare our own default compilation rules
.cxx.o:
	$(C++) $(CXXFLAGS) -c $<

.F.o:
	$(F77) $(FFLAGS) -c $<

.f.o:
	$(F77) $(FFLAGS) -c $<











