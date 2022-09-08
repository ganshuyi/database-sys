#ifndef __BUFFER_H__
#define __BUFFER_H__

#define BUFFER_SIZE 256
#include "file.h"

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

        bool refbit; //LRU bit
} buffer_t;

// FUNCTIONS

extern buffer_t * frame_table;
int init_db(int num_buf);
void prn_buf(buffer_t * buf);
void prn_table_id(int this_id);
void add_page_to_buffer(int table_id, node * thisnode, bool is_dirty);
int get_buffer_frame();
int write_buffer_to_disk(buffer_t this_frame);
int find_in_buffer(int table_id, node * thisnode);
void update_page_in_buffer(int frame_id, node * thisnode);
int close_table(int table_id);
int shutdown_db(void);

#endif /* __BUFFER_H__*/

