#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "mk5daemon.h"

struct mk5dirParams
{
	Mk5Daemon *D;
	char bank[10];
};

static void *mk5dirRun(void *ptr)
{
	struct mk5dirParams *params;
	char command[MAX_COMMAND_SIZE];

	params = (struct mk5dirParams *)ptr;

	Logger_logData(params->D->log, "mk5dir starting\n");

	snprintf(command, MAX_COMMAND_SIZE, "su -l difx -c 'mk5dir %s'", 
		params->bank);
	command[MAX_COMMAND_SIZE-1] = 0;
	Mk5Daemon_system(params->D, command, 1);

	Logger_logData(params->D->log, "mk5dir done\n");

	params->D->processDone = 1;

	pthread_exit(0);

	free(params);

	return 0;
}

void Mk5Daemon_startMk5Dir(Mk5Daemon *D, const char *bank)
{
	struct mk5dirParams *P;

	P = (struct mk5dirParams *)calloc(1, sizeof(struct mk5dirParams));

	if(!D->isMk5)
	{
		return;
	}

	pthread_mutex_lock(&D->processLock);

	if(D->process == PROCESS_NONE)
	{
		D->processDone = 0;
		D->process = PROCESS_MK5DIR;

		P->D = D;
		strncpy(P->bank, bank, 9);
		P->bank[9] = 0;
		pthread_create(&D->processThread, 0, &mk5dirRun, P);
	}

	pthread_mutex_unlock(&D->processLock);
}

void Mk5Daemon_stopMk5Dir(Mk5Daemon *D)
{
	Mk5Daemon_system(D, "killall -HUP mk5dir", 1);
}
