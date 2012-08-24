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

/*--------------	struct and defines	------------------*/
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


// dio_rbentity used for handling nuggets as sector order
struct dio_rbentity{
	struct rb_node rblink;		//red black tree link
	struct list_head nghead;	//head of nugget list
	uint64_t sector;
};

// dio_nugget is a treated data of bit
// it will be linked at dio_rbentity 's nghead
#define MAX_ELEMENT_SIZE 20
#define NG_BACKMERGE 1
#define NG_FRONTMERGE 2
#define NG_COMPLETE 4
struct dio_nugget{
	struct list_head nglink;	//link of dio_nugget datatype

	//real nugget data
	int elemidx;	//element index. (elemidx-1) is count of nugget states
	char states[MAX_ELEMENT_SIZE];	//action
	uint64_t times[MAX_ELEMENT_SIZE];	//states[elemidx] is occured at times[elemidx]
	char type[5];	//type of bit who was requested
	uint64_t sector;	//sector number of bit who was requested. is it really need?
	struct dio_nugget* mlink;	//if it was merged, than mlink points the other nugget as 'mflag'
	int mflag;	//BACKMERGE, FRONTMERGE
	bool isend;	//if action is 'M' or 'C', than that nugget is ended
};

// list node of blk_io_trace
// it just maintain the time ordered bits
struct bit_entity{
	struct list_head link;
	
	struct blk_io_trace bit;
};


/*--------------	function interfaces	-----------------------*/
/* function for bit list */
// insert bit_entity data into rbiten_head order by time
static void insert_proper_pos(struct bit_entity* pbiten);

/* function for rbentity */
//initialize dio_rbentity
static void init_rbentity(struct dio_rbentity* prben);
static struct dio_rbentity* rb_search_entity(uint64_t sector);
static struct dio_rbentity* __rb_insert_entity(struct dio_rbentity* prben);
static struct dio_rbentity* rb_insert_entity(struct dio_rbentity* prben);

/* function for nugge */
static void init_nugget(struct dio_nugget* pdng);

// it return a valid nugget point even if inserted 'sector' doesn't existed in rbtree
// if NULL value is returned, reason is a problem of inserting the new rbentity 
// or memory allocating the new nugget 
static struct dio_nugget* get_nugget_at(uint64_t sector);

static void handle_action(uint32_t act, struct dio_nugget* pdng);

/*--------------	global variables	-----------------------*/
#define MAX_FILEPATH_LEN 255
static char respath[MAX_FILEPATH_LEN];	//result file path
static struct rb_root rben_root;	//root of rbentity tree
static struct list_head biten_head;

/*--------------	function implementations	---------------*/
int main(int argc, char** argv){
	INIT_LIST_HEAD(&biten_head);
	rben_root = RB_ROOT;

	int ifd = -1;
	int rdsz = 0;
	char pdubuf[256];

	strncpy(respath, "dioshark.output", MAX_FILEPATH_LEN);

	ifd = open(respath, O_RDONLY);
	if( ifd < 0 ){
		perror("failed to open result file");
		goto err;
	}
	
	struct bit_entity* pbiten = NULL;
	struct dio_nugget* pdng = NULL;

	int i;
	while(1){
		pbiten = (struct bit_entity*)malloc(sizeof(struct bit_entity));
		if( pbiten == NULL ){
			perror("failed to allocate memory");
			goto err;
		}

		rdsz = read(ifd, &(pbiten->bit), sizeof(struct blk_io_trace));
		if( rdsz < 0 ){
			perror("failed to read");
			goto err;
		}
		else if( rdsz == 0 ){
			DBGOUT(">end read\n");
			break;
		}
		
		//BE_TO_LE_BIT(pbiten->bit);

		DBGOUT(">pdu_len : %d\n", pbiten->bit.pdu_len);
		if( pbiten->bit.pdu_len > 0 ){
			rdsz = read(ifd, pdubuf, pbiten->bit.pdu_len);
			pdubuf[rdsz] = '\0';
			DBGOUT(">pdu data : %s\n", pdubuf);
			//lseek(ifd, pbiten->bit.pdu_len, SEEK_CUR);
		}
		
		//insert into list order by time
		insert_proper_pos(pbiten);
	}

	struct list_head* p= NULL;
	__list_for_each(p, &biten_head){
		struct bit_entity* pb = list_entry(p, struct bit_entity, link);

		pdng = get_nugget_at(pb->bit.sector);	
		if( pdng == NULL ){
			DBGOUT(">failed to get nugget at sector %llu\n", pb->bit.sector);
			goto err;
		}
		handle_action(pb->bit.action, pdng);
	}

	//test printing
	DBGOUT("end parse.\nprint start\n");
	p = NULL;
	__list_for_each(p, &(biten_head)){
		struct bit_entity* _pbiten = list_entry(p, struct bit_entity, link);
		DBGOUT("time : %llu, sector %llu, action 0x%x, pid %d, cpu %d\n", 
			_pbiten->bit.time, _pbiten->bit.sector, _pbiten->bit.action, 
			_pbiten->bit.pid, _pbiten->bit.cpu);
	}
	DBGOUT("end printing\n");

	//clean all list entities
	return 0;
err:
	if( ifd < 0 )
		close(ifd);
	if( pbiten != NULL )
		free(pbiten);
	return 0;
}

