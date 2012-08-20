/*
	dio_shark.c
	the main project source file
	it contains 'main' function

	dio-shark is disk block tracing and analysis tool
	it will loosing sharks to each cpu
*/

#include <stdlib.h>		// malloc(), SIGINT, SIGHUP, SIGTERM, SIGPIPE, SIG_IGN
#include <stdio.h>		// stderr, fprintf(), printf()
#include <unistd.h>		// read()
#include <getopt.h>		// required_argument, arg_opts
#include <string.h>		// memset()
#include <fcntl.h>		// O_RDONLY, O_WRONLY
#include <sys/ioctl.h>	// ioctl()
#include <stdbool.h>	// bool, true, false
#include <pthread.h>	// pthread_mutex_t, pthread_cond_t, pthread_create(), \
							pthread_cond_wait(), pthread_mutex_lock(), \
							pthread_mutex_unlock(), pthread_cond_signal(),\
							PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER

#include "dio_shark.h"
//#include "dst/dio_list.h"

#define BUF_SIZE 	1024*512
#define BUF_NR		4

/* define macro and structure define */
#define BUTS_STAT_NONE		0
#define	BUTS_STAT_SETUPED	1
#define	BUTS_STAT_STARTED	2
#define	BUTS_STAT_STOPPED	3

/* global variables */
//static int cpucnt = 0;	//number of CPUs
//static struct dl_head* sharks;	//This list's entity data type is struct shark_inven
bool g_isdone = false;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_cond	= PTHREAD_COND_INITIALIZER;

/* function declaration */
struct list_head* create_list_head(void);

bool parse_args(int argc, char** argv);

void signalHandler(int idxSignal);
void set_signalHandler(void);
void put_signalHandler(void);

bool wait_open_debugfs(void);
void* shark_body(void* param);

bool loose_sharks(struct list_head* shark_boss, int numCPU);
struct thread_shark* loose_shark(int idxCPU);
void* wait_comeback_shark(struct list_head* shark_boss);
void fasten_sharks(struct list_head* shark_boss);

int openfile_device(char *devpath);
int openfile_debugfs(void);
int openfile_output(void);

void setup_buts(struct blk_user_trace_setup *pbuts);

/*
   main function
*/
int main(int argc, char** argv){
	int numCPU;
	int fdDevice;
	struct blk_user_trace_setup buts;
	struct list_head *shark_boss;
	struct list_head *p;
	int buts_stat;
	int ret;

	// get the number of cpus
	numCPU = sysconf(_SC_NPROCESSORS_ONLN);

	// set signal handler function
	set_signalHandler();

	// handle args
	if( !parse_args(argc, argv) ){
		fprintf(stderr, "dio-shark argument error.\n");
		goto out;
	}

	// open device file	
	fdDevice = openfile_device("/dev/sda");
	if(fdDevice < 0)
	{	
		goto out;
	}

	// setup blk_user_trace_setup
	setup_buts(&buts);

	// device controller setup
	ret = ioctl(fdDevice, BLKTRACESETUP, &buts);
	if(ret < 0)
	{
		goto out;
	}
	buts_stat = BUTS_STAT_SETUPED;

	// create list head for creating threads
	shark_boss = create_list_head();

	// create threads
	ret = loose_sharks(shark_boss, numCPU);
	if(ret == (int)false)
	{
		goto out;
	}

	// wait until open debug file
	ret = wait_open_debugfs();
	if(ret == (int)false)
	{
		goto out;
	}

	// wait until all thread terminate
	wait_comeback_shark(shark_boss);

	// fasten sharks that loosed
	fasten_sharks(shark_boss);
	
	// destroy list head
	free(shark_boss);	

	// device controller start
	ret = ioctl(fdDevice, BLKTRACESTART);
	if(ret < 0)
	{
		goto out;
	}
	buts_stat = BUTS_STAT_STARTED;

	// close device file
	close(fdDevice);

	// put signal handler
	put_signalHandler();

	return 0;

out:
	return -1;
}

struct list_head* create_list_head(void)
{
	struct list_head *head;

	head = (struct list_head*)malloc(sizeof(struct list_head));
	INIT_LIST_HEAD(head);

	return head;
}

/* start parse_args */
#define ARG_OPTS "d:o:"
static struct option arg_opts[] = {
	{
		.name = "device",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'd'
	},
	{
		.name = "outfile",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'o'
	}
};

char usage_detail[] = 	"\n"\
			"  [ -d <device> ]\n"\
			"  [ -o <outfile> ]\n"\
			"\n"\
			"\t-d : device which is traced\n"\
			"\t-o : output file name\n";

bool parse_args(int argc, char** argv){
	char tok;
	int cnt = 0;

	while( (tok = getopt_long(argc, argv, ARG_OPTS, arg_opts, NULL)) >= 0 ){
		switch(tok){
			case 'd':
				printf(" option d!\n");
				//set device info
				break;
			case 'o':
				printf(" option o!\n");
				//set output file
				break;
			default:
				printf("USAGE : %s %s\n", argv[0], usage_detail);
				return false;
				break;
		};
		cnt++;
	}

	if(cnt == 0){
		fprintf(stderr, "dio-shark has no argument.\n");
		return false;
	}

	return true;
}
/* end parse_args */

