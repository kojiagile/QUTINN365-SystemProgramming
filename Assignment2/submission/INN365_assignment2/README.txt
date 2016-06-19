======================================================================
* README
*	Author: Koji
*	Update: 21/10/2014
======================================================================

This is README.txt for Distributed Communication application.
This system works on Linux operating systems in C language.
1 make file is provided to compile.
Make sure that linux user possesses permission of the directory to operate all the instruction below.

--- How to compile: --------------------------------------------------
	place all three source files and (following) a Makefile provided in preferable directory in a server.
		Source files:
			applib.h
			distcomclient.c
			distcomserver.c
	use "cd" command to move to the directory where you copied the source files.
	type "make", and then press enter key.
	compile process will be automatically implemented.
----------------------------------------------------------------------

--- How to use: ------------------------------------------------------
	Run server program:
	type "./distcomserver <digitA>", and then press enter key.
		<digitA> is the port number that listens to client request.
	
	Run client program:
	type "./distcomclient <Server IP address> <digitA>", and then press enter key.
		<Server IP address> is the IP address that the server program is running.
		<digitA> is the server listening port.
	
----------------------------------------------------------------------
