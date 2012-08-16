#ifndef DIO_SHARK_H
#define DIO_SHARK_H
/*
	dio_shark.h
	The main tracing source header file.
	
	This file declare core structures and interfaces
	in dio-shark, shark means tracing thread
*/

/* defines */
//type defines
#define bool unsigned short
#define true ~0
#define false 0

//shark status
enum shark_stat{
	SHARK_READY = 0,	//shark thread is ready
	SHARK_SICK,		//shark thread has problem
	SHARK_DONE		//shark thread done all of works
};

/* structures */
struct cpu_info{
};

//shark's personal inventory
struct shark_inven{

};

/* function declares */
extern void loose_sharks();			//dealing all sharks(all tracing thread)
extern bool loose_shark();			//create shark (tracing thread)
extern void shark_signal();			//for thread synchronizing
extern void wait_allsharks_ready();		//will be called from outer thread
extern void wait_gunfire();			//shark(tracing thread) will be waiting a start sign

#endif 
