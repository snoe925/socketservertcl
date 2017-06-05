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

void * socketserver_thread(void *args)
{
    int sock = *(int *)args;
    int socket_desc , client_sock , c;
    struct sockaddr_in server , client;

    // create tcp socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8888 );
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("bind failed");
        return (void *)1;
    }
    puts("bind done");
    
    //Listen
    listen(socket_desc , SOMAXCONN);
    
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    
    while (1) {
        //accept connection from an incoming client
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0 && errno != EINTR)
        {
            // EINTR are ok in accept calls, retry
            perror("accept failed");
        }
        else if (client_sock != -1)
        {
            puts("Connection accepted");
            if (ancil_send_fd(sock, client_sock)) {
                perror("ancil_send_fd");
            } else {
                printf("Sent fd.\n");
            }
            close(client_sock);
        }
    }
    return (void *)0;
}


int socketserver_server_makepipe(int *in, int *out)
{
    pthread_t tid;
    int sock[2];
    
    if(socketpair(PF_UNIX, SOCK_STREAM, 0, sock)) {
        perror("socketpair");
        return -1;
    } else {
        printf("Established socket pair: (%d, %d)\n", sock[0], sock[1]);
    }
    *in = sock[0];
    *out = sock[1];
    
    return 0;
}

int socketserver_server_subcommand(int * sock)
{
    pthread_t tid;

    if (pthread_create(&tid, NULL, socketserver_thread, sock) != 0) {
        perror("Failed to create thread to read socket");
        return 1;
    }
    pthread_detach(tid);
    
    return 0;
}

int socketserver_client_subcommand(int sock)
{
    ssize_t read_size;
    char client_message[2000];
    int fd;

    if(ancil_recv_fd(sock, &fd)) {
        perror("ancil_recv_fd");
        exit(1);
    } else {
        printf("%d Received fd: %d\n", getpid(), fd);
    }
    
    memset(client_message, 0, sizeof(client_message));
    //Receive a message from client
    while( (read_size = recv(fd , client_message , 2000 , 0)) > 0 )
    {
        //Send the message back to client
        int len = strlen(client_message);
        int write_size = write(fd , client_message , len);
        if (read_size != write_size) {
            perror("write failed to echo string");
        }
        memset(client_message, 0, sizeof(client_message));
    }
    
    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }
    
    close(fd);
    exit(0);
}

#if 0
int
socketserverObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	int optIndex;

    static CONST char *options[] = {
        "-server",
        "-client",
        NULL
    };

    enum options {
		OPT_SERVER,
		OPT_CLIENT
    };

    // basic command line processing
    if (objc < 2) {
        Tcl_WrongNumArgs (interp, 1, objv, "subcommand ?args?");
        return TCL_ERROR;
    }

    // argument must be one of the subOptions defined above
    if (Tcl_GetIndexFromObj (interp, objv[1], options, "option",
        TCL_EXACT, &optIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    switch ((enum options) optIndex) {
		case OPT_SERVER:
			return socketserver_server_subcommand(interp, objc, objv);

		case OPT_CLIENT:
			return socketserver_client_subcommand(interp, objc, objv);
	}

    return TCL_OK;
}
#else
int main(int argc, char **argv)
{
    int in;
    int out;
    
    if (socketserver_server_makepipe(&in, &out) != 0) {
        perror("Failed to create socketpipe");
        return 1;
    }

    switch(fork()) {
        case 0:
            close(in);
            socketserver_client_subcommand(out);
            break;
        case -1:
            perror("fork");
            exit(1);
        default:
            close(out);
            socketserver_server_subcommand(&in);
            wait(NULL);
            break;
    }
    return(0);
}
#endif

/* vim: set ts=4 sw=4 sts=4 noet : */
