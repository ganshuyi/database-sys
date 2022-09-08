#ifndef __FILE_H__
#define __FILE_H__

// Uncomment the line below if you are compiling on Windows.
// #define WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> //added this to support uint64_t, 2020-10-04
#include <pthread.h> //lock table

#define HSIZE 1000 //lock table
#define GTSIZE 1000 //transaction for wait-for graph


#ifdef WINDOWS
#define bool char
#define false 0
#define true 1
#endif


//2020-10-04 page structures

// Default order is 32. 2020-10-04
#define DEFAULT_ORDER 32
#define MIN_ORDER 16
#define MAX_ORDER 32 //2020-10-04

#define BUFFER_SIZE 256

#define PAGE_SIZE 4096

//TYPE
typedef struct record {
    char value[120]; //2020-10-04 changed variable to array of char
} record;

typedef struct node {
    void ** pointers;
    int * keys;
    struct node * parent;
    bool is_leaf;
    int num_keys;
    int thisNodeNum;
    struct node * next; // Used for queue.
} node;


typedef int64_t pagenum_t; //(8 bytes)

//Page header (128 bytes)
typedef struct page_hd {
	pagenum_t parent; //parent (internal/leaf page/note) or next free page number (for header page), (8 bytes)
	int is_leaf; //if this is leaf page (4 bytes)
	int num_keys; //total keys in this page or total pages for this data file, (4 bytes)
	unsigned char reserved1[8]; //8 bytes
	pagenum_t lsn; //page LSN value, 8 bytes
	unsigned char reserved2[88]; //88 bytes
	pagenum_t sibling; //right sibling page num (leaf page)/ first child (internal page)  page num (8 bytes)
} page_hd; //(8 + 4 + 4 + + 8 + 8 + 88 + 8 = 128 bytes)


//Record in Leaf Page/Node of 128 bytes
typedef struct page_record {
	pagenum_t key; //8 bytes
	char value[120]; //120 bytes
} page_record;


typedef struct page_lrec {
	pagenum_t lsn; //8 bytes
	pagenum_t key; //8 bytes
	char value[120]; //120 bytes
	struct page_lrec * next;
} page_lrec;


//Entry in Internal Page/Node of 16 bytes
typedef struct page_entry {
	pagenum_t key; //8 bytes
	pagenum_t pagenum; //8 bytes
} page_entry;


//Page (leaf) (4096 bytes)
typedef struct page_leaf_t {
	struct page_hd header; //128 bytes
	struct page_record rec[31]; //leaf page/node (128 bytes x 31)
} page_leaf_t;


//Page (internal) (4096 bytes)
typedef struct page_internal_t {
	struct page_hd header; //128 bytes
	struct page_entry entry[248]; //leaf page/node, of 16 bytes each x 248
} page_internal_t;


//Free Page (leaf) (4096 bytes)
typedef struct page_free_t {
	pagenum_t next_free_pgnum; //next free page number (8 bytes)
	unsigned char reserved[4088]; //reserved (4088 bytes)
} page_free_t;


//Header Page (special) - first record in data file (4096 bytes)
typedef struct page_header_t {
	pagenum_t first_free_pg; //points to the first free page (8 bytes)
	pagenum_t root_pg; //root page number (8 bytes)
	pagenum_t total_pg; //total pages in this data file (8 bytes)
	unsigned char reserved[4072]; //reserved (4072 bytes)
} page_header_t;


//In-memory page structure
typedef struct page_t {
	struct page_hd header;
	pagenum_t this_pgnum; //this page number
	pagenum_t next_free_pg; //points to the next free page (8 bytes)
	pagenum_t root_pg; //root page number (8 bytes)
	pagenum_t total_pg; //total pages in this data file (8 bytes)

	struct page_record rec[31]; //leaf page/node
	struct page_record entry[248]; //leaf page/node, of 16 bytes each
} page_t;