void signalHandler(int idxSignal)
{
	g_isdone = true;
}
void set_signalHandler(void)
{
	signal(SIGINT, signalHandler);
	signal(SIGHUP, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGPIPE, SIG_IGN);
}
void put_signalHandler(void)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
}

/*
   Install Threads to get i/o data. 
*/
bool loose_sharks(struct list_head* shark_boss, int numCPU){
	struct thread_shark *tmpShark;
	int i;

	// install threads
	for(i=0 ; i<numCPU ; i++)
	{
		tmpShark = loose_shark(i);
		if(tmpShark == NULL)
			return false;
		list_add_tail(shark_boss, &(tmpShark->list));
	}

	return true;
}
struct thread_shark* loose_shark(int idxCPU)
{
	struct thread_shark *shark = NULL;
	int ret;

	shark = (struct thread_shark*)malloc(sizeof(struct thread_shark));
	ret = pthread_create(&(shark->td), NULL, shark_body, shark);
	if(ret)
	{
		fprintf(stderr, "dio-shark can not create thread.\n");
		
		goto ERROR;
	}

	return shark;

ERROR:
	// release tshark memory
	if(shark != NULL)
		free(shark);
	
	return NULL;
}
void* wait_comeback_shark(struct list_head* shark_boss)
{
	void* tReturn;
	struct list_head* p;

	__list_for_each(p, shark_boss)
	{
		struct thread_shark *tmpShark;
		tmpShark = list_entry(p, struct thread_shark, td);
		pthread_join(tmpShark->td, tReturn);
	}
}
void fasten_sharks(struct list_head* shark_boss)
{
	struct list_head* p;
	
	list_for_each_prev(p, shark_boss)
	{
		struct thread_shark *tmpShark;
		tmpShark = list_entry(p, struct thread_shark, td);
		list_del(p);
		free(tmpShark);
	}
}
bool wait_open_debugfs(void)
{
	int ret;

	// thread mutex lock
	ret = pthread_mutex_lock(&g_mutex);
	if(ret != 0)
	{
		return false;
	}

	while(!g_isdone)	//wait until program done
		pthread_cond_wait(&g_cond, &g_mutex);	// wait until open

	// thread mutex unlock
	pthread_mutex_unlock(&g_mutex);
}
void* shark_body(void* param){
	int buts_stat = BUTS_STAT_NONE;
	/*
	   	BUTS_STAT_NONE 	  : 0
		BUTS_STAT_SETUPED : 1
		BUTS_STAT_STARTED : 2
		BUTS_STAT_STOPPED : 3
	*/
	struct blk_user_trace_setup buts;
	int fdDebugfs, fdOutput;
	char buf[BUF_SIZE];
	int lenred;

	// open debug file
	fdDebugfs = openfile_debugfs();
	if(fdDebugfs < 0)
		goto out;

	// wake thread that wait opening debug file
	pthread_cond_signal(&g_cond);

	// open output file
	fdOutput = openfile_output();
	if(fdOutput < 0)
		goto out;

	// get i/o data
	while(!g_isdone)
	{
		lenred = read(fdDebugfs, buf, sizeof(buf));
		if(lenred < 0)
			goto out;

		write(fdOutput, buf, lenred);
	}

out:
	/* Move to main function
	// stop ioctl
	if(!(pfd.fd < 0) && buts_stat != BUTS_STAT_NONE)
	{
		ioctl(pfd.fd, DIOSHARKSTOP);
		buts_stat = BUTS_STAT_STOPPED;
	}
	*/

	// close output file
	if(!(fdOutput < 0))
		close(fdOutput);

	// close debugfs file
	if(!(fdDebugfs < 0))
		close(fdDebugfs);
	
	/* Move to main function
	// close poll file
	if(!(pfd.fd < 0))
		close(pfd.fd);
	*/

	return NULL;
}

int openfile_device(char *devpath){
	int fdDevice;

	fdDevice = open(devpath, O_RDONLY);
	if (fdDevice < 0)
		return -1;

	return fdDevice;
}
int openfile_debugfs(void)
{
	int fdDebugfs;

	fdDebugfs = open("/sys/kernel/debug/block/sda.trace.o", O_RDONLY);
	if (fdDebugfs < 0)
		return -1;

	return fdDebugfs;
}
int openfile_output(void)
{	int fdOutput;

	fdOutput = open("./output.sda", O_WRONLY);
	if (fdOutput <0)
		return -1;

	return fdOutput;
}

void setup_buts(struct blk_user_trace_setup *pbuts)
{
	memset(pbuts, 0, sizeof(*pbuts));
	pbuts->buf_size	= BUF_SIZE;
	pbuts->buf_nr 	= BUF_NR;
	pbuts->act_mask = 0xffff;
}
