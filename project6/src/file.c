/* file.c  */

#include "file.h"
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


// GLOBALS.
page_t * pg_queue = NULL;
int order = DEFAULT_ORDER;
int globalNodeNum = 0;
node * queue = NULL;
node * root = NULL;
page_lrec * pg_lsn_ls = NULL;
pagenum_t root_pg_num = 0;
pagenum_t next_free_pg_num = 0;
pagenum_t total_pg_num = 0;
char * bpt_filename;
bool verbose_output = false;

buffer_t * frame_table = NULL; //Buffer Manager

int buf_size = 0; //Size of buffer
int last_table_id = 0; //ID assigned to each opened table, between 1 to 10;
int clock_hd = 0; //clock hand for LRU policy
char table_id_ls[11][20]; //array of table id to store opened filenames

//Lock table
lock_t hash_table[HSIZE]; //hash array of HSIZE
pthread_mutex_t lock_table_latch = PTHREAD_MUTEX_INITIALIZER; //lock latch
pthread_cond_t lock_cv = PTHREAD_COND_INITIALIZER; //condition variable

int global_qid = 0;
int global_cnt = 0;
int locked_tcnt = 0;
int global_trx_id = 0; //global transaction id
trx_t * trx_table = NULL; //global transaction table

gnode_t * wgraph_hd = NULL; //wait graph list


//Crash & Recovery
pagenum_t last_lsn = 0;
pagenum_t flushed_lsn = 0;

logbuffer_t * logbuf = NULL;//log buffer in-memory
int activetrans[100]; //array to keep track of the last LSN for each transaction thread

char log_fname[20]; //log filename
char logmsg_fname[20]; //log msg filename
char db_fname[20]; //db filename

// FUNCTION DEFINITIONS.

// OUTPUT AND UTILITIES

/* Open existing data file using ‘pathname’ or create one if not existed.
 * If success, return the unique table id, which represents the own table in this database.
 * Otherwise, return negative value.
 */
int open_table (char *pathname) {
	FILE *fptr;
	page_header_t rec; //header page
	bpt_filename = pathname;
	
	//get table_id from pathname, eg data1, table_id=1
	int table_id = 0;
	sscanf(pathname,"%*[^0-9]%d",&table_id);
	//printf("Table id:%d\n",table_id);
	
	//check if buffer has been initialised
	if (frame_table == NULL) {
		printf("Please initialise buffer first. Command: h BUFFER_SIZE\n");
		return 0;
	}
	
	fptr=fopen(pathname,"r"); //open file for reading
	
	//assign table ID and store filename to table_id array
	//last_table_id++;
	
	strcpy(table_id_ls[table_id],bpt_filename); //strcopy(destination, src)
	
	//remove(db_fname); //remove old db file

	if (!fptr) {//if file does not exist
		page_t * n;
		n = malloc(sizeof(page_t));
		n->this_pgnum = 0; //initialise to 0
		n->next_free_pg = 0; //initialise to 0
		n->root_pg = 0; //initialise to 0
		n->total_pg = 0; //initialise to 0
		
		file_write_page(0,n);
		printf("Empty file. Creating new file: %s\n", bpt_filename);
	} 
	else {
		build_bpt(table_id,pathname);
		//printf("FirstFreePg:%d, RootPg:%d, TotalPg:%d\n",next_free_pg_num,root_pg_num,total_pg_num);
		fclose(fptr); //close file after reading
	}
	
	return table_id;
} //open_table


/* Insert input ‘key/value’ (record) to data file at the right place. 
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_insert (int table_id, int64_t key, char * value) {
	//printf ("v: %s\n", value);
	root = insert(table_id, root, key, value); //2020-10-04
	return 0;
} //db_insert


/* Find the record containing input ‘key’.
 * If found matching ‘key’, store matched ‘value’ string in ret_val and return 0.
 * Otherwise, return non-zero value
 */
int db_find(int table_id, int64_t key, char* ret_val, int trx_id) {
	record * r = find(table_id, root, key, false, trx_id);
	//page_record pr = search_db(key,db_fname);
	if (r != NULL) {  //if found
		ret_val = r->value;
		printf("found key: %ld, value: %s\n",key,r->value);
		//printf("found key2: %ld, value2: %s\n",pr.key,pr.value);
	}
	else {printf("No key found for: %ld\n",key);}
	
	
	return 0; //success
} //db_find



int db_update(int table_id, int64_t key, char* value, int trx_id) {
	record * r = find(table_id, root, key, false, trx_id);
	if (r != NULL) {  //if found
		int stat = add_to_trx_arr(table_id,key,trx_id,1,'U',r->value,value); //store ori value and new value, for rollback, if needed
		
		last_lsn++;
		add_to_log(key,table_id,trx_id,1,last_lsn,activetrans[trx_id],r->value,value,0);
	
		if (stat) {
			printf("found key: %ld, value: %s\n",key,r->value);
			strcpy(r->value,value); //strcopy(destination, src)
			printf("update key: %ld, to value: %s\n",key,r->value);
			
			//save_db(key,2,value,db_fname);
			//printf("table_id: %d, filename: %s\n",table_id,table_id_ls[table_id]);
			sv_page(table_id_ls[table_id],key,value,last_lsn);
			add_to_pg_lsn(key,last_lsn);
		}
		else {
			printf("Failed to perform update!\n");
		}
	}
	else {printf("No key found for: %ld\n",key);}
	
	return 0; //success
} //db_update


/* Find the matching record and delete it if found
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_delete (int table_id, int64_t key) {
	root = delete(table_id, root, key);
	return 0;
} //db_delete


// File Actions
// Allocate an on-disk page from the free page list
pagenum_t file_alloc_page() {
	if (!next_free_pg_num) {return 0;} //if no more free page
	pagenum_t current_free_pg_num = next_free_pg_num;
	
	page_free_t rec;
	
	FILE *fptr1; //file handle
	fptr1=fopen(bpt_filename,"r"); //open file for reading
	if (!fptr1) {return 0;}
	
	int rec_location = sizeof(page_free_t) * current_free_pg_num;
	fseek(fptr1,rec_location,SEEK_SET); //point file handle to the correct location
	fread(&rec,sizeof(page_internal_t),1,fptr1);
	fclose(fptr1); //close file after reading
	
	next_free_pg_num = rec.next_free_pgnum; //assign to next available free page
	
	return current_free_pg_num;
} //file_alloc_page


// Free an on-disk page to the free page list
void file_free_page(pagenum_t pagenum) {
	
	pagenum_t current_free_pg_num = next_free_pg_num;
	page_free_t rec1, rec2, tmp;
	int rec_location;
	
	rec1.next_free_pgnum = pagenum; //the current free page should be pointed to the new free page
	
	FILE *fptr1; //file handle
	fptr1=fopen(bpt_filename,"r+"); //open file for reading
	
	rec_location = sizeof(page_internal_t) * current_free_pg_num; //update the current free page record
	fseek(fptr1,rec_location,SEEK_SET); //write to the record
	
	fread(&tmp,sizeof(page_internal_t),1,fptr1); //read the current free page record, to find out the next free page it is pointed to
	rec2.next_free_pgnum = tmp.next_free_pgnum; //and assign that to the new free page
	
	fseek(fptr1,rec_location,SEEK_SET); //write to the record
	fwrite(&rec1,sizeof(page_header_t),1,fptr1);

	rec_location = sizeof(page_internal_t) * pagenum; //update the new free page record
	fseek(fptr1,rec_location,SEEK_SET); //write to the record
	fwrite(&rec2,sizeof(page_header_t),1,fptr1);

	fclose(fptr1); //close file after writing
} //file_free_page


// Read an on-disk page into the in-memory page structure(dest)
void file_read_page(pagenum_t pagenum, page_t* dest) {
	FILE *fptr1; //file handle
	fptr1=fopen(bpt_filename,"r"); //open file for reading
	
	if (fptr1) {
		page_internal_t rec; //internal page
		page_leaf_t rec_leaf; //leaf page
		
		int rec_location = sizeof(page_internal_t) * pagenum;
		
		fseek(fptr1,rec_location,SEEK_SET);
		fread(&rec,sizeof(page_internal_t),1,fptr1);
		
		if (rec.header.is_leaf) {
			fseek(fptr1,rec_location,SEEK_SET);
			fread(&rec_leaf,sizeof(page_leaf_t),1,fptr1);
		}
		
		fclose(fptr1); //close file after reading
		//dest = rec;
	}
} //file_read_page


// Write an in-memory page(src) to the on-disk page
void file_write_page(pagenum_t pagenum, const page_t* src) {
	FILE *fptr1; //file handle
	fptr1=fopen(bpt_filename,"r+"); //open file for writing (update)
	if (!fptr1) {fptr1=fopen(bpt_filename,"w");} //open file for writing, if file does not exist
	
	if (pagenum == 0) { //first record in file, the Header Page
		page_header_t rec; //header page
		//printf("Writing - FirstFreePg:%d, RootPg:%d, TotalPg:%d\n",src->next_free_pg,src->root_pg,src->total_pg);
		
		rec.first_free_pg = src->next_free_pg;
		rec.root_pg = src->root_pg;
		rec.total_pg = src->total_pg;
		
		//printf("Size of page_header_t rec: %d\n", sizeof(page_header_t));
		
		fseek(fptr1,0,SEEK_SET); //write to the start of the file (first record)
		fwrite(&rec,sizeof(page_header_t),1,fptr1);
	} //if pagenum == 0
	
	fclose(fptr1); //close file after writing
} //file_write_page



void update_header_pg() {
	page_t * n;
	n = malloc(sizeof(page_t));
	n->next_free_pg = next_free_pg_num;
	n->root_pg = root_pg_num;
	n->total_pg = total_pg_num;
	
	file_write_page(0,n);
} //update_header_pg


void save_node_to_disk(int table_id, node * thisnode) {
	return;
	
	if (!table_id) {return;} //if no table_id (data file name) provided
	
	bpt_filename = table_id_ls[table_id];
	if (!bpt_filename) {return;} //if no filename associated to table_is_ls[]
	
	printf ("Saving to filename: %s\n",bpt_filename);
    
    FILE *fptr;
    record * r;
    int i = 0;
    int rec_exists = 0;
    int parent_num = 0;
    int next_num = 0;
    node * n;
    page_leaf_t rec;
    
    //find out if the node/page exists in bpt_filename. If exists, update the record. Else, append to file
    fptr=fopen(bpt_filename,"r"); //open file for reading
    int rec_location = sizeof(page_leaf_t)*thisnode->thisNodeNum; 
    fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
	fread(&rec,sizeof(page_leaf_t),1,fptr);
	fclose(fptr); //close file after reading
	
	if (rec.header.num_keys) {rec_exists = 1;}
	
	if (rec.header.is_leaf) { //leaf page
		page_leaf_t rec_l;
		parent_num = 0;
		if (thisnode->parent != NULL) {parent_num = thisnode->parent->thisNodeNum;}
		
		//header
		rec_l.header.parent = parent_num;
		rec_l.header.is_leaf = 1;
		rec_l.header.num_keys = thisnode->num_keys;
		
		for (i = 0; i < thisnode->num_keys; i++) {
			rec_l.rec[i].key = thisnode->keys[i];
			r = (record *)thisnode->pointers[i];
			strcpy(rec_l.rec[i].value,r->value); //strcopy(destination, src)
		} //for
		
		
		//write to file
		if (rec_exists) { //update record
			fptr=fopen(bpt_filename,"r+"); //open file for update
			int rec_location = sizeof(page_leaf_t)*thisnode->thisNodeNum;
			fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
			fwrite(&rec_l,sizeof(page_leaf_t),1,fptr);
			fclose(fptr); //close file after reading
		}
		else { //append rec to end of file
			fptr=fopen(bpt_filename,"a"); //open file for append
			fwrite(&rec_l,sizeof(page_leaf_t),1,fptr);
			fclose(fptr); //close file after reading
		}
		
	} //if leaf page
	else { //internal page
		page_internal_t rec_i;
		parent_num = 0;
		if (thisnode->parent != NULL) {parent_num = thisnode->parent->thisNodeNum;}
		
		//header
		rec_i.header.parent = parent_num;
		rec_i.header.is_leaf = 0;
		rec_i.header.num_keys = thisnode->num_keys;
		
		for (i = 0; i < thisnode->num_keys; i++) {
			rec_i.entry[i].key = thisnode->keys[i];
			rec_i.entry[i].pagenum = 0;
		} //for
		
		
		//write to file
		if (rec_exists) { //update record
			fptr=fopen(bpt_filename,"r+"); //open file for update
			int rec_location = sizeof(page_internal_t)*thisnode->thisNodeNum;
			fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
			fwrite(&rec_i,sizeof(page_leaf_t),1,fptr);
			fclose(fptr); //close file after reading
		}
		else { //append rec to end of file
			fptr=fopen(bpt_filename,"a"); //open file for append
			fwrite(&rec_i,sizeof(page_internal_t),1,fptr);
			fclose(fptr); //close file after reading
		}
	} //else internal page
	
	update_header_pg();
        
} // save_node_to_disk



void print_db (char *pathname) {
	FILE *fptr;
	//fptr=fopen(pathname,"r"); //open file for reading
	fptr=fopen("sample_10000.db","r");
	
	page_header_t rec_h; //header page
	page_leaf_t rec_l; //leaft page
	page_internal_t rec_i; //leaft page
	
	int rec_location = 0;
	int pg_size = 4096;
	int c_free = 0, c_leaf=0, c_int = 0;
	int pg_type = 1; //1: leaf, 2: internal, 3: free
	
	if (!fptr) {//if file does not exist
		printf("File is empty: %s\n", pathname);
	} 
	else {
		int i=0, k=0, eof=0, rsize = 0;
		while(!eof) {
			pg_type = 1; //default 1 - leaf
			if (!i) { //Header Page
				rsize = fread(&rec_h,4096,1,fptr);
				if (!rsize) {break;}
				printf("PgNum: %ld, FirstFreePg:%ld, RootPg:%ld, TotalPg:%ld\n", i, rec_h.first_free_pg, rec_h.root_pg, rec_h.total_pg);
			}
			else { //Internal or Leaf Page
				int rec_location = pg_size * i;
				fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
				rsize = fread(&rec_l,4096,1,fptr);
				if (!rsize) {break;}
				
				if (!rec_l.header.num_keys) {//free page
					c_free++;
					pg_type = 3;
				} 
				else {
					if (rec_l.header.is_leaf) {c_leaf++;} //leaf
					else { //internal
						c_int++;
						pg_type = 2;
					} 
				}
				
				printf("PgNum: %ld, Parent: %ld, Is_leaf: %d, Num_keys: %d, \n",i,rec_l.header.parent,rec_l.header.is_leaf,rec_l.header.num_keys);
				
				if (pg_type == 1) {
					for (k=0;k<rec_l.header.num_keys; k++) {
						printf("i:%d Key: %d, Value: %s\n",k,rec_l.rec[k].key,rec_l.rec[k].value);
					} //for
				} //if pg_type
				
			}
			i++;
			if (feof(fptr)) 
				break; 
		}
		
		printf("Leaf: %d, Internal: %d, Free: %d\n",c_leaf,c_int,c_free);
		printf("Total pages: %d\n",i);
		fclose(fptr); //close file after reading
	}
} //print_db


void build_bpt (int table_id, char *pathname) {
	FILE *fptr;
	fptr=fopen(pathname,"r"); //open file for reading
	//fptr=fopen("sample_10000.db","r");
	
	page_header_t rec_h; //header page
	page_leaf_t rec_l; //leaft page
	page_internal_t rec_i; //leaft page
	
	int rec_location = 0;
	int pg_size = 4096;
	int c_free = 0, c_leaf=0, c_int = 0;
	int pg_type = 1; //1: leaf, 2: internal, 3: free
	
	if (!fptr) {//if file does not exist
		printf("File is empty: %s\n", pathname);
	} 
	else {
		int i=0, k=0, eof=0, rsize = 0;
		while(!eof) {
			pg_type = 1; //default 1 - leaf
			if (!i) { //Header Page
				rsize = fread(&rec_h,4096,1,fptr);
				if (!rsize) {break;}
				root_pg_num = rec_h.root_pg;
				next_free_pg_num = rec_h.first_free_pg;
				total_pg_num = rec_h.total_pg;
				//printf("PgNum: %ld, FirstFreePg:%ld, RootPg:%ld, TotalPg:%ld\n", i, rec_h.first_free_pg, rec_h.root_pg, rec_h.total_pg);
			}
			else { //Internal or Leaf Page
				int rec_location = pg_size * i;
				fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
				rsize = fread(&rec_l,4096,1,fptr);
				if (!rsize) {break;}
				
				if (!rec_l.header.num_keys) {//free page
					c_free++;
					pg_type = 3;
				} 
				else {
					if (rec_l.header.is_leaf) {c_leaf++;} //leaf
					else { //internal
						c_int++;
						pg_type = 2;
					} 
				}
				
				//printf("PgNum: %ld, Parent: %ld, Is_leaf: %d, Num_keys: %d, \n",i,rec_l.header.parent,rec_l.header.is_leaf,rec_l.header.num_keys);
				
				if (pg_type == 1) {
					for (k=0;k<rec_l.header.num_keys; k++) {
						root = insert(table_id,root,rec_l.rec[k].key,rec_l.rec[k].value);
						if (rec_l.header.lsn) {add_to_pg_lsn(rec_l.rec[k].key,rec_l.header.lsn);}
						//printf("i:%d Key: %d, Value: %s\n",k,rec_l.rec[k].key,rec_l.rec[k].value);
					} //for
				} //if pg_type
				
			}
			i++;
			if (feof(fptr)) 
				break; 
		}
		
		printf("Leaf: %d, Internal: %d, Free: %d\n",c_leaf,c_int,c_free);
		printf("Total pages: %d\n",i);
		//printf("Data file successfully loaded %s\n",pathname);
		fclose(fptr); //close file after reading
	}
} //build_bpt


//Generate on-disk page queue
void print_tree_disk(node * root ) {

	FILE *fptr;
	char fn[30];
	char fpath[30];

    node * n = NULL;
    node * lc; //left child
    record * r;
    int i = 0;
    int rank = 0;
    int new_rank = 0;
    int parent_num = 0;
    int node_num = 0;
    int next_num = 0;
    
    int table_id = 0;

    if (root == NULL) {
        printf("Empty tree.\n");
        return;
    }
    queue = NULL;
    enqueue(root);
    verbose_output = 1;
    while( queue != NULL ) {
        n = dequeue();
        if (n->parent != NULL && n == n->parent->pointers[0]) {
            new_rank = path_to_root( root, n );
            if (new_rank != rank) {
                rank = new_rank;
                printf("\n");
            }
        }
        
		parent_num = 0;
		node_num = n->thisNodeNum;
		sprintf(fn, "%d", node_num); //convert int to char
		strcpy( fpath, fn ); //build filename
		strcat( fpath, ".txt" ); //build file extension
    
		fptr = fopen(fpath,"w"); //open file for writing
		fprintf(fptr,"nodenum:%d\n",n->thisNodeNum);
		fprintf(fptr,"num_keys:%d\n",n->num_keys);
        
        if (n->parent) {parent_num = n->parent->thisNodeNum;}
        for (i = 0; i < n->num_keys; i++) {            
            r = (record *)n->pointers[i];
            if (!n->is_leaf) {r = find(table_id,root, n->keys[i], 0,0);}
            if (n->pointers[i] != NULL) {
				lc = n->pointers[i]; //child
				next_num = lc->thisNodeNum;
			}
			if (n->is_leaf) {
				next_num = 0;
			}
                  
            printf("%d (Node: %d, V:%s, P:%d, LC:%d) ", n->keys[i],node_num, r->value,parent_num,next_num);
            
            fprintf(fptr,"i:%d, K:%d, V:%s, P:%d, LC:%d\n", i, n->keys[i], r->value, parent_num,next_num);
        }
        if (!n->is_leaf)
            for (i = 0; i <= n->num_keys; i++)
                enqueue(n->pointers[i]);
        
        fclose(fptr); //close file after writing
        printf("| ");
    }
    printf("\n");
} //print_tree_disk



//////////////////////////////////////////


/* Helper function for printing the
 * tree out.  See print_tree.
 */
