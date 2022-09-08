#ifndef __FILE_H__
#define __FILE_H__

// Uncomment the line below if you are compiling on Windows.
// #define WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> //added this to support uint64_t, 2020-10-04
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
	unsigned char reserved[104]; //104 bytes
	pagenum_t sibling; //right sibling page num (leaf page)/ first child (internal page)  page num (8 bytes)
} page_hd; //(8 + 4 + 4 + 104 + 8 = 128 bytes)


//Record in Leaf Page/Node of 128 bytes
typedef struct page_record {
	pagenum_t key; //8 bytes
	char value[120]; //120 bytes
} page_record;


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


// GLOBALS.

extern int order;
extern bool verbose_output;
extern int globalNodeNum;
extern node * queue;
extern node * root;
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
int db_insert (int64_t key, char * value);
int db_find (int64_t key, char * ret_val);
int db_delete (int64_t key);

//2020-10-04
void print_tree_disk( node * root );
void save_node_to_disk(node * root);
void print_db(char *pathname);
void build_bpt(char *pathname);


void enqueue( node * new_node );
node * dequeue( void );
int path_to_root( node * root, node * child );
void print_tree( node * root );
void find_and_print(node * root, int key, bool verbose); 
node * find_leaf( node * root, int key, bool verbose );
record * find( node * root, int key, bool verbose );
int cut( int length );


// Insertion.
record * make_record(char * value); //2020-10-04 changed parameter to char pointer
node * make_node( void );
node * make_leaf( void );
int get_left_index(node * parent, node * left);
node * insert_into_leaf( node * leaf, int key, record * pointer );
node * insert_into_leaf_after_splitting(node * root, node * leaf, int key, record * pointer);
node * insert_into_node(node * root, node * parent, int left_index, int key, node * right);
node * insert_into_node_after_splitting(node * root, node * parent, int left_index, int key, node * right);
node * insert_into_parent(node * root, node * left, int key, node * right);
node * insert_into_new_root(node * left, int key, node * right);
node * start_new_tree(int key, record * pointer);
node * insert( node * root, int key, char * value ); //2020-10-04 changed 3rd parameter to char pointer


// Deletion.
int get_neighbor_index( node * n );
node * adjust_root(node * root);
node * coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime);
node * redistribute_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime_index, int k_prime);
node * delete_entry( node * root, node * n, int key, void * pointer );
node * delete( node * root, int key );

void destroy_tree_nodes(node * root);
node * destroy_tree(node * root);


#endif /* __FILE_H__*/
