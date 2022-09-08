/* file.c  */

#include "file.h"
#include <string.h>

// GLOBALS.
page_t * pg_queue = NULL;
int order = DEFAULT_ORDER;
int globalNodeNum = 0;
node * queue = NULL;
node * root = NULL;
pagenum_t root_pg_num = 0;
pagenum_t next_free_pg_num = 0;
pagenum_t total_pg_num = 0;
char * bpt_filename;
bool verbose_output = false;

// FUNCTION DEFINITIONS.

// OUTPUT AND UTILITIES

/* Open existing data file using �pathname� or create one if not existed.
 * If success, return the unique table id, which represents the own table in this database.
 * Otherwise, return negative value.
 */
int open_table (char *pathname) {
	FILE *fptr;
	bpt_filename = pathname;
	fptr=fopen(pathname,"r"); //open file for reading
	
	page_header_t rec; //header page
	
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
		build_bpt(pathname);
		//fread(&rec,sizeof(page_header_t),1,fptr);
		//next_free_pg_num = rec.first_free_pg;
		//root_pg_num = rec.root_pg;
		//total_pg_num = rec.total_pg;
	
		printf("FirstFreePg:%ld, RootPg:%ld, TotalPg:%ld\n",next_free_pg_num,root_pg_num,total_pg_num);
		fclose(fptr); //close file after reading
	}

	return 0;
} //open_table


/* Insert input �key/value� (record) to data file at the right place. 
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_insert (int64_t key, char * value) {
	//printf ("v: %s\n", value);
	root = insert(root, key, value); //2020-10-04
	return 0;
} //db_insert


/* Find the record containing input �key�.
 * If found matching �key�, store matched �value� string in ret_val and return 0.
 * Otherwise, return non-zero value
 */
int db_find (int64_t key, char * ret_val) {
	record * r = find(root, key, true);
	if (r == NULL)
		return -1;
	else {
		strcpy(ret_val, r->value);
		printf("in db_find, ret_val: %s\n", ret_val);
		return 0;
	}	
} //db_find


/* Find the matching record and delete it if found
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_delete (int64_t key) {
	root = delete(root, key);
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


void save_node_to_disk(node * thisnode) {
	
	if (!bpt_filename) {return;} //if no data file name provided
    
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
				printf("PgNum: %d, FirstFreePg:%ld, RootPg:%ld, TotalPg:%ld\n", i, rec_h.first_free_pg, rec_h.root_pg, rec_h.total_pg);
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
				
				printf("PgNum: %d, Parent: %ld, Is_leaf: %d, Num_keys: %d, \n",i,rec_l.header.parent,rec_l.header.is_leaf,rec_l.header.num_keys);
				
				if (pg_type == 1) {
					for (k=0;k<rec_l.header.num_keys; k++) {
						printf("i:%d Key: %ld, Value: %s\n",k,rec_l.rec[k].key,rec_l.rec[k].value);
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


void build_bpt (char *pathname) {
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
				printf("PgNum: %d, FirstFreePg:%ld, RootPg:%ld, TotalPg:%ld\n", i, rec_h.first_free_pg, rec_h.root_pg, rec_h.total_pg);
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
						root = insert(root,rec_l.rec[k].key,rec_l.rec[k].value);
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
		fclose(fptr); //close file after reading
	}
} //build_bpt


//Generate on-disk page queue
void print_tree_disk( node * root ) {

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
            if (!n->is_leaf) {r = find(root, n->keys[i], 0);}
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
void find_and_print(node * root, int key, bool verbose) {
    record * r = find(root, key, verbose);
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
node * find_leaf( node * root, int key, bool verbose ) {
    int i = 0;
    node * c = root;
    if (c == NULL) {
        if (verbose) 
            printf("Empty tree.\n");
        return c;
    }
    while (!c->is_leaf) {
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
record * find( node * root, int key, bool verbose ) {
    int i = 0;
    node * c = find_leaf( root, key, verbose );
    if (c == NULL) return NULL;
    for (i = 0; i < c->num_keys; i++)
        if (c->keys[i] == key) break;
    if (i == c->num_keys) 
        return NULL;
    else
        return (record *)c->pointers[i];
}

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
node * insert_into_leaf( node * leaf, int key, record * pointer ) {

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
    
    save_node_to_disk(leaf);
    return leaf;
}


/* Inserts a new key and pointer
 * to a new record into a leaf so as to exceed
 * the tree's order, causing the leaf to be split
 * in half.
 */
node * insert_into_leaf_after_splitting(node * root, node * leaf, int key, record * pointer) {

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
    
    save_node_to_disk(leaf);
    save_node_to_disk(new_leaf);

    return insert_into_parent(root, leaf, new_key, new_leaf);
}


/* Inserts a new key and pointer to a node
 * into a node into which these can fit
 * without violating the B+ tree properties.
 */
node * insert_into_node(node * root, node * n, int left_index, int key, node * right) {
    int i;

    for (i = n->num_keys; i > left_index; i--) {
        n->pointers[i + 1] = n->pointers[i];
        n->keys[i] = n->keys[i - 1];
    }
    n->pointers[left_index + 1] = right;
    n->keys[left_index] = key;
    n->num_keys++;
    
    save_node_to_disk(n);
    save_node_to_disk(root);
    return root;
}