void enqueue( node * new_node ) {
    node * c;
    if (queue == NULL) {
        queue = new_node;
        queue->next = NULL;
    }
    else {
        c = queue;
        while(c->next != NULL) {
            c = c->next;
        }
        c->next = new_node;
        new_node->next = NULL;
    }
}


/* Helper function for printing the
 * tree out.  See print_tree.
 */
node * dequeue( void ) {
    node * n = queue;
    queue = queue->next;
    n->next = NULL;
    return n;
}



/* Utility function to give the length in edges
 * of the path from any node to the root.
 */
int path_to_root( node * root, node * child ) {
    int length = 0;
    node * c = child;
    while (c != root) {
        c = c->parent;
        length++;
    }
    return length;
}


/* Prints the B+ tree in the command
 * line in level (rank) order, with the 
 * keys in each node and the '|' symbol
 * to separate nodes.
 * With the verbose_output flag set.
 * the values of the pointers corresponding
 * to the keys also appear next to their respective
 * keys, in hexadecimal notation.
 */
void print_tree( node * root ) {

    node * n = NULL;
    int i = 0;
    int rank = 0;
    int new_rank = 0;

    if (root == NULL) {
        printf("Empty tree.\n");
        return;
    }
    queue = NULL;
    enqueue(root);
    while( queue != NULL ) {
        n = dequeue();
        if (n->parent != NULL && n == n->parent->pointers[0]) {
            new_rank = path_to_root( root, n );
            if (new_rank != rank) {
                rank = new_rank;
                printf("\n");
            }
        }
        if (verbose_output) 
            printf("(%lx)", (unsigned long)n);
        for (i = 0; i < n->num_keys; i++) {
            if (verbose_output)
                printf("%lx ", (unsigned long)n->pointers[i]);
            
            printf("%d ", n->keys[i]);
        }
        if (!n->is_leaf)
            for (i = 0; i <= n->num_keys; i++)
                enqueue(n->pointers[i]);
        if (verbose_output) {
            if (n->is_leaf) 
                printf("%lx ", (unsigned long)n->pointers[order - 1]);
            else
                printf("%lx ", (unsigned long)n->pointers[n->num_keys]);
        }
        printf("| ");
    }
    printf("\n");
}


/* Finds the record under a given key and prints an
 * appropriate message to stdout.
 */
void find_and_print(int table_id, node * root, int key, bool verbose) {
    record * r = find(table_id, root, key, verbose,0);
    if (r == NULL)
        printf("Record not found under key %d.\n", key);
    else 
        printf("Record at %lx -- key %d, value %s.\n",  //2020-10-04 changed %d to %s
                (unsigned long)r, key, r->value);
}


/* Traces the path from the root to a leaf, searching
 * by key.  Displays information about the path
 * if the verbose flag is set.
 * Returns the leaf containing the given key.
 */
node * find_leaf(int table_id, node * root, int key, bool verbose, int trx_id) {
    int i = 0;
    int frame_id = 0;
    
    node * c = root;
    if (c == NULL) {
        if (verbose) 
            printf("Empty tree.\n");
        return c;
    }
    
    if (c->is_leaf) {
	    frame_id = find_in_buffer(table_id,c);
	    //printf ("in_buffer1: %d\n",frame_id);
	    if (frame_id < 1) {add_page_to_buffer(table_id,c,false);} //if not in buffer, add to it
    }
        
    while (!c->is_leaf) {
	    frame_id = find_in_buffer(table_id,c);
	    //printf ("in_buffer2: %d\n",frame_id);
	    if (frame_id < 1) {add_page_to_buffer(table_id,c,false);} //if not in buffer, add to it
	    
	    
        if (verbose) {
            printf("[");
            for (i = 0; i < c->num_keys - 1; i++)
                printf("%d ", c->keys[i]);
            printf("%d] ", c->keys[i]);
        }
        i = 0;
        while (i < c->num_keys) {
            if (key >= c->keys[i]) i++;
            else break;
        }
        if (verbose)
            printf("%d ->\n", i);
        c = (node *)c->pointers[i];
    }
    if (verbose) {
        printf("Leaf [");
        for (i = 0; i < c->num_keys - 1; i++)
            printf("%d ", c->keys[i]);
        printf("%d] ->\n", c->keys[i]);
    }
    return c;
}


/* Finds and returns the record to which
 * a key refers.
 */
record * find(int table_id, node * root, int key, bool verbose, int trx_id) {
    int i = 0;
    node * c = find_leaf(table_id, root, key, verbose, trx_id);
    if (c == NULL) return NULL;
    for (i = 0; i < c->num_keys; i++) {
		//printf("%d, key: %d\n",i,c->keys[i]);
		if (trx_id) {add_to_trx_arr(table_id, c->keys[i], trx_id,0,'R',0,0); }
		if (c->keys[i] == key) {break;}
    }
    if (i == c->num_keys) 
        return NULL;
    else
        return (record *)c->pointers[i];
} //find()


/* Finds the appropriate place to
 * split a node that is too big into two.
 */
int cut( int length ) {
    if (length % 2 == 0)
        return length/2;
    else
        return length/2 + 1;
}


// INSERTION

/* Creates a new record to hold the value
 * to which a key refers.
 */
record * make_record(char * value) {  //2020-10-04 parameter to char pointer
    record * new_record = (record *)malloc(sizeof(record));
    if (new_record == NULL) {
        perror("Record creation.");
        exit(EXIT_FAILURE);
    }
    else {
	    strcpy(new_record->value, value); //2020-10-04 assign value
	    //printf ("make record value: %s\n",new_record->value);
    	}
    return new_record;
}


