/*
	The parser for binary result of dio-shark
	
	This source is free on GNU General Public License.
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

#include "dio_shark.h"
#include "list.h"
#include "rbtree.h"
#include "blktrace_api.h"

/*	struct and defines	*/
#define BE_TO_LE16(word) \
	(((word)>>8 & 0x00FF) | ((word)<<8 & 0xFF00))

#define BE_TO_LE32(dword) \
	(((dword)>>24 & 0x000000FF) | ((dword)<<24 & 0xFF000000) |\
	((dword)>>8  & 0x0000FF00) | ((dword)<<8  & 0x00FF0000))

#define BE_TO_LE64(qword) \
	(((qword)>>56 & 0x00000000000000FF) | ((qword)<<56 & 0xFF00000000000000) |\
	((qword)>>40 & 0x000000000000FF00) | ((qword)<<40 & 0x00FF000000000000) |\
	((qword)>>24 & 0x0000000000FF0000) | ((qword)<<24 & 0x0000FF0000000000) |\
	((qword)>>8  & 0x00000000FF000000) | ((qword)<<8  & 0x000000FF00000000))

#define BE_TO_LE_BIT(bit) \
	(bit).magic 	= BE_TO_LE32((bit).magic);\
	(bit).sequence 	= BE_TO_LE32((bit).sequence);\
	(bit).time	= BE_TO_LE64((bit).time);\
	(bit).sector	= BE_TO_LE64((bit).sector);\
	(bit).bytes	= BE_TO_LE32((bit).bytes);\
	(bit).action	= BE_TO_LE32((bit).action);\
	(bit).pid	= BE_TO_LE32((bit).pid);\
	(bit).device	= BE_TO_LE32((bit).device);\
	(bit).cpu	= BE_TO_LE32((bit).cpu);\
	(bit).error	= BE_TO_LE16((bit).error);\
	(bit).pdu_len	= BE_TO_LE16((bit).pdu_len)

#define MAX_ELEMENT_SIZE 20
struct dio_nugget{
	struct rb_node link;	//rbtree linker

	int elemidx;	//element index
	char states[MAX_ELEMENT_SIZE];	//action
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
//function for bit list
static void insert_proper_pos(struct dio_entity* pde);

//function for nugget
static void init_nugget(struct dio_nugget* pdng);
static struct dio_nugget* rb_search_nugget(uint64_t sector);
static struct dio_nugget* __rb_insert_nugget(struct dio_nugget* pdng);
static struct dio_nugget* rb_insert_nugget(struct dio_nugget* pdng);

/*	global variables	*/
#define MAX_FILEPATH_LEN 255
static char respath[MAX_FILEPATH_LEN];
static struct rb_root dng_root;

static struct list_head de_head;

/*	function implementations	*/
int main(int argc, char** argv){

	INIT_LIST_HEAD(&de_head);
	dng_root = RB_ROOT;

	int ifd = -1;
	int rdsz = 0;

	strncpy(respath, "dioshark.output", MAX_FILEPATH_LEN);

	ifd = open(respath, O_RDONLY);
	if( ifd < 0 ){
		perror("failed to open result file");
		goto err;
	}
	
	struct dio_entity* pde = NULL;
	struct dio_nugget* pdng = NULL;

	int i;
	while(1){
		pde = (struct dio_entity*)malloc(sizeof(struct dio_entity));
		if( pde == NULL ){
			perror("failed to allocate memory");
			goto err;
		}

		rdsz = read(ifd, &(pde->bit), sizeof(struct blk_io_trace));
		if( rdsz < 0 ){
			perror("failed to read");
			goto err;
		}
		else if( rdsz == 0 ){
			DBGOUT("end read\n");
			break;
		}
		
		DBGOUT("pdu_len : %d\n", pde->bit.pdu_len);
		//ignore pdu_len size
		if( pde->bit.pdu_len > 0 ){
			lseek(ifd, pde->bit.pdu_len, SEEK_CUR);
		}
		
		DBGOUT("read ok\n");
		BE_TO_LE_BIT(pde->bit);
		
		//insert to list 
		insert_proper_pos(pde);
		
		pdng = rb_search_nugget( pde->bit.sector );
		if( pdng == NULL ){
			DBGOUT(" > %llu isn't in tree\n", pde->bit.sector);
			pdng = (struct dio_nugget*)malloc(sizeof(struct dio_nugget));
			if( pdng == NULL ){
				perror("failed to allocate nugget memory");
				goto err;
			}
			init_nugget(pdng);
			pdng->sector = pde->bit.sector;
			rb_insert_nugget(pdng);	//it doesn't need to check null (already checked above)
		}

		pdng->states[pdng->elemidx++] = 'Q';	//this line will be modified to switch case
	}

	//test printing
	DBGOUT("end parse.\nprint start\n");
	struct list_head* p = NULL;
	__list_for_each(p, &(de_head)){
		struct dio_entity* _pde = list_entry(p, struct dio_entity, link);
		DBGOUT("time : %20llu, sector %20llu, action 0x%08x, pid %10u, cpu %d\n", 
			_pde->bit.time, _pde->bit.sector, _pde->bit.action, _pde->bit.pid, _pde->bit.cpu);
	}
	DBGOUT("end printing\n");

	//clean all list entities
	return 0;
err:
	if( ifd < 0 )
		close(ifd);
	if( pde != NULL )
		free(pde);
	return 0;
}

void insert_proper_pos(struct dio_entity* pde){
	struct list_head* p = NULL;
	struct dio_entity* _pde = NULL;

	//list foreach back
	for(p = de_head.prev; p != &(de_head); p = p->prev){
		_pde = list_entry(p, struct dio_entity, link);
		if( _pde->bit.time <= pde->bit.time ){
			list_add(&(pde->link), p);
			return;
		}
	}
	list_add(&(pde->link), &(de_head));
}

void init_nugget(struct dio_nugget* pdng){
	memset(pdng, 0, sizeof(struct dio_nugget));
	pdng->elemidx = 0;
}

struct dio_nugget* rb_search_nugget(uint64_t sector){
	struct rb_node* rn = dng_root.rb_node;
	struct dio_nugget* dn = NULL;

	while(rn){
		dn = rb_entry(rn, struct dio_nugget, link);
		if( sector < dn->sector )
			rn = dn->link.rb_left;
		else if( sector > dn->sector )
			rn = dn->link.rb_right;
		else
			return dn;	//find
	}
	return NULL;
}

struct dio_nugget* __rb_insert_nugget(struct dio_nugget* pdng){
	struct rb_node** p = &dng_root.rb_node;
	struct rb_node* parent = NULL;
	struct dio_nugget* dn = NULL;

	while(*p){
		parent = *p;
		dn = rb_entry(parent, struct dio_nugget, link);

		if( pdng->sector < dn->sector )
			p = &(*p)->rb_left;
		else if( pdng->sector > dn->sector )
			p = &(*p)->rb_right;
		else
			return dn;
	}

	rb_link_node(&(pdng->link), parent, p);
	return NULL;	//success
}

struct dio_nugget* rb_insert_nugget(struct dio_nugget* pdng){
	struct dio_nugget* ret;
	if( (ret = __rb_insert_nugget(pdng)))
		return ret;	//there already exist

	rb_insert_color(&(pdng->link), &(dng_root));
	return NULL;
}
