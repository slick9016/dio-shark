#ifndef DIO_SHARK_H
#define DIO_SHARK_H
/*
	dio_shark.h
	The main tracing source header file.
	
	This file declare core structures and interfaces
	in dio-shark, shark means tracing thread
*/

/* defines */
//shark status
enum shark_stat{
	SHARK_READY = 0,	//shark thread is ready
	SHARK_SICK,		//shark thread has problem
	SHARK_DONE		//shark thread done all of works
};

/* structures */
//shark's personal inventory
struct shark_inven{

};

/* function declares */
static int parse_args(int argc, char** argv);	//parsing input argument
static int get_cpustat();			//

static void loose_sharks();			//dealing all sharks(all tracing thread)
static int loose_shark();			//create shark (tracing thread)
static void shark_signal();			//for thread synchronizing
static void wait_allsharks_ready();		//will be called from outer thread
static void wait_gunfire();			//shark(tracing thread) will be waiting a start sign

#endif 
