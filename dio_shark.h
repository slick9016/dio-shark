/*
	dio_shark.h
	The main tracing source header file.
	
	This file declare core structures and interfaces
	in dio-shark, shark means tracing thread
*/

#ifndef DIO_SHARK_H
#define DIO_SHARK_H

#include <pthread.h>	// pthread_t
#include <stdint.h>		// uint16_t
#include <stdbool.h>	// bool
//#include <linux/list.h>
#include "list.h"

/* ioctl() request defines */
#define BLKTRACESETUP	_IOWR(0x12,115,struct blk_user_trace_setup)
#define BLKTRACESTART	_IO(0x12,116)
#define BLKTRACESTOP	_IO(0x12,117)
#define BLKTRACEDOWN	_IO(0x12,118)

/* thread info */
struct thread_shark{
	struct list_head list;
	pthread_t td;
	bool isOpenDebugfs;
	int idxCPU;
};

/*
 * User setup structure passed with BLKSTARTTRACE
 */
struct blk_user_trace_setup {
	char name[32];			/* output */
	uint16_t act_mask;		/* input */
	uint32_t buf_size;		/* input */
	uint32_t buf_nr;		/* input */
	uint64_t start_lba;
	uint64_t end_lba;
	uint32_t pid;
};

#endif 