/* Creates a new general node, which can be adapted
 * to serve as either a leaf or an internal node.
 */
node * make_node( void ) {
    node * new_node;
    new_node = malloc(sizeof(node));
    if (new_node == NULL) {
        perror("Node creation.");
        exit(EXIT_FAILURE);
    }
    new_node->keys = malloc( (order - 1) * sizeof(int) );
    if (new_node->keys == NULL) {
        perror("New node keys array.");
        exit(EXIT_FAILURE);
    }
    new_node->pointers = malloc( order * sizeof(void *) );
    if (new_node->pointers == NULL) {
        perror("New node pointers array.");
        exit(EXIT_FAILURE);
    }
    new_node->is_leaf = false;
    new_node->num_keys = 0;
    new_node->parent = NULL;
    new_node->next = NULL;
    globalNodeNum++;
    new_node->thisNodeNum = globalNodeNum;
    total_pg_num++;//increate pg total by one
    return new_node;
}

/* Creates a new leaf by creating a node
 * and then adapting it appropriately.
 */
node * make_leaf( void ) {
    node * leaf = make_node();
    leaf->is_leaf = true;
    return leaf;
}


/* Helper function used in insert_into_parent
 * to find the index of the parent's pointer to 
 * the node to the left of the key to be inserted.
 */
int get_left_index(node * parent, node * left) {

    int left_index = 0;
    while (left_index <= parent->num_keys && 
            parent->pointers[left_index] != left)
        left_index++;
    return left_index;
}

/* Inserts a new pointer to a record and its corresponding
 * key into a leaf.
 * Returns the altered leaf.
 */
node * insert_into_leaf(int table_id, node * leaf, int key, record * pointer ) {

    int i, insertion_point;

    insertion_point = 0;
    while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key)
        insertion_point++;

    for (i = leaf->num_keys; i > insertion_point; i--) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->pointers[i] = leaf->pointers[i - 1];
    }
    leaf->keys[insertion_point] = key;
    leaf->pointers[insertion_point] = pointer;
    leaf->num_keys++;
    
    int frame_id = find_in_buffer(table_id, leaf);
    if (frame_id < 1) {add_page_to_buffer(table_id, leaf, true);}
    else {update_page_in_buffer(frame_id, leaf);}
    
    save_node_to_disk(table_id,leaf);
    return leaf;
}


/* Inserts a new key and pointer
 * to a new record into a leaf so as to exceed
 * the tree's order, causing the leaf to be split
 * in half.
 */
node * insert_into_leaf_after_splitting(int table_id, node * root, node * leaf, int key, record * pointer) {

    node * new_leaf;
    int * temp_keys;
    void ** temp_pointers;
    int insertion_index, split, new_key, i, j;

    new_leaf = make_leaf();

    temp_keys = malloc( order * sizeof(int) );
    if (temp_keys == NULL) {
        perror("Temporary keys array.");
        exit(EXIT_FAILURE);
    }

    temp_pointers = malloc( order * sizeof(void *) );
    if (temp_pointers == NULL) {
        perror("Temporary pointers array.");
        exit(EXIT_FAILURE);
    }

    insertion_index = 0;
    while (insertion_index < order - 1 && leaf->keys[insertion_index] < key)
        insertion_index++;

    for (i = 0, j = 0; i < leaf->num_keys; i++, j++) {
        if (j == insertion_index) j++;
        temp_keys[j] = leaf->keys[i];
        temp_pointers[j] = leaf->pointers[i];
    }

    temp_keys[insertion_index] = key;
    temp_pointers[insertion_index] = pointer;

    leaf->num_keys = 0;

    split = cut(order - 1);

    for (i = 0; i < split; i++) {
        leaf->pointers[i] = temp_pointers[i];
        leaf->keys[i] = temp_keys[i];
        leaf->num_keys++;
    }

    for (i = split, j = 0; i < order; i++, j++) {
        new_leaf->pointers[j] = temp_pointers[i];
        new_leaf->keys[j] = temp_keys[i];
        new_leaf->num_keys++;
    }

    free(temp_pointers);
    free(temp_keys);

    new_leaf->pointers[order - 1] = leaf->pointers[order - 1];
    leaf->pointers[order - 1] = new_leaf;

    for (i = leaf->num_keys; i < order - 1; i++)
        leaf->pointers[i] = NULL;
    for (i = new_leaf->num_keys; i < order - 1; i++)
        new_leaf->pointers[i] = NULL;

    new_leaf->parent = leaf->parent;
    new_key = new_leaf->keys[0];
    
    save_node_to_disk(table_id,leaf);
    save_node_to_disk(table_id,new_leaf);

    return insert_into_parent(table_id, root, leaf, new_key, new_leaf);
}


/* Inserts a new key and pointer to a node
 * into a node into which these can fit
 * without violating the B+ tree properties.
 */
node * insert_into_node(int table_id,node * root, node * n, int left_index, int key, node * right) {
    int i;

    for (i = n->num_keys; i > left_index; i--) {
        n->pointers[i + 1] = n->pointers[i];
        n->keys[i] = n->keys[i - 1];
    }
    n->pointers[left_index + 1] = right;
    n->keys[left_index] = key;
    n->num_keys++;
    
    save_node_to_disk(table_id,n);
    save_node_to_disk(table_id,root);
    return root;
}


/* Inserts a new key and pointer to a node
 * into a node, causing the node's size to exceed
 * the order, and causing the node to split into two.
 */
node * insert_into_node_after_splitting(int table_id, node * root, node * old_node, int left_index, int key, node * right) {

    int i, j, split, k_prime;
    node * new_node, * child;
    int * temp_keys;
    node ** temp_pointers;

    /* First create a temporary set of keys and pointers
     * to hold everything in order, including
     * the new key and pointer, inserted in their
     * correct places. 
     * Then create a new node and copy half of the 
     * keys and pointers to the old node and
     * the other half to the new.
     */

    temp_pointers = malloc( (order + 1) * sizeof(node *) );
    if (temp_pointers == NULL) {
        perror("Temporary pointers array for splitting nodes.");
        exit(EXIT_FAILURE);
    }
    temp_keys = malloc( order * sizeof(int) );
    if (temp_keys == NULL) {
        perror("Temporary keys array for splitting nodes.");
        exit(EXIT_FAILURE);
    }

    for (i = 0, j = 0; i < old_node->num_keys + 1; i++, j++) {
        if (j == left_index + 1) j++;
        temp_pointers[j] = old_node->pointers[i];
    }

    for (i = 0, j = 0; i < old_node->num_keys; i++, j++) {
        if (j == left_index) j++;
        temp_keys[j] = old_node->keys[i];
    }

    temp_pointers[left_index + 1] = right;
    temp_keys[left_index] = key;

    /* Create the new node and copy
     * half the keys and pointers to the
     * old and half to the new.
     */  
    split = cut(order);
    new_node = make_node();
    old_node->num_keys = 0;
    for (i = 0; i < split - 1; i++) {
        old_node->pointers[i] = temp_pointers[i];
        old_node->keys[i] = temp_keys[i];
        old_node->num_keys++;
    }
    old_node->pointers[i] = temp_pointers[i];
    k_prime = temp_keys[split - 1];
    for (++i, j = 0; i < order; i++, j++) {
        new_node->pointers[j] = temp_pointers[i];
        new_node->keys[j] = temp_keys[i];
        new_node->num_keys++;
    }
    new_node->pointers[j] = temp_pointers[i];
    free(temp_pointers);
    free(temp_keys);
    new_node->parent = old_node->parent;
    for (i = 0; i <= new_node->num_keys; i++) {
        child = new_node->pointers[i];
        child->parent = new_node;
    }
    
    save_node_to_disk(table_id,new_node);
    save_node_to_disk(table_id,old_node);
    save_node_to_disk(table_id,child);

    /* Insert a new key into the parent of the two
     * nodes resulting from the split, with
     * the old node to the left and the new to the right.
     */

    return insert_into_parent(table_id, root, old_node, k_prime, new_node);
}



/* Inserts a new node (leaf or internal node) into the B+ tree.
 * Returns the root of the tree after insertion.
 */
node * insert_into_parent(int table_id, node * root, node * left, int key, node * right) {

    int left_index;
    node * parent;

    parent = left->parent;

    /* Case: new root. */

    if (parent == NULL)
        return insert_into_new_root(table_id,left, key, right);

    /* Case: leaf or node. (Remainder of
     * function body.)  
     */

    /* Find the parent's pointer to the left 
     * node.
     */

    left_index = get_left_index(parent, left);


    /* Simple case: the new key fits into the node. 
     */

    if (parent->num_keys < order - 1)
        return insert_into_node(table_id,root, parent, left_index, key, right);

    /* Harder case:  split a node in order 
     * to preserve the B+ tree properties.
     */

    return insert_into_node_after_splitting(table_id,root, parent, left_index, key, right);
}


/* Creates a new root for two subtrees
 * and inserts the appropriate key into
 * the new root.
 */
node * insert_into_new_root(int table_id, node * left, int key, node * right) {

    node * root = make_node();
    root->keys[0] = key;
    root->pointers[0] = left;
    root->pointers[1] = right;
    root->num_keys++;
    root->parent = NULL;
    left->parent = root;
    right->parent = root;
    
    root_pg_num = root->thisNodeNum;
    save_node_to_disk(table_id,root);
    return root;
}



/* First insertion:
 * start a new tree.
 */
node * start_new_tree(int table_id, int key, record * pointer) {

    node * root = make_leaf();
    root->keys[0] = key;
    root->pointers[0] = pointer;
    root->pointers[order - 1] = NULL;
    root->parent = NULL;
    root->num_keys++;
    
    root_pg_num = root->thisNodeNum;
    
    add_page_to_buffer(table_id,root,true);
    save_node_to_disk(table_id,root);
    return root;
}



/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */
node * insert(int table_id, node * root, int key, char * value) { //2020-10-04 changed 3rd parameter to char pointer
	//save_db(key,1,value,db_fname);

	//check if buffer has been initialised
	if (frame_table == NULL) {
		printf("Please initialise buffer first. Command: h BUFFER_SIZE\n");
		return 0;
	}
	
	//check if data file has been opened
	if (!table_id) {
		printf("Please open data file first. Command: o DATA_FILENAME\n");
		return 0;
	}

    record * pointer;
    node * leaf;
    node * n;
    
    //printf("Inserting K: %d, V: %d\n",key,value);

    /* The current implementation ignores
     * duplicates.
     */
	
    if (find(table_id,root, key, false,0) != NULL){
		record * temp = find(table_id,root, key, true,0);
		printf("Record at key %d already exists with value %s.\n", key, temp->value);
        return root;
    }
    /* Create a new record for the
     * value.
     */
    pointer = make_record(value);


    /* Case: the tree does not exist yet.
     * Start a new tree.
     */

    if (root == NULL) {
        return start_new_tree(table_id, key, pointer);
    }


    /* Case: the tree already exists.
     * (Rest of function body.)
     */

    leaf = find_leaf(table_id,root, key, false, 0);

    /* Case: leaf has room for key and pointer.
     */

    if (leaf->num_keys < order - 1) {
        leaf = insert_into_leaf(table_id, leaf, key, pointer);
        return root;
    }


    /* Case:  leaf must be split.
     */

    return insert_into_leaf_after_splitting(table_id, root, leaf, key, pointer);
}




// DELETION.

/* Utility function for deletion.  Retrieves
 * the index of a node's nearest neighbor (sibling)
 * to the left if one exists.  If not (the node
 * is the leftmost child), returns -1 to signify
 * this special case.
 */
int get_neighbor_index( node * n ) {

    int i;

    /* Return the index of the key to the left
     * of the pointer in the parent pointing
     * to n.  
     * If n is the leftmost child, this means
     * return -1.
     */
    for (i = 0; i <= n->parent->num_keys; i++)
        if (n->parent->pointers[i] == n)
            return i - 1;

    // Error state.
    printf("Search for nonexistent pointer to node in parent.\n");
    printf("Node:  %#lx\n", (unsigned long)n);
    exit(EXIT_FAILURE);
}


node * remove_entry_from_node(int table_id, node * n, int key, node * pointer) {

    int i, num_pointers;

    // Remove the key and shift other keys accordingly.
    i = 0;
    while (n->keys[i] != key)
        i++;
    for (++i; i < n->num_keys; i++)
        n->keys[i - 1] = n->keys[i];

    // Remove the pointer and shift other pointers accordingly.
    // First determine number of pointers.
    num_pointers = n->is_leaf ? n->num_keys : n->num_keys + 1;
    i = 0;
    while (n->pointers[i] != pointer)
        i++;
    for (++i; i < num_pointers; i++)
        n->pointers[i - 1] = n->pointers[i];


    // One key fewer.
    n->num_keys--;

    // Set the other pointers to NULL for tidiness.
    // A leaf uses the last pointer to point to the next leaf.
    if (n->is_leaf)
        for (i = n->num_keys; i < order - 1; i++)
            n->pointers[i] = NULL;
    else
        for (i = n->num_keys + 1; i < order; i++)
            n->pointers[i] = NULL;

	int frame_id = find_in_buffer(table_id,n);
	if (frame_id) {update_page_in_buffer(table_id,n);} //update buffer frame
	
	save_node_to_disk(table_id,n);    
    return n;
}


