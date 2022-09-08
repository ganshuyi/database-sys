/* buffer.c */

#include "file.h"
#include "buffer.h"
#include <string.h>


buffer_t * frame_table = NULL; //Buffer Manager

int buf_size = 0; //Size of buffer
//int last_table_id = 0; //ID assigned to each opened table, between 1 to 10;
int clock_hd = 0; //clock hand for LRU policy
char table_id_ls[11][20]; //array of table id to store opened filenames


// FUNCTION DEFINITIONS


int init_db(int num_buf) {
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
} //init_db()


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
        //printf ("Add page to buffer. TableId: %d\n", table_id);
        if (!table_id) {return;} //if no table_id, return

        int buf_frame_id = get_buffer_frame(); //get empty frame ID, using LRU policy
        //printf ("Empty frame ID: %d\n",buf_frame_id);

        frame_table[buf_frame_id].table_id = table_id;
        frame_table[buf_frame_id].page_n = thisnode;
        frame_table[buf_frame_id].page_num = thisnode->thisNodeNum;
        frame_table[buf_frame_id].has_data = true;
        frame_table[buf_frame_id].is_dirty = is_dirty;

        //printf ("Added page to buffer successfully\n");
} // add_page_to_buffer



//get next availabe buffer frame ID, using LRU policy
int get_buffer_frame() {
        int empty_frame_id = 0;
        buffer_t current;
        
        while (empty_frame_id == 0) {
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
                if (y < 2) {printf ("Saved %d frame to disk for TableID: %d\n", y, table_id);}
		else {printf ("Saved %d frames to disk for TableID: %d\n", y, table_id);}
                strcpy(table_id_ls[table_id],""); //remove the data filename from table_id_ls array
                if (root) {destroy_tree(root);} //remove in-memory tree
        }

        return 0;
} //close_table()


int write_buffer_to_disk(buffer_t this_frame) {
        //printf ("Saving buffer frame to disk: TableID: %d\n", this_frame.table_id);
        save_node_to_disk(this_frame.table_id,this_frame.page_n);

        return 0;
} //write_buffer_to_disk()


//check if the target page is in buffer, if found, no need to read from disk, just read from memory
int find_in_buffer(int table_id, node * thisnode) {

        //printf ("Find PageNum: %d in buffer\n",thisnode->thisNodeNum);
        int x = 0;
        for (x = 0; x < buf_size; x++){
                if (frame_table[x].has_data)
                        if (thisnode->thisNodeNum == frame_table[x].page_n->thisNodeNum) {return x;} //found the page in buffer, return the frame index
        } //for x

        //printf ("PageNum: %d not found\n",thisnode->thisNodeNum);
        return 0;
} //find_in_buffer


//update node in buffer, after insertion/deletio 
void update_page_in_buffer(int frame_id, node * thisnode) {
        //printf ("Update page in buffer. FrameId: %d\n", frame_id);
        if (!frame_id) {return;} //if no frame_id, return
                
        frame_table[frame_id].page_n = thisnode;
        frame_table[frame_id].page_num = thisnode->thisNodeNum;
        frame_table[frame_id].has_data = true;
        frame_table[frame_id].is_dirty = true;
        frame_table[frame_id].is_pinned = true;
        frame_table[frame_id].refbit = true;
        
        printf ("Updated page in buffer successfully\n");
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