//Buffer structure
typedef struct buffer_t {
	struct node * page_n; //stores the node
	struct page_internal_t page_i; //stores the page data (internal page)
	struct page_leaf_t page_l; //stores the page data (leaf page)
	int table_id; // the unique id of table of the loaded data file
	pagenum_t page_num; //the target page number within the loaded data file
	bool is_dirty; //whether this buffer block is dirty (updated) or not
	bool is_pinned; //whether this buffer block is accessed right now
	bool has_data; //whether this buffer block has data, ie not empty
	
	pagenum_t lsn; //last seq no
	
	bool refbit; //LRU bit
} buffer_t;


//lock manager datastructures

//actions
typedef struct xact_t {
	char act_name; //U: update, R: read
	int table_id;
	pagenum_t lsn;
	int64_t key;
	char ori_value[120];
	char new_value[120];
	struct lock_t * lock_obj;
	struct xact_t * next;
} xact_t;


//transaction
typedef struct trx_t {
	int tid;
	int lock_mode; //0:Shared, 1:Xclusive
	struct xact_t * actions; //list of actions for this transaction
	struct trx_t * next;
} trx_t;


typedef struct queue_t {
	int qid; //queue id
	int tid; //transaction id
	int lock_mode; //0:Shared, 1:Xclusive
	struct queue_t *next;
} queue_t;


//lock table
typedef struct lock_t {
	int id;
	int locked; //0:not locked, 1:locked
	int qlen;
	struct queue_t * q;
	
} lock_t;


//wait-for-graph
// A linked list graph node for wait-for graph
typedef struct gnode_t { 
    int id;
    int w[GTSIZE];  
    struct gnode_t * next; 
} gnode_t;


//Log record, 288 bytes
typedef struct log_t { 
    pagenum_t lsn; //log sequence number, 8 bytes
    pagenum_t plsn; //previous log id, 8 bytes
    int trx_id; //transaction id, 4 bytes
    int type; //log type: Begin(0), Update(1), Commit(2), Rollback(3), Compensate(4), 4 bytes
    
    int table_id; //4 bytes
    pagenum_t pagenum; //8 bytes
    int offset; //4 bytes
    int data_len; //4 bytes
    
    char old_image[120]; //old value, 120 bytes
    char new_image[120]; //new value, 120 bytes
    
    pagenum_t next_undo_lsn; //for compensate log only, 8 bytes 
} log_t;


//Log buffer in memory
typedef struct logbuffer_t { 
    log_t log; //log record
    struct logbuffer_t * next;
} logbuffer_t;




// GLOBALS.

extern int order;
extern bool verbose_output;
extern int globalNodeNum;
extern node * queue;
extern node * root;
extern page_lrec * pg_lsn_ls;
extern pagenum_t root_pg_num;
extern pagenum_t next_free_pg_num;
extern pagenum_t total_pg_num;
extern char * bpt_filename;


// FUNCTION PROTOTYPES.

pagenum_t file_alloc_page();
void file_free_page(pagenum_t pagenum);
void file_read_page(pagenum_t pagenum, page_t* dest);
void file_write_page(pagenum_t pagenum, const page_t* src);

int open_table (char *pathname);
int db_insert (int table_id, int64_t key, char * value);
int db_find (int table_id, int64_t key, char* ret_val, int trx_id);
int db_update(int table_id, int64_t key, char* value, int trx_id);
int db_delete (int table_id, int64_t key);


//2020-10-04
void print_tree_disk(node * root);
void save_node_to_disk(int table_id, node * root);
void print_db(char *pathname);
void build_bpt(int table_id, char *pathname);

void add_to_pg_lsn(pagenum_t key, pagenum_t lsn);
pagenum_t get_pg_lsn(pagenum_t key);
pagenum_t prn_pg_lsn();

void enqueue( node * new_node );
node * dequeue( void );
int path_to_root( node * root, node * child );
void print_tree( node * root );
void find_and_print(int table_id, node * root, int key, bool verbose); 
node * find_leaf(int table_id, node * root, int key, bool verbose, int trx_id);
record * find(int table_id, node * root, int key, bool verbose, int trx_id);
int cut( int length );