node * adjust_root(int table_id, node * root) {

    node * new_root;

    /* Case: nonempty root.
     * Key and pointer have already been deleted,
     * so nothing to be done.
     */

    if (root->num_keys > 0)
        return root;

    /* Case: empty root. 
     */

    // If it has a child, promote 
    // the first (only) child
    // as the new root.

    if (!root->is_leaf) {
        new_root = root->pointers[0];
        new_root->parent = NULL;
    }

    // If it is a leaf (has no children),
    // then the whole tree is empty.
    else
        new_root = NULL;

    free(root->keys);
    free(root->pointers);
    free(root);
    
    file_free_page(root->thisNodeNum); //free the old root page
    root_pg_num = new_root->thisNodeNum;
    total_pg_num--; //less one page
    
    int frame_id = find_in_buffer(table_id,root);
	if (frame_id) {update_page_in_buffer(table_id,root);} //update buffer frame

    return new_root;
}


/* Coalesces a node that has become
 * too small after deletion
 * with a neighboring node that
 * can accept the additional entries
 * without exceeding the maximum.
 */
node * coalesce_nodes(int table_id, node * root, node * n, node * neighbor, int neighbor_index, int k_prime) {

    int i, j, neighbor_insertion_index, n_end;
    node * tmp;

    /* Swap neighbor with node if node is on the
     * extreme left and neighbor is to its right.
     */

    if (neighbor_index == -1) {
        tmp = n;
        n = neighbor;
        neighbor = tmp;
    }

    /* Starting point in the neighbor for copying
     * keys and pointers from n.
     * Recall that n and neighbor have swapped places
     * in the special case of n being a leftmost child.
     */

    neighbor_insertion_index = neighbor->num_keys;

    /* Case:  nonleaf node.
     * Append k_prime and the following pointer.
     * Append all pointers and keys from the neighbor.
     */

    if (!n->is_leaf) {

        /* Append k_prime.
         */

        neighbor->keys[neighbor_insertion_index] = k_prime;
        neighbor->num_keys++;


        n_end = n->num_keys;

        for (i = neighbor_insertion_index + 1, j = 0; j < n_end; i++, j++) {
            neighbor->keys[i] = n->keys[j];
            neighbor->pointers[i] = n->pointers[j];
            neighbor->num_keys++;
            n->num_keys--;
        }

        /* The number of pointers is always
         * one more than the number of keys.
         */

        neighbor->pointers[i] = n->pointers[j];

        /* All children must now point up to the same parent.
         */

        for (i = 0; i < neighbor->num_keys + 1; i++) {
            tmp = (node *)neighbor->pointers[i];
            tmp->parent = neighbor;
        }
    }

    /* In a leaf, append the keys and pointers of
     * n to the neighbor.
     * Set the neighbor's last pointer to point to
     * what had been n's right neighbor.
     */

    else {
        for (i = neighbor_insertion_index, j = 0; j < n->num_keys; i++, j++) {
            neighbor->keys[i] = n->keys[j];
            neighbor->pointers[i] = n->pointers[j];
            neighbor->num_keys++;
        }
        neighbor->pointers[order - 1] = n->pointers[order - 1];
    }

    root = delete_entry(table_id, root, n->parent, k_prime, n);
    
    file_free_page(n->thisNodeNum); //free this pg
    total_pg_num--; //less one page
    
    free(n->keys);
    free(n->pointers);
    free(n); 
    
    save_node_to_disk(table_id,root);
    return root;
}


/* Redistributes entries between two nodes when
 * one has become too small after deletion
 * but its neighbor is too big to append the
 * small node's entries without exceeding the
 * maximum
 */
node * redistribute_nodes(int table_id, node * root, node * n, node * neighbor, int neighbor_index, 
        int k_prime_index, int k_prime) {  

    int i;
    node * tmp;

    /* Case: n has a neighbor to the left. 
     * Pull the neighbor's last key-pointer pair over
     * from the neighbor's right end to n's left end.
     */

    if (neighbor_index != -1) {
        if (!n->is_leaf)
            n->pointers[n->num_keys + 1] = n->pointers[n->num_keys];
        for (i = n->num_keys; i > 0; i--) {
            n->keys[i] = n->keys[i - 1];
            n->pointers[i] = n->pointers[i - 1];
        }
        if (!n->is_leaf) {
            n->pointers[0] = neighbor->pointers[neighbor->num_keys];
            tmp = (node *)n->pointers[0];
            tmp->parent = n;
            neighbor->pointers[neighbor->num_keys] = NULL;
            n->keys[0] = k_prime;
            n->parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
        }
        else {
            n->pointers[0] = neighbor->pointers[neighbor->num_keys - 1];
            neighbor->pointers[neighbor->num_keys - 1] = NULL;
            n->keys[0] = neighbor->keys[neighbor->num_keys - 1];
            n->parent->keys[k_prime_index] = n->keys[0];
        }
    }

    /* Case: n is the leftmost child.
     * Take a key-pointer pair from the neighbor to the right.
     * Move the neighbor's leftmost key-pointer pair
     * to n's rightmost position.
     */

    else {  
        if (n->is_leaf) {
            n->keys[n->num_keys] = neighbor->keys[0];
            n->pointers[n->num_keys] = neighbor->pointers[0];
            n->parent->keys[k_prime_index] = neighbor->keys[1];
        }
        else {
            n->keys[n->num_keys] = k_prime;
            n->pointers[n->num_keys + 1] = neighbor->pointers[0];
            tmp = (node *)n->pointers[n->num_keys + 1];
            tmp->parent = n;
            n->parent->keys[k_prime_index] = neighbor->keys[0];
        }
        for (i = 0; i < neighbor->num_keys - 1; i++) {
            neighbor->keys[i] = neighbor->keys[i + 1];
            neighbor->pointers[i] = neighbor->pointers[i + 1];
        }
        if (!n->is_leaf)
            neighbor->pointers[i] = neighbor->pointers[i + 1];
    }

    /* n now has one more key and one more pointer;
     * the neighbor has one fewer of each.
     */

    n->num_keys++;
    neighbor->num_keys--;
    
    save_node_to_disk(table_id,n);

    return root;
}


/* Deletes an entry from the B+ tree.
 * Removes the record and its key and pointer
 * from the leaf, and then makes all appropriate
 * changes to preserve the B+ tree properties.
 */
node * delete_entry(int table_id, node * root, node * n, int key, void * pointer ) {

    int min_keys;
    node * neighbor;
    int neighbor_index;
    int k_prime_index, k_prime;
    int capacity;

    // Remove key and pointer from node.

    n = remove_entry_from_node(table_id, n, key, pointer);

    /* Case:  deletion from the root. 
     */

    if (n == root) 
        return adjust_root(table_id, root);


    /* Case:  deletion from a node below the root.
     * (Rest of function body.)
     */

    /* Determine minimum allowable size of node,
     * to be preserved after deletion.
     */

    min_keys = n->is_leaf ? cut(order - 1) : cut(order) - 1;

    /* Case:  node stays at or above minimum.
     * (The simple case.)
     */

	if (n->num_keys >= min_keys)
		return root;

    /* Case:  node falls below minimum.
     * Either coalescence or redistribution
     * is needed.
     */

    /* Find the appropriate neighbor node with which
     * to coalesce.
     * Also find the key (k_prime) in the parent
     * between the pointer to node n and the pointer
     * to the neighbor.
     */

    neighbor_index = get_neighbor_index( n );
    k_prime_index = neighbor_index == -1 ? 0 : neighbor_index;
    k_prime = n->parent->keys[k_prime_index];
    neighbor = neighbor_index == -1 ? n->parent->pointers[1] : 
        n->parent->pointers[neighbor_index];

    capacity = n->is_leaf ? order : order - 1;

    /* Coalescence. */

    if (neighbor->num_keys + n->num_keys < capacity)
        return coalesce_nodes(table_id, root, n, neighbor, neighbor_index, k_prime);

    /* Redistribution. */

    else
        return redistribute_nodes(table_id, root, n, neighbor, neighbor_index, k_prime_index, k_prime);
}



/* Master deletion function.
 */
node * delete(int table_id, node * root, int key) {

    node * key_leaf;
    record * key_record;

    key_record = find(table_id,root, key, false,0);
    if (key_record == NULL) {
	printf("Record does not exist at key %d.\n", key);
	return root;
    }
    key_leaf = find_leaf(table_id, root, key, false, 0);

    if (key_record != NULL && key_leaf != NULL) {
        root = delete_entry(table_id, root, key_leaf, key, key_record);
        free(key_record);
    }
    
    return root;
}


void destroy_tree_nodes(node * root) {
    int i;
    if (root->is_leaf)
        for (i = 0; i < root->num_keys; i++)
            free(root->pointers[i]);
    else
        for (i = 0; i < root->num_keys + 1; i++)
            destroy_tree_nodes(root->pointers[i]);
    free(root->pointers);
    free(root->keys);
    free(root);
}


node * destroy_tree(node * root) {
    destroy_tree_nodes(root);
    return NULL;
}



//////////////////////////
/* Buffer Management */

int init_buffer (int num_buf) {
	int x;
	frame_table = malloc(num_buf * sizeof(buffer_t));
	
	if (frame_table == NULL) {
		perror("Record creation error");
		exit(EXIT_FAILURE);
	} //if buff
	
	//initialise array of buffer
	for (x = 0; x < num_buf; x++){
		frame_table[x].table_id = 0;
		frame_table[x].page_num = 0;
		frame_table[x].is_dirty = false;
		frame_table[x].is_pinned = false;
		frame_table[x].has_data = false;
		frame_table[x].refbit = false;
		
	} //for x
	
	buf_size = num_buf;
	
	printf("Buffer successfully initialised, size: %d frames\n",num_buf);

	return 0;
} //init_buffer() 


void prn_buf(buffer_t * buf) {
	int x;
	
	for (x = 0; x < buf_size; x++){
		printf("Frame: %d, ", x);
		printf("TableID: %d, ", buf[x].table_id);
		printf("PageNum: %ld, ", buf[x].page_num);
		printf("IsDirty: %d, ", buf[x].is_dirty);
		printf("IsPinned: %d, ", buf[x].is_pinned);
		printf("HasData: %d, ", buf[x].has_data);
		printf("\n");
		
	} //for x
	
	if (!buf_size) {printf("Buffer not initialsed. Please initialise buffer!\n");}
} //prn_buf()


void prn_table_id(int table_id) {
	if (frame_table == NULL) {
		printf("Please initialise buffer first. Command: h BUFFER_SIZE\n");
		return;
	}
	
	if (table_id) { //print selected table_id
		printf("Table ID: %d, Filename: %s\n", table_id,table_id_ls[table_id]);
		return;
	}
	
	//print entire table_id_ls array
	int x=0;
	for (x=1;x<=10;x++) {
		printf("Table ID: %d, Filename: %s\n", x,table_id_ls[x]);
	} //for x
} //prn_table_id


//add node to buffer
void add_page_to_buffer(int table_id, node * thisnode, bool is_dirty) {
	return;
	printf ("Add page to buffer. TableId: %d\n", table_id);
	if (!table_id) {return;} //if no table_id, return
	
	int buf_frame_id = get_buffer_frame(); //get empty frame ID, using LRU policy
	printf ("Empty frame ID: %d\n",buf_frame_id);  
	
	frame_table[buf_frame_id].table_id = table_id;
	frame_table[buf_frame_id].page_n = thisnode;
	frame_table[buf_frame_id].page_num = thisnode->thisNodeNum;
	frame_table[buf_frame_id].has_data = true;
	frame_table[buf_frame_id].is_dirty = is_dirty;
	
	printf ("Added successfully\n");
} // add_page_to_buffer


//get next availabe buffer frame ID, using LRU policy
int get_buffer_frame() {
	int empty_frame_id = 0;
	buffer_t current;
	
	printf("Buff Size: %d, Clock Hd: %d\n", buf_size, clock_hd);
	
	while (empty_frame_id == 0) {
		printf("Clock HD: %d\n", clock_hd);
		current = frame_table[clock_hd];
		if (!current.is_pinned && !current.refbit) {
			if (current.is_dirty) {write_buffer_to_disk(current);} //write page to disk, if it is updated
			
			empty_frame_id = clock_hd; //take this frame ID
			current.is_dirty = false; //reset
			current.is_pinned = true; //pinned
			current.refbit = true; //referenced
		}
		else if (!current.is_pinned && current.refbit) { //not currently accessed, but refbit is set
			current.refbit = false; //unreference this frame, if not currently accessed
		}
		
		clock_hd = (clock_hd+1)% buf_size;
	} //while
	
	return empty_frame_id;
} //get_buffer_frame()


