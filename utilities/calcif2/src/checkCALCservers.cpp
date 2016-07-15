#include <stdio.h>
#include <stdlib.h>

#include "DelayHandlerBase.h"
#include "DelayHandlerCalcServer9_1.h"
#include "DelayHandlerCalcServer9_1_RA.h"
#include "DelayHandlerDiFXDelayServer.h"
#include "difxio/difx_input.h"


int_fast32_t test_DelayHandler(DiFX::Delay::Handler::DelayHandlerBase& handler)
{
	int_fast32_t retval;
	retval = handler.test_delay_service(11);
	printf("\n\n\n");
	if((retval))
	{
		printf("Handler failure %"PRIdFAST32" while testing delays\n", retval);
		return retval;
	}
	printf("Handler success while testing delays\n\n\n");
	retval = handler.test_parameter_service(11);
	printf("\n\n\n");
	if((retval))
	{
		printf("Handler failure %"PRIdFAST32" while testing parameters\n", retval);
		return retval;
	}
	printf("Handler success while testing parameters\n\n\n");
	return 0;
}



int main(int argc, char* argv[])
{
	if((argc < 1) || (argc > 2))
	{
		fprintf(stderr, "Error: correct usage is: %s [NUM_THREADS]\n", argv[0]);
		return EXIT_FAILURE;
	}
	int_fast32_t NUM_THREADS = 4;
	if(argc == 2)
	{
		NUM_THREADS = int_fast32_t(atoi(argv[1]));
	}
	
	int_fast32_t CalcServer9_1_working = 0;
	int_fast32_t CalcServer9_1_RA_working = 0;
	int_fast32_t DiFXDelayServerCalcServer9_1_working = 0;
	int_fast32_t DiFXDelayServerCalcServer9_1_RA_working = 0;
	int_fast32_t failures=0;
	int_fast32_t retval;
	{
		printf("\n\n\nTesting CalcServer9_1\n\n\n");
		DiFX::Delay::Handler::DelayHandlerCalcServer9_1 handler(NUM_THREADS,10);
		retval = test_DelayHandler(handler);
		if((retval))
		{
			fprintf(stderr, "\n\n\nError: CalcServer9_1 test returned %"PRIdFAST32"\n\n\n", retval);
			++failures;
		}
		else {
			printf("\n\n\nCalcServer9_1 WORKING\n\n\n");
			CalcServer9_1_working = 1;
		}
		fflush(stdout);
		fflush(stderr);
	}
	{
		printf("\n\n\nTesting CalcServer9_1_RA\n\n\n");
		DiFX::Delay::Handler::DelayHandlerCalcServer9_1_RA handler(NUM_THREADS,10);
		retval = test_DelayHandler(handler);
		if((retval))
		{
			fprintf(stderr, "\n\n\nError: CalcServer9_1_RA test returned %"PRIdFAST32"\n\n\n", retval);
			++failures;
		}
		else {
			printf("\n\n\nCalcServer9_1_RA WORKING\n\n\n");
			CalcServer9_1_RA_working = 1;
		}
		fflush(stdout);
		fflush(stderr);
	}
	{
		printf("\n\n\nTesting DiFXDelayServerCalcServer9_1\n\n\n");
		const char* const host = getenv("DIFX_DELAY_SERVER");
		if(host != NULL)
		{
			DiFX::Delay::Handler::DelayHandlerDiFXDelayServer handler(NUM_THREADS,10, host, delayServerTypeIds[CALCServer]);
			retval = test_DelayHandler(handler);
			if((retval))
			{
				fprintf(stderr, "\n\n\nError: DiFXDelayServerCalcServer9_1 test returned %"PRIdFAST32"\n\n\n", retval);
				++failures;
			}
			else {
				printf("\n\n\nDiFXDelayServerCalcServer9_1 WORKING\n\n\n");
				DiFXDelayServerCalcServer9_1_working = 1;
			}
		}
		else
		{
			fprintf(stderr, "\n\n\nError: DiFXDelayServerCalcServer9_1 test not performed because environment variable DIFX_DELAY_SERVER not set\n\n\n");
			++failures;
		}
		fflush(stdout);
		fflush(stderr);
	}
	{
		printf("\n\n\nTesting DiFXDelayServerCalcServer9_1_RA\n\n\n");
		const char* const host = getenv("DIFX_DELAY_SERVER");
		if(host != NULL)
		{
			DiFX::Delay::Handler::DelayHandlerDiFXDelayServer handler(NUM_THREADS,10, host, delayServerTypeIds[CALC_9_1_RA_Server]);
			retval = test_DelayHandler(handler);
			if((retval))
			{
				fprintf(stderr, "\n\n\nError: DiFXDelayServerCalcServer9_1_RA test returned %"PRIdFAST32"\n\n\n", retval);
				++failures;
			}
			else {
				printf("\n\n\nDiFXDelayServerCalcServer9_1_RA WORKING\n\n\n");
				DiFXDelayServerCalcServer9_1_RA_working = 1;
			}
		}
		else
		{
			fprintf(stderr, "\n\n\nError: DiFXDelayServerCalcServer9_1_RA test not performed because environment variable DIFX_DELAY_SERVER not set\n\n\n");
			++failures;
		}
		fflush(stdout);
		fflush(stderr);
	}
	// Report
	printf("\n\n\nStatus:\n");
	printf("    CalcServer9_1       (CalcServer_pipe)        : %s\n", (CalcServer9_1_working) ? "WORKING" : "BROKEN");
	printf("    CalcServer9_1_RA    (CALC_9_1_RA_Server_pipe): %s\n", (CalcServer9_1_RA_working) ? "WORKING" : "BROKEN");
	printf("    CalcServer9_1       (DiFX_Delay_Server)      : %s\n", (DiFXDelayServerCalcServer9_1_working) ? "WORKING" : "BROKEN");
	printf("    CalcServer9_1_RA    (DiFX_Delay_Server)      : %s\n", (DiFXDelayServerCalcServer9_1_RA_working) ? "WORKING" : "BROKEN");
	printf("\n\n");
	if(failures == 0)
	{
		printf("All pipe delay servers are working properly.\n");
	}
	else {
		fprintf(stderr, "Error: check pipe delay servers --- found %"PRIdFAST32" failures.\n", failures);
		return EXIT_FAILURE;
	}
	
	return 0;
}


	
