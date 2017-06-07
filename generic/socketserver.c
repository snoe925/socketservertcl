/* -*- mode: c; tab-width: 4; indent-tabs-mode: t -*- */

/*
 * socketserver - Tcl interface to libancillary to create a socketserver
 *
 * Copyright (C) 2017 FlightAware LLC
 *
 * freely redistributable under the Berkeley license
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#define SPARE_SEND_FDS 1
#define SPARE_RECV_FDS 1

#include "../libancillary/ancillary.h"
#include "../libancillary/fd_recv.c"
#include "../libancillary/fd_send.c"

#include "socketserver.h"

/*
 *----------------------------------------------------------------------
 *
 * sockerserverObjCmd --
 *
 *      socketserver command
 *
 * Results:
 *      A standard Tcl result.
 *
 *
 *----------------------------------------------------------------------
 */

static char msgbuf[512];

void * socketserver_thread(void *args)
{
    int sock = *(int *)args;
    int socket_desc , client_sock;
    struct sockaddr_in server , client;
    int c = sizeof(struct sockaddr_in);

    // create tcp socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        strcpy(msgbuf, "Could not create socket");
    }
    strcpy(msgbuf, "Socket created");
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8888 );
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        strcpy(msgbuf, "bind failed");
        return (void *)1;
    }
    strcpy(msgbuf, "bind done");
    
    listen(socket_desc , SOMAXCONN);
    
    strcpy(msgbuf, "Waiting for incoming connections...");
    
    while (1) {
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0 && errno != EINTR)
        {
            // EINTR are ok in accept calls, retry
            strcpy(msgbuf, "accept failed");
        }
        else if (client_sock != -1)
        {
            strcpy(msgbuf, "Connection accepted");
            if (ancil_send_fd(sock, client_sock)) {
                strcpy(msgbuf, "ancil_send_fd failed");
            } else {
                strcpy(msgbuf, "Sent fd.\n");
            }
            close(client_sock);
        }
    }
    return (void *)0;
}

int
socketserverObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    socketserver_objectClientData *data = (socketserver_objectClientData *)clientData;
    int optIndex;
    pthread_t tid;
    char channel_name[80];

    enum options {
	OPT_CLIENT,
        OPT_SERVER
    };
    static CONST char *options[] = { "client", "server" };

    // basic command line processing
    if (objc != 3) {
        Tcl_WrongNumArgs (interp, 1, objv, "server ?port? | client ?handler?");
        return TCL_ERROR;
    }

    // argument must be one of the subOptions defined above
    if (Tcl_GetIndexFromObj (interp, objv[1], options, "option",
        TCL_EXACT, &optIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    if (data->object_magic != SOCKETSERVER_OBJECT_MAGIC) {
       	Tcl_AddErrorInfo(interp, "Incorrect magic value on internal state");
       	return TCL_ERROR;
    }

    switch ((enum options) optIndex) {
	case OPT_SERVER:

		/* If we do not have a socket pair create it */
	    	if (data->in == -1) {
        		int sock[2];
        		if (socketpair(PF_UNIX, SOCK_STREAM, 0, sock)) {
            			Tcl_AddErrorInfo(interp, "Failed to create thread to read socketpipe");
            			return TCL_ERROR;
        		}
	        	data->in = sock[0];
       		 	data->out = sock[1];
    		}
    
    		if (pthread_create(&tid, NULL, socketserver_thread, &data->in) != 0) {
		        Tcl_AddErrorInfo(interp, "Failed to create thread to read socketpipe");
		        return TCL_ERROR;
    		}
    		pthread_detach(tid);
	break;

	case OPT_CLIENT:
	{
		int fd = -1;
		if (ancil_recv_fd(data->out, &fd) || fd == -1) {
			Tcl_AddErrorInfo(interp, "Failed in ancil_recv_fd");
			return TCL_ERROR;
		}
                Tcl_Channel channel = Tcl_MakeFileChannel((void *)fd, TCL_READABLE|TCL_WRITABLE);
                Tcl_RegisterChannel(interp, channel);
		const char *channel_name = Tcl_GetChannelName(channel);
                Tcl_SetObjResult(interp, Tcl_NewStringObj(channel_name, strlen(channel_name)));
	}
	break;

        default:
		Tcl_AddErrorInfo(interp, "Unexpected command option");
		return TCL_ERROR;    
	}

    return TCL_OK;
}

/* vim: set ts=4 sw=4 sts=4 noet : */
