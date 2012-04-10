if WITH_SS
mark5_files = \
	nativemk5.cpp \
	mark5dir.cpp
else
mark5_files = \
	nativemk5_stubs.cpp
endif

bin_PROGRAMS = mpifxcorrfake 

mpifxcorrfake_SOURCES = \
	mpifxcorr.cpp \
	fxmanager.cpp \
	core.cpp \
	datastream.cpp \
	visibility.cpp \
	configuration.cpp \
	mode.cpp \
        model.cpp \
	mk5.cpp \
	mk5mode.cpp \
	polyco.cpp \
	alert.cpp \
	pcal.cpp \
	switchedpower.cpp \
	datamuxer.cpp \
	$(mark5_files)

