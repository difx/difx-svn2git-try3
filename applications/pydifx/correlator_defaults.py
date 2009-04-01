# Various parameters which are default for *all* observation

# NONE of these variables are inherited directly by any of the scripts
# They are all inherited via observation.py
# Therefore if you need to make many experiment-specific changes, they
# they can be made in the experiment-specific observation.py
from logging import DEBUG
from os import getenv
difxroot = getenv('DIFXROOT')
if not difxroot:
    difxroot = ''
mpiroot = getenv('MPIROOT')
if not mpiroot:
    mpiroot = ''
difxbin = difxroot + '/bin'

#######################################################################
#executables
#######################################################################
# mpirun
mpi = mpiroot + '/bin/mpirun'
# mpifxcorr
mpifxcorr = difxbin = '/mpifxcorr'
# difx2fits
difx2fits = difxbin = '/difx2fits'
#remote shell used to connect to machines (used by killdifx)
rsh = 'ssh'
# checkCalcServer host
checkCalc = difxbin + '/checkCalcServer localhost'
# startCalcServer
startCalc = difxbin + '/startCalcServer'

#######################################################################
# calcif
#######################################################################
calcif_options = '-v'
calcif_timeout = 240

#######################################################################
# difx2fits
#######################################################################
difx2fits_options = '-v -v -s 1'
difx2fits_delete = False

#######################################################################
# eop.py
#######################################################################
eop_path = difxroot + "/calc"
iat_path = eop_path
# can get TAI-UTC from here but they seem to be out of date
# http://gemini.gsfc.nasa.gov/500/oper/solve_apriori_files/ut1ls.dat
iat_url = "http://maia.usno.navy.mil/ser7/tai-utc.dat"
eop_url = "http://gemini.gsfc.nasa.gov/solve_save/usno_finals.erp"

eop_extra = 2
eop_download = False
eop_force = False

#######################################################################
# killdifx.py
#######################################################################
killdifx_options = ''

#######################################################################
# Cluster path (used by machinegen)
#######################################################################
cluster_path = difxroot + "/share/grid.cluster"

#######################################################################
# calc file defaults
#######################################################################
job_id = 1
spectral_average = 1
taper_function = 'UNIFORM'
offset = 0
increment = 1.0

tail = 30
download = False
force = False

#######################################################################
# log2clock / log2input defaults
#######################################################################
starttime = None

#######################################################################
# mpifxcorr.py defaults
#######################################################################
mpifxcorr_timeout = 120

#######################################################################
# spawn.py defaults
#######################################################################
spawn_timeout = 30

#######################################################################
# vex2flag defaults
#######################################################################
flag_shrink = 0
flag_printuv = False
flag_flagfilename = 'flag'
