/*
	The parser for binary result of dio-shark
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "list.h"
#include "blktrace_api.h"

/*	struct and defines	*/
#define MAX_ELEMENT_SIZE 10
struct dio_nugget{
	char states[MAX_ELEMENT_SIZE];
	uint64_t times[MAX_ELEMENT_SIZE];
	char type[5];
	uint64_t sector;
};

// list node of blk_io_trace
struct dio_entity{
	struct list_head link;
	
	struct blk_io_trace bit;
};

/*	function interfaces	*/
static void insert_proper_pos(struct dio_entity* pde);

/*	global variables	*/
#define MAX_FILEPATH_LEN 255
static char respath[MAX_FILEPATH_LEN];

static struct list_head dio_head;

/*	function implementations	*/
int main(int argc, char** argv){

	INIT_LIST_HEAD(&dio_head);

	int ifd = -1;
	int rdsz = 0;

	strncpy(respath, "dioshark.output", MAX_FILEPATH_LEN);

	ifd = open(respath, O_RDONLY);
	if( ifd < 0 ){
		perror("failed to open result file");
		goto err;
	}
	
	struct dio_entity* pde = NULL;

	struct dio_nugget dnugget;	//just for test
	memset(&dnugget, 0, sizeof(struct dio_nugget));
	dnugget.sector = -1;
	int tmp=0;

	char c;
	int i;
	while(1){
		pde = (struct dio_entity*)malloc(sizeof(struct dio_entity));

		rdsz = read(ifd, &(pde->bit), sizeof(struct blk_io_trace));
		if( rdsz < 0 ){
			perror("failed to read");
			goto err;
		}
		else if( rdsz == 0 ){
			printf("read zero size\n");
			free(pde);
			break;
		}
		
		printf("pdu_len : %d\n", pde->bit.pdu_len);
		//ignore pdu_len size
		if( pde->bit.pdu_len > 0 ){
			lseek(ifd, pde->bit.pdu_len, SEEK_CUR);
		}
		
		printf("read ok\n");
		insert_proper_pos(pde);
		printf("! time %ld, pid %d\n", pde->bit.time, pde->bit.pid);
		if( dnugget.sector == -1){
			printf("nugget sector : %d\n", dnugget.sector);
			dnugget.sector = pde->bit.sector;
		}else if(dnugget.sector == pde->bit.sector ){
			dnugget.times[tmp++] = pde->bit.time;
			printf("same sector!\n");
		}
	}

	printf("end spliting.\ngo printing\n");
	struct list_head* p = NULL;
	__list_for_each(p, &(dio_head)){
		struct dio_entity* _pde = list_entry(p, struct dio_entity, link);
		printf("time : %ld, sector %ld, action 0x%x, pid %d, cpu %d\n", 
			_pde->bit.time, _pde->bit.sector, _pde->bit.action, _pde->bit.pid, _pde->bit.cpu);
	}
	printf("end printing\n");

	//clean all list entities
	return 0;
err:
	if( ifd < 0 )
		close(ifd);
	return 0;
}

void insert_proper_pos(struct dio_entity* pde){
	struct list_head* p = NULL;
	struct dio_entity* _pde = NULL;

	//list foreach back
	for(p = dio_head.prev; p != &(dio_head); p = p->prev){
		_pde = list_entry(p, struct dio_entity, link);
		if( _pde->bit.time <= pde->bit.time ){
			list_add(&(pde->link), p);
			return;
		}
	}
	list_add(&(pde->link), &(dio_head));
}