int close_table(int table_id) {
	int x;
	int ok = 0;
	int y = 0;
	//saving from buffer to disk, if is_dirty, for the selected table_id

	for (x = 0; x < buf_size; x++){
		if (frame_table[x].table_id == table_id) {
			ok++; //to keep track if there is any frame matching the selected table_id
			if (frame_table[x].is_dirty) {
				y++; //keep track on how may frames saved to disk
				write_buffer_to_disk(frame_table[x]);
				
				//reset and free buffer frame 
				frame_table[x].table_id = 0;
				frame_table[x].page_num = 0;
				frame_table[x].has_data = false;
				frame_table[x].is_dirty = false;
				frame_table[x].is_pinned = false;
				frame_table[x].refbit = false;
			}
		 }
	} //for x
	
	if (!ok) {
		printf ("No buffer frame with TableID: %d\n", table_id);
		return 1;
	}
	else {
		printf ("Saved %d frames to disk for TableID: %d\n", y, table_id);
		strcpy(table_id_ls[table_id],""); //remove the data filename from table_id_ls array
		if (root) {destroy_tree(root);} //remove in-memory tree
	}

	return 0;
} //close_table()


int write_buffer_to_disk(buffer_t this_frame) {
	printf ("Saving buffer frame to disk: TableID: %d\n", this_frame.table_id);
	save_node_to_disk(this_frame.table_id,this_frame.page_n);
	
	return 0;
} //write_buffer_to_disk()


//check if the target page is in buffer, if found, no need to read from disk, just read from memory
int find_in_buffer(int table_id, node * thisnode) {
	return 0;
	printf ("Find PageNum: %ld in buffer\n",thisnode->thisNodeNum);  
	int x = 0;
	for (x = 0; x < buf_size; x++){
		if (frame_table[x].has_data)
			if (thisnode->thisNodeNum == frame_table[x].page_n->thisNodeNum) {return x;} //found the page in buffer, return the frame index
	} //for x
	
	printf ("PageNum: %ld not found\n",thisnode->thisNodeNum);  
	return 0;
} //find_in_buffer


//update node in buffer, after insertion/deletio 
void update_page_in_buffer(int frame_id, node * thisnode) {
	printf ("Update page in buffer. FrameId: %d\n", frame_id);
	if (!frame_id) {return;} //if no frame_id, return
		
	frame_table[frame_id].page_n = thisnode;
	frame_table[frame_id].page_num = thisnode->thisNodeNum;
	frame_table[frame_id].has_data = true;
	frame_table[frame_id].is_dirty = true;
	frame_table[frame_id].is_pinned = true;
	frame_table[frame_id].refbit = true;
	
	printf ("Updated successfully\n");
} // update_page_in_buffer


int shutdown_db(void) {
	int x;
	
	if (frame_table != NULL) {
		free(frame_table);
		frame_table = NULL;
		buf_size = 0;
		printf ("Buffer successfully destroyed\n");
	}
	
	return 0;
} //shutdown_db



/*Lock table functions */
int init_lock_table() {
	printf("init_lock_table()\n"); //debug print
	global_qid = 0;
	global_cnt = 0;
	
	if (pthread_mutex_init(&lock_table_latch, NULL) != 0){ //initialise lock latch
        printf("\n lock_table_latch init failed\n");
        return 1;
    } //if
    
    pthread_cond_init(&lock_cv, NULL); //initialise condition variable
    
    int i;
    for (i=0; i<HSIZE; i++) { //initialse hash table of size HSIZE
    	hash_table[i].id = i;
	    hash_table[i].locked = 0;
	    hash_table[i].qlen = 0;
	    hash_table[i].q = NULL;
	    //printf("initalise hash_table item: %d\n",i); //debug print
    } //for i=0

	return 0;
} // init_lock_table()


lock_t* lock_acquire(int table_id, int64_t key, int trx_id, int lock_mode) {
	global_cnt++;
	lock_t * tmp = NULL;
	int hash_num = do_hash(table_id,key);
	
	pthread_mutex_lock(&lock_table_latch);
	
	int can_proceed = 0;
	if (hash_table[hash_num].locked) {
		int has_conflict = chk_queue(hash_table[hash_num].q);
		if (!has_conflict) {can_proceed = 1;}
		if (lock_mode == 1) {can_proceed = 0;} //for Xclusive lock, just add to existing queue
	}
	else {can_proceed = 1;}

	//printf("generated hash_num: %d\n",hash_num); //debug print
	if (!can_proceed) { //has conflict
		
		locked_tcnt++;
		hash_table[hash_num].qlen++;
		printf("this element is locked: %d, q: %d, lcnt: %d\n",hash_num, hash_table[hash_num].qlen,locked_tcnt); //debug print
		
		add_to_queue(hash_table[hash_num].q,trx_id,lock_mode);
		
		//check wait-for-graph
		int dlock = chk_wfg(wgraph_hd,hash_num,trx_id);
		if (dlock) { //deadlock
			remove_last(hash_table[hash_num].q);
			
			pthread_mutex_unlock(&lock_table_latch);
			
			return NULL; 
		} //
		
		tmp = &hash_table[hash_num];
		
		pthread_cond_wait(&lock_cv, &lock_table_latch); //block - sleep
		//locked_tcnt--;
		//printf("resume thread table_id:%d, key:%ld, rc:%d, hash_num: %d, lcnt: %d\n",table_id,key,rc,hash_num, locked_tcnt);
		
	}
	else {
		//printf("not locked, just acquire and lock id:%d, trx_id:%d, lock_mode:%d\n",hash_num,trx_id,lock_mode); //debug print
		hash_table[hash_num].locked = 1;
		hash_table[hash_num].qlen++;
		add_to_queue(hash_table[hash_num].q,trx_id,lock_mode);
		tmp = &hash_table[hash_num];
	}
	
	pthread_mutex_unlock(&lock_table_latch);
	if (tmp != NULL) {return tmp;}
	return (void*) 0;
} // lock_acquire()


int lock_release(lock_t* lock_obj) {
	//printf("lock_release() id:%d, qlen:%d\n", lock_obj->id,lock_obj->qlen); //debug print
	//int rc = 0;
	pthread_mutex_lock(&lock_table_latch);
	
	//pthread_cond_broadcast(&lock_cv);
	
	if (lock_obj->qlen == 1) {
		//printf("no queue, just release id:%d\n", lock_obj->id); //debug print
		lock_obj->locked = 0;
		lock_obj->qlen = 0;
		lock_obj->q = NULL;
	}
	else {
		pthread_cond_broadcast(&lock_cv);
		printf("waking up sleeping thread qlen: %d, lcnt:%d\n",lock_obj->qlen, locked_tcnt); //debug print

		//printf("process %d items in queue\n",lock_obj->qlen); //debug print
		//prn_queue(lock_obj->q);
		process_queue(lock_obj->q);
		lock_obj->locked = 0;
		lock_obj->qlen = 0;
		//printf("queue cleared, id:%d released\n",lock_obj->id);
		//prn_queue(lock_obj->q);
	}
	
	pthread_mutex_unlock(&lock_table_latch);
	return 0;
} // lock_release()


//generate a hash key using the given table_id and key
int do_hash(int table_id,  int64_t key) {
	return (((table_id+1)*(key+1)) % HSIZE);
} //do_hash


//check queue for conflicting lock (X mode)
int chk_queue(queue_t * q) {
	queue_t * tmp;
	tmp = q;
	while (tmp != NULL) { //traverse to the end of queue
		if (tmp->lock_mode == 1) {return 1;} //Xclusive lock
		tmp = tmp->next;
	} 
    
	return 0;
} //chk_queue()



//add to the end of queue
void add_to_queue(queue_t * q, int trx_id, int lock_mode) {
	queue_t * new_q = malloc(sizeof(queue_t)); 
	global_qid++;
	new_q->qid = global_qid; 
	new_q->tid = trx_id;
	new_q->lock_mode = lock_mode;
	new_q->next = NULL;
	
	//printf("add_to_queue2: qid: %d, table_id:%d, key: %ld\n",new_q->qid,new_q->table_id,new_q->key); //debug print
    
	if (q == NULL) {
		q = new_q;
		//printf("q is NULL. Add to head. table_id:%d, key:%ld\n",q->table_id,q->key);
		return;
	} //if q
    
	queue_t * tmp;
	tmp = q;
	while (tmp->next != NULL) { tmp = tmp->next; } //traverse to the end of queue
    
	tmp->next = new_q; //add new node to the end of queue
	//printf("append to existing queue\n");
} //add_to_queue()


//add to the end of queue
void add_to_pg_lsn(pagenum_t key, pagenum_t lsn) {
	//printf("Adding LSN to Page k:%ld, lsn:%ld\n",key, lsn);
	page_lrec * new_q = malloc(sizeof(page_lrec)); 
	new_q->key = key; 
	new_q->lsn = lsn;
	new_q->next = NULL;
	   
	page_lrec * tmp;
	tmp = pg_lsn_ls;
	
	if (tmp == NULL) {
		//printf("Empty queue - start new\n");
		pg_lsn_ls = new_q;
		return;
	} //if q
	
	while (tmp->next != NULL) { //traverse to the end of queue
		if (tmp->key == key) { //existing key, just update lsn and return
			//printf("Key found - just update existing record\n");
			tmp->lsn = lsn;
			return;
		}
		tmp = tmp->next;
	} //while
	
	if (tmp->key == key) {
		//printf("Key found - just update existing record\n");
		tmp->lsn = lsn;
		return;
	}
    
	tmp->next = new_q; //add new node to the end of queue
	//printf("append to existing queue\n");
} //add_to_pg_lsn()


pagenum_t get_pg_lsn(pagenum_t key) {
	page_lrec * tmp = pg_lsn_ls;
	while(tmp != NULL) {
		if (tmp->key == key) {return tmp->lsn;}
		tmp = tmp->next; // goto next record
	} //while
	
	return 0;
} //get_pg_lsn


pagenum_t prn_pg_lsn() {
	page_lrec * q = pg_lsn_ls; 
	printf ("Print Page LSN list\n");
	int i=0;
	while(q != NULL) {
		i++;
		printf("%d k:%ld, lsn:%ld\n",i,q->key,q->lsn);
		q = q->next; // goto next record
	} //while
	
	if (!i) {printf("empty list\n");}
	
	return 0;
} //prn_pg_lsn




//remove first node from queue
void remove_head(queue_t * q) {
	//printf("remove_from_queue: qid:%d\n",q->qid); //debug print
	if (q == NULL) {return;} //empty queue
	
	// Store head queue 
	queue_t * tmp = q; 
	
	q = tmp->next; // point the head to the 2nd item in the queue
	free(tmp); // free old head
} //remove_head()


//remove last node from queue
void remove_last(queue_t * q) {
	if (q == NULL) {return;} //empty queue
	
	
	queue_t * z;
	queue_t * tmp = q; // Point to head queue
	while (tmp->next != NULL) {
		z = tmp;
		tmp = tmp->next;
		if (tmp->next == NULL) { //last node
			z->next = NULL;
			free(tmp);
		} //if
	} //while
} //remove_last()



//process the first item in the queue
void process_queue(queue_t * q) {
	//printf("process_queue()\n"); //debug print
	queue_t * tmp = q; 
	int i = 0;
	while(q != NULL) {
		tmp = q;
		q = tmp->next; // point the head to the 2nd item in the queue
		free(tmp); // free old head
		//printf("process_queue item: %d\n", i); //debug print
		i++;
	} //while
	
	return;
} //process_queue()


//print items in the queue
void prn_queue(queue_t * q) {
	//printf("print queue:\n"); //debug print
	int i = 0;
	while(q != NULL) {
		//printf("%d, ", q->qid); //debug print
		q = q->next;
		i++;
	} //while
	
	return;
} //prn_queue()



int trx_begin(void) {
	global_trx_id++;
	
	trx_t * new_t = malloc(sizeof(trx_t)); 
	if (new_t == NULL) {
        printf("New transaction initialisation error.\n");
        return 0;
    }
    
	new_t->tid  = global_trx_id; 
	new_t->lock_mode = 0; //share mode, by default
	new_t->actions = NULL;
	new_t->next = NULL;
	
	if (trx_table == NULL) {
		trx_table = new_t;
		return global_trx_id;
	}
	
	trx_t * tmp;
    tmp = trx_table; //point to the head of transaction table
    while (tmp->next != NULL) { 
		tmp = tmp->next; 
    }
    
	tmp->next = new_t; //append new record to the end of transaction table
	
	return global_trx_id;
} //trx_begin()


