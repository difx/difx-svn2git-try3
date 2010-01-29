#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include "mk5daemon.h"

const char defaultMpiWrapper[] = "mpirun";
const char defaultMpiOptions[] = "--mca btl ^udapl,openib --mca mpi_yield_when_idle 1";
const char defaultDifxProgram[] = "mpifxcorr";

typedef struct
{
	char hostname[DIFX_MESSAGE_PARAM_LENGTH];
	int n;
} Uses;

static int addUse(Uses *U, const char *hostname)
{
	int i;

	for(i = 0; U[i].n; i++)
	{
		if(strcmp(hostname, U[i].hostname) == 0)
		{
			U[i].n++;
			return i;
		}
	}

	strcpy(U[i].hostname, hostname);
	U[i].n = 1;

	return i;
}

int getUse(const Uses *U, const char *hostname)
{
	int i;

	for(i = 0; U[i].n; i++)
	{
		if(strcmp(hostname, U[i].hostname) == 0)
		{
			return U[i].n;
		}
	}

	return 0;
}

void Mk5Daemon_startMpifxcorr(Mk5Daemon *D, const DifxMessageGeneric *G)
{
	int i, l, n;
	int childPid;
	char filebase[DIFX_MESSAGE_FILENAME_LENGTH];
	char filename[DIFX_MESSAGE_FILENAME_LENGTH];
	char message[MAX_MESSAGE_SIZE];
	char command[MAX_COMMAND_SIZE];
	FILE *out;
	Uses *uses;
	const char *jobName;
	const DifxMessageStart *S;
	int outputExists = 0;
	const char *mpiOptions;
	const char *mpiWrapper;
	const char *difxProgram;
	int returnValue;


	S = &G->body.start;

	if(G->nTo != 1)
	{
		return;
	}

	if(strcmp(G->to[0], D->hostName) != 0)
	{
		return;
	}

	if(S->headNode[0] == 0 || S->nDatastream <= 0 || S->nProcess <= 0 || S->inputFilename[0] != '/')
	{
		difxMessageSendDifxAlert("Malformed DifxStart message received", DIFX_ALERT_LEVEL_ERROR);
		Logger_logData(D->log, "Mk5Daemon_startMpifxcorr: degenerate request\n");
		return;
	}

	if(!D->isHeadNode)
	{
		difxMessageSendDifxAlert("Attempt to start job from non head node", DIFX_ALERT_LEVEL_ERROR);
		Logger_logData(D->log, "Mk5Daemon_startMpifxcorr: I am not a head node\n");
		return;
	}

	if(strlen(S->inputFilename) + 12 > DIFX_MESSAGE_FILENAME_LENGTH)
	{
		difxMessageSendDifxAlert("Attempt to start job with filename that is too long", DIFX_ALERT_LEVEL_ERROR);

		return;
	}

	/* generate filebase */
	strcpy(filebase, S->inputFilename);
	l = strlen(filebase);
	for(i = l-1; i > 0; i--)
	{
		if(filebase[i] == '.')
		{
			filebase[i] = 0;
			break;
		}
	}
	jobName = filebase;
	for(i = 0; filebase[i]; i++)
	{
		if(filebase[i] == '/')
		{
			jobName = filebase + i + 1;
		}
	}

	if(access(S->inputFilename, F_OK) != 0)
	{
		snprintf(message, MAX_MESSAGE_SIZE, 
			"Input file %s does not exist.  Aborting correlation.",
			S->inputFilename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);

		snprintf(message, MAX_MESSAGE_SIZE,
			"Mk5Daemon_startMpifxcorr: input file %s does not exist\n", 
			S->inputFilename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		Logger_logData(D->log, message);

		return;
	}

	sprintf(filename, "%s.difx", filebase);
	if(access(filename, F_OK) == 0)
	{
		outputExists = 1;
	}
	
	if(outputExists && !S->force)
	{
		snprintf(message, MAX_MESSAGE_SIZE, 
			"Output file %s exists.  Aborting correlation.", 
			filename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		
		snprintf(message, MAX_MESSAGE_SIZE,
			"Mk5Daemon_startMpifxcorr: output file %s exists\n", 
			filename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		Logger_logData(D->log, message);

		return;
	}

	/* lock state.  Make sure to unlock if early return happens! */
	pthread_mutex_lock(&D->processLock);

	/* determine usage of each node */
	uses = (Uses *)calloc(1 + S->nProcess + S->nDatastream, sizeof(Uses));
	addUse(uses, S->headNode);
	for(i = 0; i < S->nProcess; i++)
	{
		addUse(uses, S->processNode[i]);
	}
	for(i = 0; i < S->nDatastream; i++)
	{
		addUse(uses, S->datastreamNode[i]);
	}


	/* write machines file */
	sprintf(filename, "%s.machines", filebase);
	out = fopen(filename, "w");
	if(!out)
	{
		snprintf(message, MAX_MESSAGE_SIZE,
			"Cannot open %s for write", filename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);

		sprintf(message, MAX_MESSAGE_SIZE,
			"Mk5Daemon_startMpifxcorr: cannot open %s for write\n", 
			filename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		Logger_logData(D->log, message);

		pthread_mutex_unlock(&D->processLock);
		free(uses);
		
		return;
	}

	fprintf(out, "%s slots=1 max-slots=%d\n", S->headNode, getUse(uses, S->headNode));
	for(i = 0; i < S->nDatastream; i++)
	{
		n = getUse(uses, S->datastreamNode[i]);
		fprintf(out, "%s slots=1 max-slots=%d\n", S->datastreamNode[i], n);
	}
	for(i = 0; i < S->nProcess; i++)
	{
		n = getUse(uses, S->processNode[i]);
		fprintf(out, "%s slots=1 max-slots=%d\n", S->processNode[i], n);
	}

	fclose(out);
	/* change ownership and permissions to match the input file */
	snprintf(command, MAX_COMMAND_SIZE, "chown --reference=%s %s", 
		S->inputFilename, filename);
	command[MAX_COMMAND_SIZE-1] = 0;
	Mk5Daemon_system(D, command, 1);
	
	snprintf(command, MAX_COMMAND_SIZE, "chmod --reference=%s %s", 
		S->inputFilename, filename);
	command[MAX_COMMAND_SIZE-1] = 0;
	Mk5Daemon_system(D, command, 1);


	/* write threads file */
	sprintf(filename, "%s.threads", filebase);
	
	out = fopen(filename, "w");
	if(!out)
	{
		snprintf(message, MAX_MESSAGE_SIZE, "Cannot open %s for write", 
			filename);
		message[MAX_MESSAGE_SIZE-1] = 0;
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		
		snprintf(message, MAX_MESSAGE_SIZE, 
			"Mk5Daemon_startMpifxcorr: cannot open %s for write\n", 
			filename);
		message[MAX_MESSAGE_SIZE-1] = 0;

		Logger_logData(D->log, message);
		pthread_mutex_unlock(&D->processLock);
		free(uses);

		return;
	}

	fprintf(out, "NUMBER OF CORES:    %d\n", S->nProcess);

	for(i = 0; i < S->nProcess; i++)
	{
		n = S->nThread[i] - getUse(uses, S->processNode[i]) + 1;
		if(n <= 0)
		{
			n = 1;
		}
		fprintf(out, "%d\n", n);
	}

	fclose(out);
	/* change ownership and permissions to match the input file */
	snprintf(command, MAX_COMMAND_SIZE, "chown --reference=%s %s", 
		S->inputFilename, filename);
	command[MAX_COMMAND_SIZE-1] = 0;
	Mk5Daemon_system(D, command, 1);

	snprintf(command, MAX_COMMAND_SIZE, "chmod --reference=%s %s", 
		S->inputFilename, filename);
	command[MAX_COMMAND_SIZE-1] = 0;
	Mk5Daemon_system(D, command, 1);


	/* Don't need usage info anymore */
	free(uses);

	pthread_mutex_unlock(&D->processLock);

	if(S->mpiOptions[0])
	{
		mpiOptions = S->mpiOptions;
	}
	else
	{
		mpiOptions = defaultMpiOptions;
	}

	if(S->mpiWrapper[0])
	{
		mpiWrapper = S->mpiWrapper;
	}
	else
	{
		mpiWrapper = defaultMpiWrapper;
	}

	if(S->difxProgram[0])
	{
		difxProgram = S->difxProgram;
	}
	else
	{
		difxProgram = defaultDifxProgram;
	}

	childPid = fork();

	/* here is where the spawning of mpifxcorr happens... */
	if(childPid == 0)
	{
		const char *user;

		user = getenv("DIFX_USER_ID");
		if(!user)
		{
			user = difxUser;
		}

		if(S->force && outputExists)
		{
			snprintf(command, MAX_COMMAND_SIZE, 
				"/bin/rm -rf %s.difx/", filebase);
			command[MAX_COMMAND_SIZE-1] = 0;

			difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_INFO);
		
			Mk5Daemon_system(D, command, 1);
		}

		difxMessageSendDifxAlert("mpifxcorr spawning process", DIFX_ALERT_LEVEL_INFO);

		snprintf(command, MAX_COMMAND_SIZE, "su - %s -c 'ssh -x %s \"%s -np %d --bynode --hostfile %s.machines %s %s %s\"'", 
			user,
			S->headNode,
			mpiWrapper,
			1 + S->nDatastream + S->nProcess,
			filebase,
			mpiOptions,
			difxProgram,
			S->inputFilename);
		command[MAX_COMMAND_SIZE-1] = 0;

		snprintf(message, MAX_MESSAGE_SIZE, "Executing: %s", command);
		message[MAX_MESSAGE_SIZE-1] = 0;
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_INFO);

		snprintf(message, MAX_MESSAGE_SIZE, "Spawning %d processes", 
			1 + S->nDatastream + S->nProcess);
		message[MAX_MESSAGE_SIZE-1] = 0;
		difxMessageSendDifxStatus2(jobName, DIFX_STATE_SPAWNING, 
			message);

		returnValue = Mk5Daemon_system(D, command, 1);

		/* FIXME -- figure out why this doesn't work! */
	//	if(returnValue == 0)
	//	{
			difxMessageSendDifxStatus2(jobName, DIFX_STATE_MPIDONE, "");
			difxMessageSendDifxAlert("mpifxcorr process done", DIFX_ALERT_LEVEL_INFO);
	//	}
	//	else
	//	{
	//		sprintf(message, "Error code = %d", returnValue);
	//		difxMessageSendDifxStatus2(jobName, DIFX_STATE_CRASHED, message);
	//		sprintf(message, "Job %s crashed.  Error code = %d\n", jobName, returnValue);
	//		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_FATAL);
	//	}

		/* change ownership to match input file */
		snprintf(command, MAX_COMMAND_SIZE, 
			"chown --recursive --reference=%s %s.difx", 
			S->inputFilename, filebase);
		command[MAX_COMMAND_SIZE-1] = 0;
		Mk5Daemon_system(D, command, 1);

		snprintf(command, MAX_COMMAND_SIZE,
			"chmod g+w %s.difx", filebase);
		command[MAX_COMMAND_SIZE-1] = 0;
		Mk5Daemon_system(D, command, 1);

		snprintf(command, MAX_COMMAND_SIZE, 
			"chmod --reference=%s %s.difx/*", 
			S->inputFilename, filebase);
		command[MAX_COMMAND_SIZE-1] = 0;
		Mk5Daemon_system(D, command, 1);

		exit(0);
	}

	/* if we got here, we are the parent process */
	/* now spawn the difxlog process. */
	if(fork() == 0)
	{
		const char *user;

		user = getenv("DIFX_USER_ID");
		if(!user)
		{
			user = difxUser;
		}

		snprintf(command, MAX_COMMAND_SIZE,
			"su - %s -c 'ssh -x %s \"difxlog %s %s.difxlog 4 %d\"'",
			user, S->headNode, jobName, filebase, childPid);
		command[MAX_COMMAND_SIZE-1] = 0;
		Mk5Daemon_system(D, command, 1);

		/* change ownership to match input file */
		snprintf(command, MAX_COMMAND_SIZE,
			"chown --reference=%s %s.difxlog", 
			S->inputFilename, filebase);
		command[MAX_COMMAND_SIZE-1] = 0;
		Mk5Daemon_system(D, command, 1);

		snprintf(command, MAX_COMMAND_SIZE, 
			"chmod --reference=%s %s.difxlog", 
			S->inputFilename, filebase);
		command[MAX_COMMAND_SIZE-1] = 0;
		Mk5Daemon_system(D, command, 1);

		exit(0);
	}
}
