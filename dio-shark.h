#ifndef DIO_SHARK_H
#define DIO_SHARK_H
/*
	dio_shark.h
	The main tracing source header file.
	
	This file declare core structures and interfaces
	in dio-shark, shark means tracing thread
*/

#include <pthread.h>
#include <stdbool.h>
/* defines */

#define BLKTRACESETUP _IOWR(0x12,115,struct user_setup)
#define BLKTRACESTART _IO(0x12,116)
#define BLKTRACESTOP _IO(0x12,117)
#define BLKTRACETEARDOWN _IO(0x12,118)

//shark status
enum shark_stat{
	SHARK_READY = 0,	//shark thread is ready
	SHARK_WORKING,
	SHARK_SICK,		//shark thread has problem
	SHARK_DONE		//shark thread done all of works
};

/* structures */
struct cpu_info{
};

/* User Setup Structure */
struct user_setup {
	char name[32];
	__u16 act_mask;                 /* input */
	__u32 buf_size;                 /* input */
	__u32 buf_nr;                   /* input */
	__u64 start_lba;
	__u64 end_lba;
	__u32 pid;
};

struct devpath {
	char *path;
	char *buts_name;
	int fd, ncpus;
};

struct tracer {
	struct pollfd *pfds;
	pthread_t thread;
	int cpu;
	char debugFilePath[256];
	char outputFilePath[256];
	int dfp, ofp;

};


//shark's personal inventory
struct shark_inven{
	pthread_t td;
	pthread_cond_t cond;
	struct dl_head* list;
	struct cpu_info cpuinfo;
	enum shark_stat stat;
};

/* function declares */
extern void loose_sharks();			//dealing all sharks(all tracing thread)
extern bool loose_shark(int no);		//create shark (tracing thread)
extern void shark_signal(pthread_cond_t cond);			//for thread synchronizing
extern void wait_allsharks_ready();		//will be called from outer thread
extern void wait_gunfire();			//shark(tracing thread) will be waiting a start sign

#endif 