int trx_commit(int trx_id) {
	
	trx_t * tmp;
	trx_t * trec = NULL;
	tmp = trx_table; //point to the head of transaction table
	while (tmp != NULL) { 
	    if (tmp->tid == trx_id) { //found the target transaction record
	    	trec = tmp;
	    	break;
	    }
		tmp = tmp->next; //move to next in the list
    }
    
    if (trec != NULL) { //if found the target transaction
    	printf("trx_commit trx_id:%d\n",trx_id);
    	xact_t * xa;
    	xa = trec->actions;
    	int i = 0;
    	while (xa != NULL) { //release all locks, one a time
	    	if (xa->lock_obj) {
		    	lock_release(xa->lock_obj);
		    	//free(xa->lock_obj);
		    	i++;
		    }
	    	xa = xa->next; //move to next in the list
    	} //while
    	
    	printf("releasing tbl:%d, total_locks: %d\n",trx_id,i);
    	
    	//trec->actions = NULL; //clear away list 
    } //if trec
} //trx_commit



int trx_abort(int trx_id) {
	trx_t * tmp;
	trx_t * trec = NULL;
	tmp = trx_table; //point to the head of transaction table
	while (tmp != NULL) { 
	    if (tmp->tid == trx_id) { //found the target transaction record
	    	trec = tmp;
	    	break;
	    }
		tmp = tmp->next; //move to next in the list
    }
    
	prn_trx(trx_id);

    
    if (trec != NULL) { //if found the target transaction
    	printf("trx_abort trx_id:%d\n",trx_id);
    	xact_t * xa;
    	xa = trec->actions;
    	int i = 0;
    	while (xa != NULL) { //release all locks, one a time
    		if (xa->act_name == 'U') { //undo Update/Write
    			printf("x Tbl:%d, Key: %ld, A:%c, OV:%s, NW:%s\n", xa->table_id,xa->key,xa->act_name,xa->ori_value,xa->new_value);
    			last_lsn++;
				add_to_log(xa->key,xa->table_id,trx_id,3,last_lsn,activetrans[trx_id],xa->new_value,xa->ori_value,0);//Rollback
				
				record * r = find(xa->table_id, root, xa->key, false, trx_id);
				if (r != NULL) {  //if found
					printf("Rollback k:%ld, v:%s to ori v:%s\n",xa->key,xa->new_value,xa->ori_value);
					strcpy(r->value,xa->ori_value); //strcopy(destination, src)
					sv_page(table_id_ls[xa->table_id],xa->key,xa->ori_value,last_lsn);
				}//if r
				
				last_lsn++;
				add_to_log(xa->key,xa->table_id,trx_id,4,last_lsn,activetrans[trx_id],xa->new_value,xa->ori_value,xa->lsn); //CLR
    		}
	    	xa = xa->next; //move to next in the list
    	} //while
    	
    	flush_log(log_fname);
    	printf("Completed rollback for trx_id: %d, total actions: %d\n",trx_id,i);
    	
    } //if trec
    else {printf("Unable to find transaction %d\n",trx_id);}
} //trx_abort


int add_to_trx_arr(int table_id, int64_t key, int trx_id, int lock_mode, char act_name, char * ori_val, char * new_val) {
	trx_t * tmp;
	trx_t * trec = NULL;
	tmp = trx_table; //point to the head of transaction table
	
	//printf("add_to_trx tbl:%d, key:%ld, trx:%d, act:%c, ov:%s, nv:%s\n",table_id,key,trx_id,act_name,ori_val,new_val);
	
	//look for the target transaction
	while (tmp != NULL) { 
	    if (tmp->tid == trx_id) { //found the target transaction record
	    	trec = tmp;
	    	break;
	    }
		tmp = tmp->next; 
    } //while
	
    if (trec == NULL) {return 0;} //cannot find target transaction, return
    
    if (lock_mode > 0) {trec->lock_mode = lock_mode;} //Xclusive
    
    //allocate a new xact_t record
    xact_t * n;
	n = malloc(sizeof(xact_t));
	n->table_id = table_id;
	n->lsn = last_lsn;
	n->key = key;
	n->act_name = act_name;
	if (ori_val) {strcpy(n->ori_value,ori_val);}
	if (new_val) {strcpy(n->new_value,new_val);}
	n->next = NULL;
	
	lock_t * lock_obj = lock_acquire(table_id, key, trx_id, lock_mode); //
	if (lock_obj == NULL) {return 0;} //if locking failed - ie deadlock
	
	n->lock_obj = lock_obj;
	  
	xact_t * xa;
	xa = trec->actions;
	if (trec->actions == NULL) { //if list is empty
		trec->actions = n;
		return 1;
	}
	
	while (xa->next != NULL) { //traverse to end of list 
    	xa = xa->next; //move to next in the list
	} //while
    
	xa->next = n; //point the end of list to the new record
	
	//printf("After add_to_trx tbl:%d, key:%ld, act:%c, ov:%s, nv:%s\n",n->table_id,n->key,n->act_name,n->ori_value,n->new_value);
	
	return 1; //success
} //add_to_trx_arr()


void prn_trx(int trx_id) {
	trx_t * tmp;
	trx_t * trec = NULL;
	tmp = trx_table; //point to the head of transaction table
	
	//look for the target transaction
	while (tmp != NULL) { 
	    if (tmp->tid == trx_id) { //found the target transaction record
	    	trec = tmp;
	    	break;
	    }
		tmp = tmp->next; 
    } //while
	
    if (trec == NULL) {printf("Unable to find Transaction Record for trx_id:%d\n", trx_id);}
    else {
	    xact_t * xa;
		xa = trec->actions;
		if (trec->actions == NULL) {printf("No action for Transaction Record for trx_id:%d\n", trx_id);}
		else {
			printf("Action list for Transaction Record for id:%d\n", trx_id);
			while (xa != NULL) { //release all
				printf("Tbl:%d, Key: %ld, A:%c, OV:%s, NW:%s\n", xa->table_id,xa->key,xa->act_name,xa->ori_value,xa->new_value);
    			xa = xa->next; //move to next in the list
			}//while
		} //else
    }//else
} //prn_trx()



/*Wait-for graph functions*/
//Add node to wait-for graph
gnode_t * add_edge(gnode_t * wg, int gnode_id, int trx_id) {
	struct gnode_t * tmp = NULL;
	struct gnode_t * rec = NULL;
	int i;
	tmp = wg; //point to head of wait graph
	
	//printf("add_edge(%d,%d)\n",gnode_id,trx_id);
	
	if (tmp == NULL) {
		//printf("Start list, add new node id: %d\n", gnode_id);
		
		struct gnode_t * new_n = malloc(sizeof(gnode_t)); 
		if (new_n == NULL) {
        	printf("New node allocation error.\n");
        	return wg;
    	}
		new_n->id = gnode_id;
		new_n->next = NULL;
		new_n->w[0] = trx_id; //assign first element to process id
		//printf ("gnode_id: %d, w[0] = %d\n",gnode_id,trx_id);
		for (i=1; i<GTSIZE;i++) { //initialise wait array
			new_n->w[i] = -1; //default value
		} 
		wg = new_n;
		return wg;
	}
	
	//printf("List is not empty, start searching for node %d\n",gnode_id);
	
	while (tmp != NULL) {
		if (tmp->id == gnode_id) {
			//printf("Found node:%d\n",gnode_id);
			rec = tmp; break;
		}
		tmp = tmp->next;
	} //while
	
	if (rec) { //found target node
		//printf("Add existing node id: %d\n", gnode_id);
		for (i=0; i<GTSIZE;i++) {
			//printf("w[%d]: %d\n",i,rec->w[i]);
			if (rec->w[i] == -1) { //slot is empty
				//printf ("gnode_id: %d, w[%d] = %d\n",gnode_id,i,trx_id);
				rec->w[i] = trx_id;
				break;
			} //if
		} //for
		return wg;
	}
	
	//printf("Node not found in list\n");
	//printf("Add new node id: %d\n", gnode_id);
	tmp = wg; //point to head of wait graph
	while (tmp->next != NULL) {
		tmp = tmp->next;
	} //while
	
	struct gnode_t * new_n = malloc(sizeof(gnode_t)); 
	new_n->id = gnode_id;
	new_n->next = NULL;
	new_n->w[0] = trx_id; //assign first element to process id
	//printf ("gnode_id: %d, w[0] = %d\n",gnode_id,trx_id);
	for (i=1; i<GTSIZE;i++) { //initialise wait array
		new_n->w[i] = -1; //default value
	} 
	tmp->next = new_n;
	return wg;
} //add_edge()


//Remove node to wait-for graph, when transaction is completed
gnode_t * del_edge(gnode_t * wg, int gnode_id, int trx_id) {
	struct gnode_t * tmp = NULL;
	int i;
	tmp = wg; //point to head of wait graph
	
	if (tmp == NULL) {return wg;}
	
	while (tmp != NULL) {
		if (tmp->id == gnode_id) {
			//printf("Found node:%d\n",gnode_id);
			break;
		}
		tmp = tmp->next;
	} //while
	
	if (tmp == NULL) {return wg;} //gnode not found, return
	
	for (i=0; i<GTSIZE;i++) {
		if (tmp->w[i] == trx_id) {tmp->w[i] = -1;} //reset to -1
	} //for
	
	return wg;
	
}//del_edge()


//print wait-for graph
void prn_graph(gnode_t * wg) {
	struct gnode_t * tmp;
	int i = 0;
	int j;
	
	tmp = wg; //point to head of wait graph
	printf ("Wait-for Graph:\n");
	while (tmp != NULL) {

		printf("Node %d : ",i);
		for (j=0; j<GTSIZE;j++) {
			if(tmp->w[j] != -1) {printf("%d->%d, ",tmp->id,tmp->w[j]);}
		}
		tmp = tmp->next;
		printf("\n");
		i++;
	} //while
} //prn_graph()


bool cyclicUtil(gnode_t * hd, struct gnode_t * wg, bool * visited, bool * recStack) {
	int i;
	printf("cyclicUtil gnode_id:%d\n",wg->id);
	if (visited[wg->id] == false) {
		printf("cyclicUtil gnode_id:%d not visited\n",wg->id);
		visited[wg->id] = true;
		recStack[wg->id] = true;
		
		for (i=0; i<GTSIZE; i++) {
			printf("gnode_id: %d, resource: %d\n",wg->id, i);
			if (wg->w[i] == -1) {break;}
			struct gnode_t * tmp;
			tmp = hd;
			while (tmp != NULL) {
				if (tmp->id == wg->w[i]) {break;} //found the target rec
				tmp = tmp->next;
			} //while
			
			if (tmp == NULL) {continue;}
			printf("visit adjacent gnode_id:%d\n",tmp->w[i]);
			
			if (!visited[tmp->w[i]] && cyclicUtil(hd,tmp,visited,recStack)) {return true;}
			else if (recStack[tmp->w[i]]) {return true;}
		} //for
		
		
	} //if
	
	recStack[wg->id] = false;
	return false;
	
} //cyclicUtil()


//deteck loops (deadlock) in wait-for graph
int detect_cyclic(gnode_t * wg) {
	struct gnode_t * tmp;
	int i = 0;
	int j;
	
	bool visited[GTSIZE]; 
	bool recStack[GTSIZE]; 
	
	for(int i = 0; i < GTSIZE; i++) {
		visited[i] = false; 
		recStack[i] = false; 
	} //for 
	
	tmp = wg;
	while(tmp != NULL) {
		printf("detect_cyclic gnode_id:%d\n",tmp->id);
		if (cyclicUtil(wg,tmp,visited,recStack)) {return 1;}
		tmp = tmp->next;
	} //while
  
	return 0; 
} //detect_cyclic()


//update and check for deadlock in wait-for graph
int chk_wfg(gnode_t * wg, int resource_id, int trx_id) {
	int i, has_conflict, current_trx, is_cyclic;
	queue_t * tmp, * tmp2;
	
	printf("Check wait-for-graph: hash_num:%d, trx_id:%d\n",resource_id,trx_id);
	
	//build wfg
	for (i=0; i<HSIZE; i++) { //check hash table for locks
		if (hash_table[i].locked) { //if locked, check the queue
			tmp = hash_table[i].q;
			has_conflict = 0;
			
			while (tmp != NULL) { //traverse to the end of queue
				current_trx = tmp->tid;
				tmp2 = tmp->next; //next element in list
				
				if (tmp->lock_mode) {has_conflict = 1;} //Xclusive lock
				if (has_conflict && (tmp2 != NULL)) { //has conflict and next is not null
					wg = add_edge(wg,tmp2->tid,current_trx); //next transaction waiting for current transaction
				} //if has_conflict
				
				tmp = tmp->next; //move to next element
			} //while 
			
		} //if
	} //for i=0

	is_cyclic = detect_cyclic(wg);
	printf("is_cyclic:%d\n",is_cyclic);
	return is_cyclic;
} //chk_wfg()



