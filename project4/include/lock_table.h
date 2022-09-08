#ifndef __LOCK_TABLE_H__
#define __LOCK_TABLE_H__

#include <stdint.h>
#include <pthread.h>

//Size of hash table
#define HSIZE 1000

//Types


typedef struct queue_t {
	int qid;
	int table_id;
	int64_t key;
	struct queue_t *next;
} queue_t;


typedef struct lock_t {
	int id;
	int locked;
	int qlen;
	struct queue_t * q;
} lock_t;



/* APIs for lock table */
int init_lock_table();
lock_t* lock_acquire(int table_id, int64_t key);
int lock_release(lock_t* lock_obj);
int do_hash(int table_id,  int64_t key);
void add_to_queue(queue_t * q, int table_id, int64_t key);
void remove_from_queue(queue_t * q);
void process_queue(queue_t * q);
void prn_queue(queue_t * q);

// GLOBALS
extern lock_t hash_table[HSIZE];
extern pthread_mutex_t lock_table_latch;
extern pthread_cond_t my_cv;
extern int global_qid;
extern int global_cnt;

#endif /* __LOCK_TABLE_H__ */
