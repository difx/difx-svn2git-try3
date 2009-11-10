bin_PROGRAMS = \
	vex2difx \
	vex2script \
	vexpeek

LDADD = \
	$(top_builddir)/vex/libvex.la \
	-lfl

vex2difx_SOURCES = \
	vextables.cpp \
	corrparams.h \
	vextables.h \
	vexload.h \
	vex2difx.cpp \
	vexload.cpp \
	corrparams.cpp

vex2script_SOURCES = \
	pystream.h \
	corrparams.h \
	vextables.h \
	vexload.h \
	vextables.cpp \
	vex2script.cpp \
	vexload.cpp \
	pystream.cpp \
	corrparams.cpp

vexpeek_SOURCES = \
	corrparams.h \
	vextables.h \
	vexload.h \
	vexpeek.cpp \
	vextables.cpp \
	vexload.cpp \
	corrparams.cpp