/* Project 6: Crash & Recovery */

//add to log buffer, in memory
int add_to_log(pagenum_t pagenum, int table_id, int trx_id, int type, int lsn, int plsn, char * oldvalue, char * newvalue, pagenum_t next_undo_lsn) {
	//new log record
	
	//printf("Add to log tbl_id:%d, trx_id:%d, type:%d, lsn:%d, plsn:%d, ov:%s, nv:%d, clr:%ld\n",table_id,trx_id,type,lsn,plsn,oldvalue,newvalue,next_undo_lsn);
	
	log_t newlog;
	newlog.lsn = lsn;
	newlog.plsn = plsn;
	newlog.trx_id = trx_id;
	newlog.type = type;
	newlog.table_id = table_id;
	newlog.pagenum = pagenum;
	if (oldvalue) {strcpy(newlog.old_image,oldvalue);} //for Undo
	else {strcpy(newlog.old_image,"");}
	if (newvalue) {strcpy(newlog.new_image,newvalue);} //for Redo
	else {strcpy(newlog.new_image,"");}
	newlog.next_undo_lsn = next_undo_lsn; //for CLR
	
	newlog.offset = 0;
	//newlog.data_len = strlen(newvalue);
	newlog.data_len = 0;
	
	activetrans[trx_id] = lsn; //last seq no for trx_id thread
	
	//new logbuffer record
	logbuffer_t * newlgbuf = (logbuffer_t *)malloc(sizeof(logbuffer_t));
	newlgbuf->log = newlog;
	newlgbuf->next = NULL;
	
	
	//add to buffer
	if (logbuf == NULL) {
		logbuf = newlgbuf;
		return 0;
	} //if q
    
	logbuffer_t * tmp;
	tmp = logbuf;
	while (tmp->next != NULL) { tmp = tmp->next; } //traverse to the end of queue
    
	tmp->next = newlgbuf; //add new node to the end of queue
	
	prn_logbuf();
	
	return 0;
} //add_to_log()


//write log buffer to disk
int flush_log(char * log_path) {
	//write to file
	FILE *fptr;
    log_t * r;
    int i = 0;
	log_t lrec;
    
	fptr=fopen(log_path,"a"); //open file for append
	printf("Flush log to disk: %s\n", log_path);
	
	logbuffer_t * tmp;
	tmp = logbuf;
	while (tmp != NULL) { //traverse to the end of queue
		lrec = tmp->log;
		i++;
		printf("i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s\n",i, lrec.lsn,lrec.plsn,lrec.type,lrec.trx_id,lrec.pagenum,lrec.table_id,lrec.old_image,lrec.new_image);

		flushed_lsn = lrec.lsn; //keep track of the last seq no.
		fwrite(&lrec,sizeof(log_t),1,fptr); //write to disk
		tmp = tmp->next; //goto next record
	}//while 
	
	fclose(fptr); //close file after writing
	
	update_master_lsn(2); //save last lsn to disk
	
	printf("Done writing to disk\n");
	//clear log buffer in memory
	logbuf = NULL;
	return 0;
} //flush_log


void prn_logbuf() {
	printf("Print logbuf()\n");
	log_t lrec;
	logbuffer_t * tmp;
	tmp = logbuf;
	int i = 0;
	while (tmp != NULL) { //traverse to the end of queue
		i++;
		lrec = tmp->log;
		printf("i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s, crl:%ld\n",i, lrec.lsn,lrec.plsn,lrec.type,lrec.trx_id,lrec.pagenum,lrec.table_id,lrec.old_image,lrec.new_image,lrec.next_undo_lsn);
		tmp = tmp->next; //goto next record
	}//while 
} //prn_logbuf()


void init_log() {
	last_lsn = 0;
	flushed_lsn = 0;
	
	update_master_lsn(1); //read last lsn from disk

	logbuf = NULL;//log buffer in-memory
	int i;
	//initialse activetran array - this array keeps track of the last seq no for each transaction
	for (i=0;i<100;i++) {
		activetrans[i] = 0;
	} //for

	strcpy(log_fname, "logfile.data");
	//remove(log_fname); //remove old log file
	
	strcpy(logmsg_fname, "logmsg.txt");
	strcpy(db_fname, "file.data");
	
	
	int x;
	int buf_num = 1000;
	frame_table = malloc(buf_num * sizeof(buffer_t));
	
	if (frame_table == NULL) {
		perror("Record creation error");
		exit(EXIT_FAILURE);
	} //if buff
	
	//initialise array of buffer
	for (x = 0; x < buf_num; x++){
		frame_table[x].table_id = 0;
		frame_table[x].page_num = 0;
		frame_table[x].is_dirty = false;
		frame_table[x].is_pinned = false;
		frame_table[x].has_data = false;
		frame_table[x].refbit = false;
		
	} //for x
	
	buf_size = buf_num;

} //init_log



void prn_log(char * log_path) {
	FILE *fptr;
	fptr=fopen(log_path,"r"); //open file for reading
	
	log_t lrec;
	
	int rec_location = 0;
	int log_size = 288;
	int c_free = 0, c_leaf=0, c_int = 0;
	int pg_type = 1; //1: leaf, 2: internal, 3: free
	
	if (!fptr) {//if file does not exist
		printf("Log file is empty: %s\n", log_path);
	} 
	else {
		int i=0, k=0, eof=0, rsize = 0;
		
		while(fread(&lrec, sizeof(log_t), 1, fptr)) {
			i++;
			printf("i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s\n",i,lrec.lsn,lrec.plsn,lrec.type,lrec.trx_id,lrec.pagenum,lrec.table_id,lrec.old_image,lrec.new_image);
		} //while
		
		fclose(fptr); //close file after reading
	}

} //prn_log()



int init_db (int buf_num, int flag, int log_num, char* log_path, char* logmsg_path) {
	//initialise buffer frames
	//init_buffer(buf_num);
	
	//Log analysis
	FILE *fptr, *fp; //file pointers
	fptr = fopen(log_path,"r"); //open file for reading
	
	if (!fptr) {//if file does not exist
		printf("Log file is empty: %s\n", log_path);
		return 0;
	} //if
	
	log_t * lgarr = calloc(1, sizeof(log_t)); //log array in memory
	log_t lrec,lrec2; //temp log record for reading from file
	int lg_cnt = 0; //counter for log array
	int i,j,k,x;
	int total_winner,total_loser;
	pagenum_t max_lsn = 0; //the latest LSN in log file
	
	//read log record into array
	while(fread(&lrec, sizeof(log_t), 1, fptr)) {
		lg_cnt++;
		lgarr = realloc(lgarr, (lg_cnt) * sizeof(log_t));
		lgarr[lg_cnt-1] = lrec;
		//printf("i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s\n",i,lrec.lsn,lrec.plsn,lrec.type,lrec.trx_id,lrec.pagenum,lrec.table_id,lrec.old_image,lrec.new_image);
	} //while
	fclose(fptr); //close file after reading
	
	for(i=0; i<lg_cnt;i++) {
		lrec = lgarr[i];
		printf("i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s\n",i,lrec.lsn,lrec.plsn,lrec.type,lrec.trx_id,lrec.pagenum,lrec.table_id,lrec.old_image,lrec.new_image);
		max_lsn = lrec.lsn;
	} //for

	//log msg
	fp = fopen(logmsg_path,"a"); //open file for append log messages
	if (fp == NULL) {printf("Unable to open file for writing: %s\n",logmsg_path);}
	
	printf("Max LSN in logfile: %ld\n",max_lsn);
	
	fprintf(fp, "[ANALYSIS] Analysis pass start\n");
	
	//scan for winner and losers
	int * winners = calloc(1, sizeof(int)); //array of winner/complete transactions
	int * losers = calloc(1, sizeof(int)); //array of loser/incomplete transactions
	int * trx_done = calloc(1, sizeof(int));; //array of trx_id which has been processed
	int done, has_begin, has_end;
	k = 0; //keep track of trx_done length
	total_winner = 0; //keep track of winners length
	total_loser = 0; //keep track of losers length
	printf("Scan for winners and losers\n");
	for(i=0; i<lg_cnt;i++) {
		lrec = lgarr[i];
		done = 0;
		for (x=0;x<k;x++) {
			if (trx_done[x] == lrec.trx_id) {done = 1; break;}
		} //for z
		
		if (done) {continue;} //skip if this trx_id has already been processed
		k++;
		trx_done = realloc(trx_done, (k) * sizeof(int));//expand array
		trx_done[k-1] = lrec.trx_id; //add trx_id to trx_done array
		
		//scan log records for this trx_id
		printf("Scanning logs for Trx_id: %d\n",lrec.trx_id);
		has_begin = 0;
		has_end = 0;
		for(j=0; j<lg_cnt;j++) {
			lrec2 = lgarr[j];
			if (lrec2.trx_id != lrec.trx_id) {continue;} //skip if log record is not for trx_id
			else if (lrec2.type == 0) {has_begin = 1;}
			else if (lrec2.type == 2) {has_end = 1;}
		} //for j
		
		//printf("trx_id: %d, begin: %d, end: %d\n",lrec.trx_id,has_begin,has_end);
		
		if (has_begin && has_end) { //complete transaction, winner
			total_winner++; //increment winner counter by one
			winners = realloc(winners, (total_winner) * sizeof(int));//expand array
			winners[total_winner-1] = lrec.trx_id;
		}
		else {//incomplete transaction, loser
			total_loser++; //increament loser counter by one
			losers = realloc(losers, (total_loser) * sizeof(int));//expand array
			losers[total_loser-1] = lrec.trx_id;
		} //else
	} //for i
	
	fprintf(fp, "[ANALYSIS] Analysis success. Winner: ");
	//print winners and losers
	printf("Winners: ");
	for (i=0; i<total_winner; i++) {
		printf("%d, ",winners[i]);
		fprintf(fp,"%d ",winners[i]);
	}
	if (!total_winner) {
		printf("None\n");
		fprintf(fp,"None ");
	}
	
	fprintf(fp, ", Loser: ");
	printf("\nLosers: ");
	for (i=0; i<total_loser; i++) {
		printf("%d, ",losers[i]);
		fprintf(fp,"%d ",losers[i]);
	}
	if (!total_loser) {
		printf("None\n");
		fprintf(fp,"None ");
	}
	
	printf("\n");
	fprintf(fp, "\n");
	page_lrec trec;
	char db_fpath[20];
	char db_num[20];
	char default_fname[] = "data";

	
	fprintf(fp, "[REDO] Redo pass start\n");
	//loop through each winner trx_id
	int rcnt = 0; //redo counter
	for (i=0;i<total_winner;i++) {
		log_t * redo_ls = calloc(1, sizeof(log_t));
		int x = 0;
		for(j=0; j<lg_cnt;j++) {
			if (lgarr[j].trx_id != winners[i]) {continue;}
			x++;
			redo_ls = realloc(redo_ls, (x) * sizeof(log_t));//expand array
			redo_ls[x-1] = lgarr[j];
		} //for j
		
		printf("Redoing Trx_id: %d\n",winners[i]);
		int m;
		//process the redo_ls, in chronological order
		for (m=0; m<x; m++) {
			//process redo_ls[m]
			
			printf("Redo i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s, nulsn:%ld\n",m,redo_ls[m].lsn,redo_ls[m].plsn,redo_ls[m].type,
				redo_ls[m].trx_id,redo_ls[m].pagenum,redo_ls[m].table_id,redo_ls[m].old_image,redo_ls[m].new_image, redo_ls[m].next_undo_lsn);
				
			if (redo_ls[m].type == 0) {fprintf(fp, "LSN %ld [BEGIN] Transaction id %d\n", redo_ls[m].lsn,redo_ls[m].trx_id);} //begin
			else if (redo_ls[m].type == 1) { //Action: Update
			
				sprintf(db_num, "%d", redo_ls[m].table_id); //convert int to char
				strcpy(db_fpath,default_fname);
				strcat(db_fpath,db_num); //build db file name
				
				trec = fetch_page(db_fpath,redo_ls[m].pagenum);
				printf("Fetch page: k:%ld, v:%s, lsn:%ld\n",trec.key,trec.value,trec.lsn);
				if (redo_ls[m].lsn > trec.lsn) {
					fprintf(fp, "LSN %ld [UPDATE] Transaction id %d redo apply\n", redo_ls[m].lsn,redo_ls[m].trx_id);
					printf("LSN %ld [UPDATE] Transaction id %d redo apply\n", redo_ls[m].lsn,redo_ls[m].trx_id);
					sv_page(db_fpath,redo_ls[m].pagenum,redo_ls[m].new_image,redo_ls[m].lsn); //re-update
				}
				else {
					fprintf(fp, "LSN %ld [CONSIDER-REDO] Transaction id %d\n", redo_ls[m].lsn,redo_ls[m].trx_id);
					printf("LSN %ld [CONSIDER-REDO] Transaction id %d\n", redo_ls[m].lsn,redo_ls[m].trx_id);
				} //else
			} //else if
			else if (redo_ls[m].type == 2)  {fprintf(fp, "LSN %ld [COMMIT] Transaction id %d\n", redo_ls[m].lsn,redo_ls[m].trx_id);} //commit
			
			rcnt++; //increment redo counter
			if ((flag == 1)&&(rcnt >= log_num)) {break;} //terminate if log processed is/more than log_num 
		} //for m
		if ((flag == 1)&&(rcnt >= log_num)) {break;}
	} //for i
	
	fprintf(fp, "[REDO] Redo pass end\n");
	fprintf(fp, "[UNDO] Undo pass start\n");
	
	//loop through each loser trx_id
	int ucnt = 0; //undo counter
	for (i=0;i<total_loser;i++) {
		log_t * undo_ls = calloc(1, sizeof(log_t));
		int x = 0;
		for(j=0; j<lg_cnt;j++) {
			if (lgarr[j].trx_id != losers[i]) {continue;}
			x++;
			undo_ls = realloc(undo_ls, (x) * sizeof(log_t));//expand array
			undo_ls[x-1] = lgarr[j];
		} //for j
		
		printf("Undoing Trx_id: %d\n",losers[i]);
		int m;
		//process the undo_ls, in reverse order
		for (m=x-1; m>=0; m--) {
			//process undo_ls[m]
			
			printf("Undo i:%d, lsn:%ld, plsn:%ld, type:%d, trx:%d, pgno:%ld, tbl:%d, old:%s, new:%s, nulsn:%ld\n",m,undo_ls[m].lsn,undo_ls[m].plsn,undo_ls[m].type,
				undo_ls[m].trx_id,undo_ls[m].pagenum,undo_ls[m].table_id,undo_ls[m].old_image,undo_ls[m].new_image, undo_ls[m].next_undo_lsn);

			if (undo_ls[m].type == 0) {fprintf(fp, "LSN %ld [BEGIN] Transaction id %d\n", undo_ls[m].lsn,undo_ls[m].trx_id);} //begin
			else if (undo_ls[m].type == 1) { //Action: Update
				sprintf(db_num, "%d", undo_ls[m].table_id); //convert int to char
				strcpy(db_fpath,default_fname);
				strcat(db_fpath,db_num); //build db file name
				
				trec = fetch_page(db_fpath,undo_ls[m].pagenum);
				printf("Fetch page: k:%ld, v:%s, lsn:%ld\n",trec.key,trec.value,trec.lsn);
				
				fprintf(fp, "LSN %ld [UPDATE] Transaction id %d undo apply\n", undo_ls[m].lsn,undo_ls[m].trx_id);
				printf("LSN %ld [UPDATE] Transaction id %d undo apply\n", undo_ls[m].lsn,undo_ls[m].trx_id);
				sv_page(db_fpath,undo_ls[m].pagenum,undo_ls[m].old_image,undo_ls[m].lsn); //undo-update
			} //else
			else if (undo_ls[m].type == 2)  {fprintf(fp, "LSN %ld [COMMIT] Transaction id %d\n", undo_ls[m].lsn,undo_ls[m].trx_id);} //commit
			
			
			ucnt++; //increment undo counter
			if ((flag == 2)&&(ucnt >= log_num)) {break;} //terminate if log processed is/more than log_num
		} //for m
		if ((flag == 2)&&(ucnt >= log_num)) {break;}
	} //for i
	fprintf(fp, "[UNDO] Undo pass end\n");
	
	fclose(fp); //close file after writing

	return 0;
} //init_db() 


