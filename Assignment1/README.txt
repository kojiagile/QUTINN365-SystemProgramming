======================================================================
* README
*	Author: Koji
*	Update: 08/09/2014
======================================================================

This is README.txt for AirportSimulator.
This system works on Linux operating systems in C language with POSIX.
No make file is required to compile.
Make sure that linux user possesses permission of the directory to operate all the instruction below.

--- How to compile: --------------------------------------------------
	place the source file in preferable directory in a server.
	use "cd" command to move to the directory where you copied the source file.
	type "gcc -o AirportSimulator AirportSimulator.c -lpthread", and then press enter key.
	compile process will successfully finish if no error occur.
----------------------------------------------------------------------

--- How to run: ------------------------------------------------------
	type "./AirportSimulator <digitA> <digitB>", and then press enter key.
		<digitA> is the chance of the landing thread generating a plane to land.
		<digitB> is the chance of the taking off thread randomly selecting a landed plane to take off.
		a integer number between 1 to 90 for both digitA and digitB can be allowed.
		both digitA and digitB must be input.
----------------------------------------------------------------------
