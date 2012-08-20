#ifndef DIO_LIST_H
#define DIO_LIST_H

/*
	dio_list.h
	This contains declaration and implementation about double linked list structure
	dio_list is not general linked list, it just use for handling dio-shark tracing data

	frequently used variable names
		pdlh : it indicates pointer of dl_head ( roll of list descriptor )
			<'p'ointer of 'dl'_'h'ead>
		pdlnh : it indicates pointer of real header of list structure
			<'p'ointer of 'dl'_'n'ode 'h'eader>
*/

// dio list entity
// contains : 	next, prev pointer to moving
// 		void* type data indicator.
struct dl_node{
	struct dl_node *nxt, *prv;
	void* pdata;
};

// handler type of data destruction
typedef void(*d_deleter)(void*);

// dio list head descriptor
// contains :	first node (named head)
//		list length for list handling ( users have permission only to read )
//		data destruction handler for deleting pdatas in each entity
struct dl_head{
	struct dl_node head;
	int length;
	d_deleter del_handler;
};


/* list manipulating functions */

// initializing dio list header
// this is only for internal list manapulation
// @param pdlh :	the pointer of list head
#define __init_dl_head(pdlh)\
		pdlh->head.nxt = &(pdlh->head);\
		pdlh->head.prv = &(pdlh->head);\
		pdlh->head.pdata = NULL;\
		pdlh->length = 0;\
		pdlh->del_handler = NULL;

// create dio list descriptor(dl_head) and return its pointer
// @param d_del_handler : deletion handler for each entity's data
//
// @return :		the pointer of initialized list head
static struct dl_head* create_list_head(d_deleter d_del_handler){
	struct dl_head* p = (struct dl_head*)malloc(sizeof(struct dl_head));
	__init_dl_head(p);
	p->del_handler = d_del_handler;
	return p;
}

// initializing dio list node
// this is only for internal list manapulation
// @param pdln :	the pointer of list node
#define __init_dl_node(pdln)\
		pdln->nxt = NULL;\
		pdln->prv = NULL;\
		pdln->pdata = NULL;\

// create dio list node and assign the data
// this is only for internal list manapulation
// @param pdata :	the pointer of data (data must have created on outer space)
// 
// @return :		the pointer of created node
static struct dl_node* __create_list_node(void* pdata){
	struct dl_node* p = NULL;
	p = (struct dl_node*)malloc(sizeof(struct dl_node));
	__init_dl_node(p);

	p->pdata = pdata;
	return p;
}

// foreach macro to iterating dio list
// @param dlnh :	value of real list head (type of dl_node)
// @param container :	pointer of iterator
#define __foreach_list(dlnh, container) \
		for( container = dlnh.nxt; container->nxt != &(dlnh); container = container->nxt )

// search node in dio list 
// @param pdlh :	pointer of list head
// @param idx :		index
//
// @return :		if there are entity index of 'idx' than return that dl_node pointer
//			else, return NULL (users have to check return value)
static struct dl_node* search_at(struct dl_head* pdlh, int idx){
	int n=0;
	struct dl_node* each=NULL;
	
	if( pdlh->length >= idx )
		return NULL;

	__foreach_list(pdlh->head, each){
		if( n == idx )
			return each;
		n++;
	}
	return NULL;
}

// insert node macro.
// this is only for internal list manapulation
// @param node_ptr :	dl_node pointer to inserted
// @param before :	dl_node pointer which indicates where to insertion
// @param after :	dl_node pointer which indicates where to insertion
#define __insert_node(node_ptr, after, before)\
		(node_ptr)->prv = (after);\
		(after)->nxt = (node_ptr);\
		(node_ptr)->nxt = (before);\
		(before)->prv = (node_ptr);
		
// insert data into the index of 'idx'
// @param pdlh :	the pointer of list head
// @param pdata :	the pointer of a data which wants to be inserted
// @param idx :		the index which the data is inerted
//
// @return :		return true when successfully inserted
static bool insert_data(struct dl_head* pdlh, void* pdata, int idx){
	struct dl_node* after = NULL;
	if( idx == 0)
		after = &(pdlh->head);
	else if( idx == pdlh->length )
		after = pdlh->head.prv;
	else
		after = search_at(pdlh, idx-1);

	if( after == NULL )
		return false;
	
	struct dl_node* pdlnbuf = __create_list_node(pdata);
	
	__insert_node(pdlnbuf, after, after->nxt);

	pdlh->length++;
	return true;
}

// push back to dio list
// this function same witch insert_data(pdlh, pdata, pdlh->length)
// @param pdlh:		the pointer of list head
// @param pdata :	the pointer of a data which wants to be inserted
static void push_back_data(struct dl_head* pdlh, void* pdata){
	struct dl_node* pdlnbuf = __create_list_node(pdata);

	__insert_node(pdlnbuf, pdlh->head.prv, &(pdlh->head));
	pdlh->length++;
}


#endif
