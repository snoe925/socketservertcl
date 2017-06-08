socketserver, a Tcl interface to libancillary for socket serving by socket passing and SCM_RIGHTS
===

Socketserver provides a Tcl command for creating a socketserver.  A socketserver is a process
which passes accepted TCP connections to a child process over a socket pair.  The socket FD can be passed to
a child process using sendmsg and SCM_RIGHTS.  This is internally implemented using libancillary for the 
file descriptor passing.

A ::socketserver::socket server ?port? opens the listening and accepting TCP socket.  You will do this once before forking children.

A ::socketserver::socket client ?handlerProc? is called in the client once for each socket.
This allows the forked process to process single connects serially.
Multiple forked processes can then handle many connections in parallel.
Each forked child process can serially handle requests to a single connection.

Instead of connections queueing in the TCP backlog of the servers socket they are queue in a socketpair between the server and all child processes of the server.
Therefore, clients to the TCP port are connected and accepted immediately, but they will not receive any data until
one child process takes the socket with recvmsg() from the socketpair.

All of the child processes inherit the file descriptor for the read side of the socket pair.
Therefore, the accepted() file descriptors are processed on a first-come, first-serve by the child processes.
It is not predicatable which child process will receive the accepted file descriptor.

In Tcl program tests/echo_server.tcl implements an example server.  Telnet to port 8888 and your input will be echoed with the
process id of the child process servicing the accepted socket.
If you signal the parent server process with SIGUSR1, more child processes are forked.

Outline of the Tcl Code
===
```
# Create the listening socket and a background C thread to accept connections
::socketserver::socket server 8888

# Fork some children, they will inherit the socketpair created in ::socketserver::socket
# ... in the forked child
proc handle_socket {sock} {
    # ... communicate over the socket
    set line [gets $sock]
    puts $sock $line
    close $sock
    # now we are ready to handle another
    ::socketserver::socket client handle_socket
}

# ... fork child process
if {[fork] == 0} {
  ::socketserver::socket client handle_socket
}

vwait done
```

Building
===
This could use git submodules but that is not working completely for me.

```
cd libancillary
git clone https://github.com/mhaberler/libancillary
# Do not make, we include the source
```
To build do a standard Tcl extension build.
```
autoreconf
./configure
make
make install
```
