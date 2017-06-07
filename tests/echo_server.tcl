package require Tclx
package require socketserver

::socketserver::socket server 8888

set done 0

proc do_client {} {
	puts "child process started"
    # forked child process
	# block until the parent process receives connection
	# and passes the fd
	puts "channel names are before [chan names]"
	set fd [::socketserver::socket client foobar]
	puts "channel names are now [chan names]"
	puts "received fd=$fd"
	fconfigure $fd -encoding utf-8 -buffering line -blocking 1 -translation lf
	while {1} {
	    set line [gets $fd]
	    if {[string first "quit" $line] != -1} {
		    exit
	    }
	    puts $fd "[pid] $line"
	}
	close $fd
}

proc make_worker {} {
  set pid [fork]
  if {$pid == 0} {
	do_client
  }
}

signal trap SIGUSR1 [list make_worker]

make_worker
make_worker

vwait done