int save_db(int64_t key, int action, char * value, char * db_fname) {
	if (!db_fname) {
		printf("db_fname not provided!\n");
		return 0;
	}
	if (!key) {return 0;}
	
	//printf("sv_db k:%ld, v:%s\n",key,value);
	
	//load db file
	FILE *fptr; //file pointers
	
	page_record * rec_ls = calloc(1, sizeof(page_record));
	page_record trec, urec;
	int tcnt = 0;
	int is_found = 0;
	
	//updated record for writing
	urec.key = key;
	strcpy(urec.value,value); //strcopy(destination, src)
	
	//read log record into array
	//printf("Loading db file: %s\n",db_fname);
	if (action == 2) { //Update record
		fptr = fopen(db_fname,"r"); //open file for reading
		if (fptr != NULL) {
			while(fread(&trec, sizeof(page_record), 1, fptr)) {
				tcnt++;
				rec_ls = realloc(rec_ls, (tcnt) * sizeof(page_record));
				rec_ls[tcnt-1] = trec;
				if (trec.key == key) {is_found = 1;}
				//printf("i:%d, k:%ld, v:%s\n",tcnt,trec.key,trec.value);
			} //while
			fclose(fptr); //close file after reading
		}
		
		//if (!tcnt) {printf("File is empty\n");}
		//if (is_found) {printf("Key: %ld is found in file - will update\n",key);}
		//else {printf("Key: %ld is NOT found in file - will append\n",key);}
		
		if (is_found) {//update existing record
			int i;
			fptr = fopen(db_fname,"w"); //open file for update
			for (i=0; i<tcnt; i++) {
				trec = rec_ls[i];
				if (trec.key == key) { //update
					fwrite(&urec,sizeof(page_record),1,fptr);
				} 
				else {fwrite(&trec,sizeof(page_record),1,fptr);}
			} //for i
			fclose(fptr);
		}
	}
	else {//new record, append to end of file
		fptr = fopen(db_fname,"a"); //open file for append
		fwrite(&urec,sizeof(page_record),1,fptr); //write to disk
		fclose(fptr);
	}
	
	return 0;
} //save_db()


page_record search_db(int64_t key, char * db_fname) {
	//load db file
	FILE *fptr; //file pointers
	fptr = fopen(db_fname,"r"); //open file for reading
	
	page_record trec;
	page_record found;
	found.key = 0;
	
	//read log record into array
	//printf("Searching in db_fname\n");
	if (fptr != NULL) {
		while(fread(&trec, sizeof(page_record), 1, fptr)) {
			if (trec.key == key) {
				found = trec;
				//printf("Found k:%ld, v:%s\n",found.key,found.value);
				break;
			}
		} //while
		fclose(fptr); //close file after reading
	}

	return found;
} //search_db()



void sv_page (char *pathname, int key, char * value, pagenum_t lsn) {
	FILE *fptr;
	fptr=fopen(pathname,"r+"); //open file for reading + writing
	
	page_header_t rec_h; //header page
	page_leaf_t rec_l, urec; //leaf page
	page_internal_t rec_i; //leaft page
	
	int rec_location = 0;
	int pg_size = 4096;
	int c_free = 0, c_leaf=0, c_int = 0, found = 0;
	int pg_type = 1; //1: leaf, 2: internal, 3: free
	
	if (!fptr) {//if file does not exist
		printf("File is empty: %s\n", pathname);
		return;
	} 
	
	int i=0, k=0, z=0, eof=0, rsize = 0, written = 0;
	//printf("Open file %s for update\n",pathname);
	while(!eof) {
		found = 0;
		pg_type = 1; //default 1 - leaf
		if (!i) { //Header Page
			rsize = fread(&rec_h,4096,1,fptr);
			if (!rsize) {break;}
			root_pg_num = rec_h.root_pg;
			next_free_pg_num = rec_h.first_free_pg;
			total_pg_num = rec_h.total_pg;
		}
		else { //Internal or Leaf Page
			int rec_location = pg_size * i;
			fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
			rsize = fread(&rec_l,4096,1,fptr);
			if (!rsize) {break;}
			
			if (!rec_l.header.num_keys) {//free page
				c_free++;
				pg_type = 3;
			} 
			else {
				if (rec_l.header.is_leaf) {c_leaf++;} //leaf
				else { //internal
					c_int++;
					pg_type = 2;
				} 
			}
			
			//printf("PgNum: %ld, Parent: %ld, Is_leaf: %d, Num_keys: %d, \n",i,rec_l.header.parent,rec_l.header.is_leaf,rec_l.header.num_keys);
			
			if (pg_type == 1) {
				for (k=0;k<rec_l.header.num_keys; k++) {
					z = rec_l.rec[k].key;
					//printf("i:%d Key: %d, Value: %s\n",k,z,rec_l.rec[k].value);
					if (key == rec_l.rec[k].key) { //found the key - update
						//printf("Found i:%d Key: %d, Value: %s\n",k,rec_l.rec[k].key,rec_l.rec[k].value);
						strcpy(rec_l.rec[k].value,value); //update the value
						found = 1;
						
					} //if key
				} //for
				
				if (found) {
					//printf("Save updated record to file\n");
					rec_l.header.lsn = lsn;
					fseek(fptr, -1*sizeof(page_leaf_t), SEEK_CUR); //move pointer to previous record b4 writing
					fwrite(&rec_l, sizeof(page_leaf_t), 1, fptr); //write record to file
					written = 1;
				}
			} //if pg_type
			
		}
		i++;
		if (feof(fptr)) {break;} //end of file 
	} //while !eof
	
	if (!written) {//append to file
		urec.header.parent = 0;
		urec.header.is_leaf = 1;
		urec.header.num_keys = 1;
		urec.header.lsn = lsn;
		urec.header.sibling = 0;
		
		urec.rec[0].key = key;
		strcpy(urec.rec[0].value,value);
		
		fwrite(&urec, sizeof(page_leaf_t), 1, fptr); //write record to file
	} //
	
	//printf("Leaf: %d, Internal: %d, Free: %d\n",c_leaf,c_int,c_free);
	//printf("Total pages: %d\n",i);
	fclose(fptr); //close file after reading
} //sv_page



page_lrec fetch_page (char * pathname, int key) {
	FILE *fptr;
	
	//printf("fetch_page(), db:%s, k:%d\n",pathname,key);
	
	fptr=fopen(pathname,"r"); //open file for reading
	
	page_header_t rec_h; //header page
	page_leaf_t rec_l; //leaf page
	page_lrec trec;
	trec.lsn = 0;
	trec.key = 0;
	strcpy(trec.value,"");
	
	int rec_location = 0;
	int pg_size = 4096;
	int c_free = 0, c_leaf=0, c_int = 0, found = 0;
	int pg_type = 1; //1: leaf, 2: internal, 3: free
	
	if (!fptr) {//if file does not exist
		//printf("File is empty: %s\n", pathname);
		return trec;
	} 
	
	int i=0, k=0, eof=0, rsize = 0;
	while(!eof) {
		pg_type = 1; //default 1 - leaf
		if (!i) { //Header Page
			rsize = fread(&rec_h,4096,1,fptr);
			if (!rsize) {break;}
		}
		else { //Internal or Leaf Page
			int rec_location = pg_size * i;
			fseek(fptr,rec_location,SEEK_SET); //point file handle to the correct location
			rsize = fread(&rec_l,4096,1,fptr);
			if (!rsize) {break;}
			
			if (!rec_l.header.num_keys) {pg_type = 3;} //free page
			else if (!rec_l.header.is_leaf) {pg_type = 2;} //leaf
			
			if (pg_type == 1) {
				//printf("Numkeys: %d\n",rec_l.header.num_keys);
				for (k=0;k<rec_l.header.num_keys; k++) {
					//printf("i:%d, k:%d, v:%s\n",i,rec_l.rec[k].key,rec_l.rec[k].value);
					if (key == rec_l.rec[k].key) { //found the key - update
						trec.lsn = rec_l.header.lsn;
						trec.key = rec_l.rec[k].key;
						strcpy(trec.value,rec_l.rec[k].value);
						found = 1;
						break;
					} //if key
				} //for
			} //if pg_type
			
		}
		i++;
		if (feof(fptr)) {break;} //end of file 
	} //while !eof

	fclose(fptr); //close file after reading
	
	//printf("found: %d\n",found);
	
	return trec;
} //fetch_page


//save lsn to disk
void update_master_lsn(int action) {
	FILE *fptr;
	char pathname[]="masterlsn.no";
	pagenum_t lsn = 0;
	if (action == 1) { //read from disk
		//printf("Reading LSN from %s\n",pathname); 
		fptr=fopen(pathname,"r"); //open file for reading
		if (fptr) {
			fread(&lsn, sizeof(pagenum_t), 1, fptr);
			fclose(fptr); //close file after reading
		}
		
		//printf("LSN value: %ld\n",lsn); 
		if (lsn) {last_lsn = lsn;}
		else {last_lsn = 0;}
	} //if
	else if (action == 2) { //save to disk
		lsn = last_lsn;
		//printf("Saving LSN %ld to %s\n",lsn,pathname); 
		fptr=fopen(pathname,"w"); //open file for writing
		fwrite(&lsn, sizeof(pagenum_t), 1, fptr); //write record to file
		fclose(fptr); //close file after reading
		
	}//else
} //update_master_lsn
