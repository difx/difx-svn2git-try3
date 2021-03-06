#ifndef __DIFX_MESSAGE_H__
#define __DIFX_MESSAGE_H__

#ifdef __cplusplus
extern "C" {
#endif


/**** LOW LEVEL MULTICAST FUNCTIONS ****/

/* single function for send.  returns message length on success, or -1 */

int MulticastSend(const char *group, int port, const char *message);


/* functions for receive */

int openMultiCastSocket(const char *group, int port);
int closeMultiCastSocket(int sock);
int MultiCastReceive(int sock, char *message, int maxlen, char *from);


/**** DIFX SPECIFIC FUNCTIONS AND DATA TYPES ****/

/* Note! Keep this in sync with Mk5StateStrings[][24] in difxmessage.c */
enum Mk5State
{
	MARK5_STATE_OPENING = 0,
	MARK5_STATE_OPEN, 
	MARK5_STATE_CLOSE, 
	MARK5_STATE_GETDIR, 
	MARK5_STATE_GOTDIR, 
	MARK5_STATE_PLAY, 
	MARK5_STATE_IDLE, 
	MARK5_STATE_ERROR,
	MARK5_STATE_BUSY,
	MARK5_STATE_INITIALIZING,
	MARK5_STATE_RESETTING,
	MARK5_STATE_REBOOTING,
	MARK5_STATE_POWEROFF,
	MARK5_STATE_NODATA,
	MARK5_STATE_NOMOREDATA,
	MARK5_STATE_PLAYINVALID,
	MARK5_STATE_START,
	NUM_MARK5_STATES	/* this needs to be the last line of enum */
};

extern const char Mk5StateStrings[][24];

/* Note! Keep this in sync with DifxStateStrings[][24] in difxmessage.c */
enum DifxState
{
	DIFX_STATE_SPAWNING = 0,/* Issued by mpirun wrapper */
	DIFX_STATE_STARTING,	/* fxmanager just started */
	DIFX_STATE_RUNNING,	/* Accompanied by visibility info */
	DIFX_STATE_ENDING,	/* Normal end of job */
	DIFX_STATE_DONE,	/* Normal end to DiFX */
	DIFX_STATE_ABORTING,	/* Unplanned early end due to runtime error */
	DIFX_STATE_TERMINATING,	/* Caught SIGINT, closing down */
	DIFX_STATE_TERMINATED,	/* Finished cleaning up adter SIGINT */
	DIFX_STATE_MPIDONE,	/* mpi process has finished */
	NUM_DIFX_STATES		/* this needs to be the last line of enum */
};

extern const char DifxStateStrings[][24];

/* Note! Keep this in sync with DifxMessageTypeStrings[][24] in difxmessage.c */
enum DifxMessageType
{
	DIFX_MESSAGE_UNKNOWN = 0,
	DIFX_MESSAGE_LOAD,
	DIFX_MESSAGE_ERROR,
	DIFX_MESSAGE_MARK5STATUS,
	DIFX_MESSAGE_STATUS,
	DIFX_MESSAGE_INFO,
	DIFX_MESSAGE_DATASTREAM,
	DIFX_MESSAGE_COMMAND,
	NUM_DIFX_MESSAGE_TYPES	/* this needs to be the last line of enum */
};

extern const char DifxMessageTypeStrings[][24];

typedef struct
{
	enum Mk5State state;
	char vsnA[10];
	char vsnB[10];
	unsigned int status;
	char activeBank;
	int scanNumber;
	char scanName[64];
	long long position;	/* play pointer */
	float rate;		/* Mbps */
	double dataMJD;
} DifxMessageMk5Status;

typedef struct
{
	
} DifxMessageDatastream;

typedef struct
{
	float cpuLoad;
	int totalMemory;
	int usedMemory;
	unsigned int netRXRate;		/* Bytes per second */
	unsigned int netTXRate;		/* Bytes per second */
} DifxMessageLoad;

typedef struct
{
	char message[1000];
	int severity;
} DifxMessageError;

typedef struct
{
	enum DifxState state;
	char message[1000];
	double mjd;
} DifxMessageStatus;

typedef struct
{
	char message[1000];
} DifxMessageInfo;

typedef struct
{
	char command[1000];
} DifxMessageCommand;

typedef struct
{
	enum DifxMessageType type;
	char from[32];
	char to[32][32];
	int nTo;
	char identifier[32];
	int mpiId;
	int seqNumber;
	/* FIXME -- add time of receipt ?? */
	union
	{
		DifxMessageMk5Status	mk5status;
		DifxMessageLoad		load;
		DifxMessageError	error;
		DifxMessageStatus	status;
		DifxMessageInfo		info;
		DifxMessageDatastream	datastream;
		DifxMessageCommand	command;
	} body;
	int _xml_level;			/* internal use only */
	char _xml_element[5][32];	/* internal use only */
	int _xml_error_count;
} DifxMessageGeneric;

int difxMessageInit(int mpiId, const char *identifier);
void difxMessagePrint();
void difxMessageGetMulticastGroupPort(char *group, int *port);

int difxMessageSend(const char *message);
int difxMessageSendProcessState(const char *state);
int difxMessageSendMark5Status(const DifxMessageMk5Status *mk5status);
int difxMessageSendDifxStatus(enum DifxState state, const char *message, double visMJD, int numdatastreams, float *weight);
int difxMessageSendLoad(const DifxMessageLoad *load);
int difxMessageSendDifxError(const char *errorMessage, int severity);
int difxMessageSendDifxInfo(const char *infoMessage);

int difxMessageReceiveOpen();
int difxMessageReceiveClose(int sock);
int difxMessageReceive(int sock, char *message, int maxlen, char *from);

int difxMessageParse(DifxMessageGeneric *G, const char *message);
void difxMessageGenericPrint(const DifxMessageGeneric *G);

#ifdef __cplusplus
}
#endif


#endif