// Insertion.
record * make_record(char * value); //2020-10-04 changed parameter to char pointer
node * make_node( void );
node * make_leaf( void );
int get_left_index(node * parent, node * left);
node * insert_into_leaf(int table_id, node * leaf, int key, record * pointer );
node * insert_into_leaf_after_splitting(int table_id,node * root, node * leaf, int key, record * pointer);
node * insert_into_node(int table_id, node * root, node * parent, int left_index, int key, node * right);
node * insert_into_node_after_splitting(int table_id, node * root, node * parent, int left_index, int key, node * right);
node * insert_into_parent(int table_id, node * root, node * left, int key, node * right);
node * insert_into_new_root(int table_id, node * left, int key, node * right);
node * start_new_tree(int table_id, int key, record * pointer);
node * insert(int table_id, node * root, int key, char * value ); //2020-10-04 changed 3rd parameter to char pointer


// Deletion.
int get_neighbor_index( node * n );
node * adjust_root(int table_id, node * root);
node * coalesce_nodes(int table_id, node * root, node * n, node * neighbor, int neighbor_index, int k_prime);
node * redistribute_nodes(int table_id, node * root, node * n, node * neighbor, int neighbor_index, int k_prime_index, int k_prime);
node * delete_entry(int table_id, node * root, node * n, int key, void * pointer );
node * delete(int table_id, node * root, int key );

void destroy_tree_nodes(node * root);
node * destroy_tree(node * root);


// Buffer Management
extern buffer_t * frame_table;
int init_buffer (int num_buf);
void prn_buf(buffer_t * buf);
void prn_table_id(int this_id);
void add_page_to_buffer(int table_id, node * thisnode, bool is_dirty);
int get_buffer_frame();
int write_buffer_to_disk(buffer_t this_frame);
int find_in_buffer(int table_id, node * thisnode);
void update_page_in_buffer(int frame_id, node * thisnode);
int close_table(int table_id);
int shutdown_db(void);
int save_db(int64_t key, int action, char * value, char * db_fname);
page_record search_db(int64_t key, char * db_fname);
void sv_page (char *pathname, int key, char * value, pagenum_t lsn);
page_lrec fetch_page (char *pathname, int key);


/* APIs for lock table */
int init_lock_table();
lock_t* lock_acquire(int table_id, int64_t key,int trx_id, int lock_mode);
int lock_release(lock_t* lock_obj);
int do_hash(int table_id,  int64_t key);
void add_to_queue(queue_t * q, int trx_id, int lock_mode);
int chk_queue(queue_t * q);
void remove_head(queue_t * q);
void remove_last(queue_t * q);
void process_queue(queue_t * q);
void prn_queue(queue_t * q);

int trx_begin(void);
int trx_commit(int trx_id);

int add_to_trx_arr(int table_id, int64_t key, int trx_id, int lock_mode, char act_name, char * ori_val, char * new_val);
void prn_trx(int trx_id);


//project 6
int trx_abort(int trx_id);


// GLOBALS for Lock table
extern lock_t hash_table[HSIZE];
extern pthread_mutex_t lock_table_latch;
extern pthread_cond_t my_cv;
extern int global_qid;
extern int global_cnt;
extern int global_trx_id;
extern trx_t * trx_table;
extern char table_id_ls[11][20];


//Wait-for-graph
gnode_t * add_edge(gnode_t * wg, int gnode_id, int trx_id);
gnode_t * del_edge(gnode_t * wg, int gnode_id, int trx_id);
void prn_graph(gnode_t * wg);
bool cyclicUtil(gnode_t * hd, struct gnode_t * wg, bool * visited, bool * recStack);
int detect_cyclic(gnode_t * wg);
int chk_wfg(gnode_t * wg, int resource_id, int trx_id);

extern gnode_t * wgraph_hd;


//Crash & Recovery
int init_db (int buf_num, int flag, int log_num, char* log_path, char* logmsg_path);
int add_to_log(pagenum_t pagenum, int table_id, int trx_id, int type, int lsn, int plsn, char * oldvalue, char * newvalue, pagenum_t next_undo_lsn);
int flush_log(char * log_path);
void init_log();
void prn_log(char * log_path);
void prn_logbuf();
void update_master_lsn(int action);

logbuffer_t * logbuf;
extern pagenum_t last_lsn;
extern pagenum_t flushed_lsn;
extern char log_fname[20];
extern char logmsg_fname[20];
extern char db_fname[20];
extern int activetrans[100];



#endif /* __FILE_H__*/
