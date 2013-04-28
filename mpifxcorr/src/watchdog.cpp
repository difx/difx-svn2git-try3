
time_t watchdogTime;
int watchdogVerbose;
char watchdogStatement[256];
pthread_mutex_t watchdogLock;
char watchdogXLRError[XLR_ERROR_LENGTH+1];
int watchdogTimeout;
pthread_t watchdogThread;

void *watchdogFunction(void *data)
{
	VDIFMark5DataStream *nativeMk5 = reinterpret_cast<VDIFMark5DataStream *>(data);
	int deltat;
	int lastdeltat = 0;

	for(;;)
	{
		usleep(100000);
		pthread_mutex_lock(&watchdogLock);

		if(strcmp(watchdogStatement, "DIE") == 0)
		{
			pthread_mutex_unlock(&watchdogLock);
			
			return 0;
		}
		else if(watchdogTime != 0)
		{
			deltat = time(0) - watchdogTime;
			if(deltat > 60)  // Nothing should take 60 seconds to complete!
			{
				cfatal << startl << "Watchdog caught a hang-up executing: " << watchdogStatement << " Aborting!!!" << endl;
				if(nativeMk5)
				{
					nativeMk5->sendMark5Status(MARK5_STATE_ERROR, 0, 0.0, 0.0);
				}
#if HAVE_MARK5IPC
				unlockMark5();
#endif
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
			else if(deltat != lastdeltat && deltat > 8)
			{
				cwarn << startl << "Waiting " << deltat << " seconds executing: " << watchdogStatement << endl;
				lastdeltat = deltat;
			}
		}
		else
		{
			lastdeltat = 0;
		}

		pthread_mutex_unlock(&watchdogLock);
	}
}

int initWatchdog()
{
	int perr;

	pthread_mutex_init(&watchdogLock, NULL);
	watchdogStatement[0] = 0;
	watchdogXLRError[0] = 0;
	watchdogTime = 0;
	watchdogTimeout = 20;
	perr = pthread_create(&watchdogThread, NULL, watchdogFunction, 0);

	if(perr != 0)
	{
		fprintf(stderr, "Error: could not launch watchdog thread!\n");

		return -1;
	}

	return 0;
}

void stopWatchdog()
{
	pthread_mutex_lock(&watchdogLock);
	strcpy(watchdogStatement, "DIE");
	pthread_mutex_unlock(&watchdogLock);
	pthread_join(watchdogThread, NULL);
}
