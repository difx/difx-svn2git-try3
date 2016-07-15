/***************************************************************************
 *   Copyright (C) 2015 by James M Anderson                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: fitsCT.c 6619 2015-04-22 14:26:47Z JamesAnderson $
// $HeadURL: https://svn.atnf.csiro.au/difx/applications/difx2fits/branches/radioastron20140201/src/fitsCT.c $
// $LastChangedRevision: 6619 $
// $Author: JamesAnderson $
// $LastChangedDate: 2015-04-22 16:26:47 +0200 (Wed, 22 Apr 2015) $
//
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include "DiFX_Delay_Server.h"
#include "DiFX_Delay_Server_Params.h"
#include "CALCServer.h"
#include "CALC_9_1_RA_Server.h"     /* RPCGEN creates this from CALC_9_1_RA_Server.x */










/******************************************************************************/
int
process_DiFX_Delay_Server_Parameters_1(FILE* flog, const unsigned int count, struct svc_req *pRequest, SVCXPRT *pTransport)
{
    static struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg argument;/* RPC caller passes this */
    static struct getDIFX_DELAY_SERVER_PARAMETERS_1_res result;  /* we return this to RPC caller */
    struct DIFX_DELAY_SERVER_PARAMETERS_1_res *res;
    int my_error_code = 0;
    int handler_error_code = 0;

    /* get RPC input argument */
    memset(&argument, 0, sizeof(argument));    /* see "The Art of DA" p193 */
    if (!svc_getargs (pTransport, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_arg, (caddr_t) &argument))
	{
        fprintf(flog, "Decode failure for getDIFX_DELAY_SERVER_PARAMETERS_1_arg --- returning\n");
        svcerr_decode (pTransport);
        return -1;
	}

    if(argument.verbosity >= 2) {
        fprintf(flog, "Processing RPC message %u: GETDIFX_DELAY_SERVER_PARAMETERS\n", count);
        fprintf(flog, "request arg: request_id=0x%lX delay_server=0x%lX\n", argument.request_id, argument.delay_server);
        fprintf(flog, "request arg: server_struct_setup_code=0x%lX verbosity=%d\n", argument.server_struct_setup_code, argument.verbosity);
    }
    
    /* make sure the response area is clean */
    res = &result.getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.response;
    memset(res, 0, sizeof(result.getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.response));

    switch(argument.delay_server) {
    case CALCPROG:
        handler_error_code = process_DiFX_Delay_Server_Parameters_1_CALCPROG(&argument, res);
        break;
    case CALC_9_1_RAPROG:
        handler_error_code = process_DiFX_Delay_Server_Parameters_1_CALC_9_1_RAPROG(&argument, res);
        break;
    case DIFX_DELAY_SERVER_PROG:
        fprintf(flog, "Recursive calling of this delay server is not allowed\n");
        my_error_code = 1;
        break;
    default:
        fprintf(flog, "Unknown delay_server code 0x%lX\n", argument.delay_server);
        my_error_code = 2;
    }
    if((handler_error_code))
    {
        fprintf(flog, "Error code %d returned by handler\n", handler_error_code);
        my_error_code = 3;
    }
    res->delay_server_error = my_error_code;
    res->delay_server = argument.delay_server;
    if((!handler_error_code) && (argument.verbosity >= 2))
    {
        fprintf(flog, "Results\n");
        fprintf(flog, "request res: delay_server_error=%d server_error=%d model_error=%d\n", res->delay_server_error, res->server_error, res->model_error);
        fprintf(flog, "request res: request_id=%ld delay_server=0x%lX\n", res->request_id, res->delay_server);
        fprintf(flog, "request res: server_struct_setup_code=0x%lX server_version=0x%lX\n", res->server_struct_setup_code, res->server_version);
        fprintf(flog, "request res: accelgrv =%25.16E\n", res->accelgrv);
        fprintf(flog, "request res: e_flat   =%25.16E\n", res->e_flat);
        fprintf(flog, "request res: earthrad =%25.16E\n", res->earthrad);
        fprintf(flog, "request res: mmsems   =%25.16E\n", res->mmsems);
        fprintf(flog, "request res: ephepoc  =%25.16E\n", res->ephepoc);
        fprintf(flog, "request res: gauss    =%25.16E\n", res->gauss);
        fprintf(flog, "request res: u_grv_cn =%25.16E\n", res->u_grv_cn);
        fprintf(flog, "request res: gmsun    =%25.16E\n", res->gmsun);
        fprintf(flog, "request res: gmmercury=%25.16E\n", res->gmmercury);
        fprintf(flog, "request res: gmvenus  =%25.16E\n", res->gmvenus);
        fprintf(flog, "request res: gmearth  =%25.16E\n", res->gmearth);
        fprintf(flog, "request res: gmmoon   =%25.16E\n", res->gmmoon);
        fprintf(flog, "request res: gmmars   =%25.16E\n", res->gmmars);
        fprintf(flog, "request res: gmjupiter=%25.16E\n", res->gmjupiter);
        fprintf(flog, "request res: gmsaturn =%25.16E\n", res->gmsaturn);
        fprintf(flog, "request res: gmuranus =%25.16E\n", res->gmuranus);
        fprintf(flog, "request res: gmneptune=%25.16E\n", res->gmneptune);
        fprintf(flog, "request res: etidelag =%25.16E\n", res->etidelag);
        fprintf(flog, "request res: love_h   =%25.16E\n", res->love_h);
        fprintf(flog, "request res: love_l   =%25.16E\n", res->love_l);
        fprintf(flog, "request res: pre_data =%25.16E\n", res->pre_data);
        fprintf(flog, "request res: rel_data =%25.16E\n", res->rel_data);
        fprintf(flog, "request res: tidalut1 =%25.16E\n", res->tidalut1);
        fprintf(flog, "request res: au       =%25.16E\n", res->au);
        fprintf(flog, "request res: tsecau   =%25.16E\n", res->tsecau);
        fprintf(flog, "request res: vlight   =%25.16E\n", res->vlight);
    }
    if(((handler_error_code)) || ((my_error_code)))
    {
        result.this_error = 1;
    }
    else {
        result.this_error = 0;
    }
            

    /* return result to client */

    if (!svc_sendreply (pTransport, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_res, (char *)&result))
    {
        fprintf(flog, "Cound not send getDIFX_DELAY_SERVER_PARAMETERS_1_res results back\n");
        svcerr_systemerr (pTransport);
    }

    /* free any memory allocated by xdr routines when argument was decoded */
    if (!svc_freeargs (pTransport, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_arg, (char *)&argument))
	{
        fprintf(flog, "Cound not free getDIFX_DELAY_SERVER_PARAMETERS_1_arg memory\n");
        exit(EXIT_FAILURE);
	}
    return my_error_code;
}