void insert_proper_pos(struct bit_entity* pbiten){
	struct list_head* p = NULL;
	struct bit_entity* _pbiten = NULL;

	//list foreach back
	for(p = biten_head.prev; p != &(biten_head); p = p->prev){
		_pbiten = list_entry(p, struct bit_entity, link);
		if( _pbiten->bit.time <= pbiten->bit.time ){
			list_add(&(pbiten->link), p);
			return;
		}
	}
	list_add(&(pbiten->link), &(biten_head));
}

static void init_rbentity(struct dio_rbentity* prben){
	INIT_LIST_HEAD(&prben->nghead);
	prben->sector = 0;
}

static struct dio_rbentity* rb_search_entity(uint64_t sector){
	struct rb_node* p = rben_root.rb_node;
	struct dio_rbentity* prben = NULL;

	while(p){
		prben = rb_entry(p, struct dio_rbentity, rblink);
		if( sector < prben->sector )
			p = prben->rblink.rb_left;
		else if( sector > prben->sector )
			p = prben->rblink.rb_right;
		else
			return prben;
	}
	return NULL;
}

static struct dio_rbentity* __rb_insert_entity(struct dio_rbentity* prben){
	struct rb_node** p = &rben_root.rb_node;
	struct rb_node* parent = NULL;
	struct dio_rbentity* prbenbuf = NULL;

	while(*p){
		parent = *p;
		prbenbuf = rb_entry(parent, struct dio_rbentity, rblink);

		if( prben->sector < prbenbuf->sector )
			p = &(*p)->rb_left;
		else if( prben->sector > prbenbuf->sector )
			p = &(*p)->rb_right;
		else
			return prbenbuf;	//there already exists
	}

	rb_link_node(&(prben->rblink), parent, p);
	return NULL;	//success
}

static struct dio_rbentity* rb_insert_entity(struct dio_rbentity* prben){
	struct dio_rbentity* rbenret = NULL;
	if( (rbenret = __rb_insert_entity(prben)) )
		return rbenret;	//there already exists

	rb_insert_color(&(prben->rblink), &(rben_root));
	return NULL;	//insert successfully
}

void init_nugget(struct dio_nugget* pdng){
	memset(pdng, 0, sizeof(struct dio_nugget));
	//pdng->elemidx = 0;
}

struct dio_nugget* get_nugget_at(uint64_t sector){
	struct dio_nugget* pdng = NULL;
	struct dio_rbentity* prben = NULL;

	prben = rb_search_entity(sector);
	if( prben == NULL ){
		prben = (struct dio_rbentity*)malloc(sizeof(struct dio_rbentity));
		init_rbentity(prben);
		prben->sector = sector;
		if( rb_insert_entity(prben) != NULL ){
			free(prben);
			DBGOUT(">failed to insert rbentity into rbtree\n");
			return NULL;
		}
	}

	//return the first item of nugget list
	if( !list_empty(&prben->nghead) ){
		pdng = list_entry(prben->nghead.next, struct dio_nugget, nglink);
		if( !pdng->isend )
			return pdng;
	}

	pdng = NULL;
	pdng = (struct dio_nugget*)malloc(sizeof(struct dio_nugget));
	if( pdng == NULL ){
		perror("failed to allocate nugget memory");
		return NULL;
	}

	init_nugget(pdng);
	pdng->sector = sector;
	list_add(&pdng->nglink, &prben->nghead);

	return pdng;
}

void handle_action(uint32_t act, struct dio_nugget* pdng){
	switch( act ){
	case BLK_TA_QUEUE:
		pdng->states[pdng->elemidx++] = 'Q';
		break;
	case BLK_TA_BACKMERGE:
		pdng->states[pdng->elemidx++] = 'M';
		break;
	case BLK_TA_FRONTMERGE:
		pdng->states[pdng->elemidx++] = 'M';
		break;
	case BLK_TA_GETRQ:
	case BLK_TA_SLEEPRQ:
	case BLK_TA_REQUEUE:
	case BLK_TA_ISSUE:
	case BLK_TA_COMPLETE:
	case BLK_TA_PLUG:
	case BLK_TA_UNPLUG_IO:
	case BLK_TA_UNPLUG_TIMER:
	case BLK_TA_INSERT:
	case BLK_TA_SPLIT:
	case BLK_TA_BOUNCE:
	case BLK_TA_REMAP:
	case BLK_TA_ABORT:
	case BLK_TA_DRV_DATA:

	case BLK_TN_PROCESS:
	case BLK_TN_TIMESTAMP:
	case BLK_TN_MESSAGE:
	default:
		break;
	};
}