/* Inserts a new key and pointer to a node
 * into a node, causing the node's size to exceed
 * the order, and causing the node to split into two.
 */
node * insert_into_node_after_splitting(node * root, node * old_node, int left_index, int key, node * right) {

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
    
    save_node_to_disk(new_node);
    save_node_to_disk(old_node);
    save_node_to_disk(child);

    /* Insert a new key into the parent of the two
     * nodes resulting from the split, with
     * the old node to the left and the new to the right.
     */

    return insert_into_parent(root, old_node, k_prime, new_node);
}



/* Inserts a new node (leaf or internal node) into the B+ tree.
 * Returns the root of the tree after insertion.
 */
node * insert_into_parent(node * root, node * left, int key, node * right) {

    int left_index;
    node * parent;

    parent = left->parent;

    /* Case: new root. */

    if (parent == NULL)
        return insert_into_new_root(left, key, right);

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
        return insert_into_node(root, parent, left_index, key, right);

    /* Harder case:  split a node in order 
     * to preserve the B+ tree properties.
     */

    return insert_into_node_after_splitting(root, parent, left_index, key, right);
}


/* Creates a new root for two subtrees
 * and inserts the appropriate key into
 * the new root.
 */
node * insert_into_new_root(node * left, int key, node * right) {

    node * root = make_node();
    root->keys[0] = key;
    root->pointers[0] = left;
    root->pointers[1] = right;
    root->num_keys++;
    root->parent = NULL;
    left->parent = root;
    right->parent = root;
    
    root_pg_num = root->thisNodeNum;
    save_node_to_disk(root);
    return root;
}



/* First insertion:
 * start a new tree.
 */
node * start_new_tree(int key, record * pointer) {

    node * root = make_leaf();
    root->keys[0] = key;
    root->pointers[0] = pointer;
    root->pointers[order - 1] = NULL;
    root->parent = NULL;
    root->num_keys++;
    
    root_pg_num = root->thisNodeNum;
    save_node_to_disk(root);
    return root;
}



/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */
node * insert( node * root, int key, char * value ) { //2020-10-04 changed 3rd parameter to char pointer

    record * pointer;
    node * leaf;
    node * n;
    
    //printf("Inserting K: %d, V: %d\n",key,value);

    /* The current implementation ignores
     * duplicates.
     */
	
    if (find(root, key, false) != NULL){
		record * temp = find(root, key, true);
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
        return start_new_tree(key, pointer);
    }


    /* Case: the tree already exists.
     * (Rest of function body.)
     */

    leaf = find_leaf(root, key, false);

    /* Case: leaf has room for key and pointer.
     */

    if (leaf->num_keys < order - 1) {
        leaf = insert_into_leaf(leaf, key, pointer);
        return root;
    }


    /* Case:  leaf must be split.
     */

    return insert_into_leaf_after_splitting(root, leaf, key, pointer);
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


node * remove_entry_from_node(node * n, int key, node * pointer) {

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

	save_node_to_disk(n);    
    return n;
}


node * adjust_root(node * root) {

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

    return new_root;
}


/* Coalesces a node that has become
 * too small after deletion
 * with a neighboring node that
 * can accept the additional entries
 * without exceeding the maximum.
 */
node * coalesce_nodes(node * root, node * n, node * neighbor, int neighbor_index, int k_prime) {

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

    root = delete_entry(root, n->parent, k_prime, n);
    
    file_free_page(n->thisNodeNum); //free this pg
    total_pg_num--; //less one page
    
    free(n->keys);
    free(n->pointers);
    free(n); 
    
    if (n->num_keys == 31) //2020-10-12
    	save_node_to_disk(root);

    return root;
}


/* Redistributes entries between two nodes when
 * one has become too small after deletion
 * but its neighbor is too big to append the
 * small node's entries without exceeding the
 * maximum
 */
node * redistribute_nodes(node * root, node * n, node * neighbor, int neighbor_index, 
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
    
    save_node_to_disk(n);

    return root;
}


/* Deletes an entry from the B+ tree.
 * Removes the record and its key and pointer
 * from the leaf, and then makes all appropriate
 * changes to preserve the B+ tree properties.
 */
node * delete_entry( node * root, node * n, int key, void * pointer ) {

    int min_keys;
    node * neighbor;
    int neighbor_index;
    int k_prime_index, k_prime;
    int capacity;

    // Remove key and pointer from node.

    n = remove_entry_from_node(n, key, pointer);

    /* Case:  deletion from the root. 
     */

    if (n == root) 
        return adjust_root(root);


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
        return coalesce_nodes(root, n, neighbor, neighbor_index, k_prime);

    /* Redistribution. */

    else
        return redistribute_nodes(root, n, neighbor, neighbor_index, k_prime_index, k_prime);
}



/* Master deletion function.
 */
node * delete(node * root, int key) {

    node * key_leaf;
    record * key_record;

    key_record = find(root, key, false);
    if (key_record == NULL) {
	printf("Record does not exist at key %d.\n", key);
	return root;
    }
    key_leaf = find_leaf(root, key, false);

    if (key_record != NULL && key_leaf != NULL) {
        root = delete_entry(root, key_leaf, key, key_record);
        free(key_record);
    }
    
    return root;
}
