/*
	dio_shark.c
	the main project source file
	it contains 'main' function

	dio-shark is disk block tracing and analysis tool
	it will loosing sharks to each cpu
*/

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "dio_shark.h"
#include "dst/dio_list.h"

/* define macro and structure define */

/* global variables */
static int cpucnt = 0;	//number of CPUs
static struct dl_head* sharks;	//This list's entity data type is struct shark_inven

/* function declaration */
static void sig_handler(__attribute__((__unused__)) int sig);
bool parse_args(int argc, char** argv);

void del_sharks(void* pdata);

void* shark_body(void* param);

/* main function */
int main(int argc, char** argv){
	
	//init settings
	cpucnt = sysconf(_SC_NPROCESSORS_ONLN);
	//ioctl setup

	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGPIPE, SIG_IGN);

	if( !parse_args(argc, argv) ){
		fprintf(stderr, "dio-shark argument error.\n");
		goto cancel;
	}

	sharks = create_list_head(del_sharks);
	loose_sharks();

	//check end states

	return 0;

cancel:
	//clear
	return -1;

}

void sig_handler(int sig){
	
}

void del_sharks(void* pdata){
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

	while( (tok = getopt_long(argc, argv, ARG_OPTS, arg_opts, NULL)) >= 0){
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

void loose_sharks(){
	int i;
	for(i=0; i<cpucnt; i++){
		loose_shark(i);
	}
}

bool loose_shark(int no){
	struct shark_inven* si = NULL;
	int i=0;

	si = (struct shark_inven*)malloc(sizeof(struct shark_inven));
	if( pthread_create(&(si->td), NULL, shark_body, si) ){	
		fprintf(stderr, "shark can not create his body.\n");
		return false;
	}

	//pthread_cond_wait(si->cond, 0);
	if( si->stat != SHARK_READY ){
		//oh dear..
	}

	
	//add si to 'sharks' the global shark handler
	return true;
}

void* shark_body(void* param){
	struct shark_inven* inven = (struct shark_inven*)param;

	//get cpu info
	//set pollfd
	//set outfd
	//set poll event
	inven->stat = SHARK_READY;
	//shark_signal(inven->cond);

	//wait gunfire
	//ioctl start
	while(1){

	}
	//ioctl stop
	//signal done

	return NULL;
}

