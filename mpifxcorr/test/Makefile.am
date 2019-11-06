
check_PROGRAMS = configuration_test sysutil_test

configuration_test_SOURCES = configuration_test.cpp \
	../src/alert.cpp \
	../src/configuration.cpp \
	../src/mode.cpp \
	../src/model.cpp \
	../src/mk5mode.cpp \
	../src/mathutil.cpp \
	../src/polyco.cpp \
	../src/sysutil.cpp \
	../src/pcal.cpp

configuration_test_CXXFLAGS = -g -I../src/ $(AM_CXXFLAGS)

sysutil_test_SOURCES = sysutil_test.cpp \
	../src/alert.cpp \
	../src/sysutil.cpp

sysutil_test_CXXFLAGS = -g -I../src/ $(AM_CXXFLAGS)
