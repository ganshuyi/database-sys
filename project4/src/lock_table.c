#include <lock_table.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

lock_t hash_table[HSIZE]; //hash array of HSIZE
pthread_mutex_t lock_table_latch = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lock_cv = PTHREAD_COND_INITIALIZER; //condition variable

int global_qid = 0;
int global_cnt = 0;
int locked_tcnt = 0;


//struct lock_t {
//	/* NO PAIN, NO GAIN. */
//};

//typedef struct lock_t lock_t;

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


lock_t* lock_acquire(int table_id, int64_t key) {
	global_cnt++;
	//int rc = 0;
	//printf("execution no: %d\n",global_cnt);  //debug print
	//printf("lock_acquire(): table_id:%d, key: %ld\n",table_id,key); //debug print
	lock_t * tmp = NULL;
	
	int hash_num = do_hash(table_id,key);
	
	pthread_mutex_lock(&lock_table_latch);

	//printf("generated hash_num: %d\n",hash_num); //debug print
	if (hash_table[hash_num].locked) {
		
		locked_tcnt++;
		hash_table[hash_num].qlen++;
		//printf("this element is locked: %d, q: %d, lcnt: %d\n",hash_num, hash_table[hash_num].qlen,locked_tcnt); //debug print
		//if (locked_tcnt >= 8) {
		//	rc = pthread_cond_broadcast(&lock_cv);
		//	printf("broadcast to wake threads %d, lcnt: %d\n", rc, locked_tcnt);
		//}
		
		add_to_queue(hash_table[hash_num].q,table_id,key);
		tmp = &hash_table[hash_num];
		
		//rc = pthread_cond_wait(&lock_cv, &lock_table_latch); //block
		//locked_tcnt--;
		//printf("resume thread table_id:%d, key:%ld, rc:%d, hash_num: %d, lcnt: %d\n",table_id,key,rc,hash_num, locked_tcnt);
		
	}
	else {
		//printf("not locked, just acquire and lock id:%d\n",hash_num); //debug print
		hash_table[hash_num].locked = 1;
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
	
	if (lock_obj->qlen == 0) {
		//printf("no queue, just release id:%d\n", lock_obj->id); //debug print
		lock_obj->locked = 0;
		lock_obj->qlen = 0;
	}
	else {
		//rc = pthread_cond_broadcast(&lock_cv);
		//printf("waking up sleeping thread qlen: %d, lcnt:%d\n",lock_obj->qlen, locked_tcnt); //debug print
		//pthread_cond_signal(&lock_cv); 

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


//add to the end of queue
void add_to_queue(queue_t * q, int table_id, int64_t key) {
	queue_t * new_q = malloc(sizeof(queue_t)); 
	global_qid++;
	new_q->qid  = global_qid; 
	new_q->table_id = table_id;
	new_q->key = key;
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


//remove from the head of the queue
void remove_from_queue(queue_t * q) {
	//printf("remove_from_queue: qid:%d\n",q->qid); //debug print
	if (q == NULL) {return;} //empty queue
	
	// Store head queue 
	queue_t * tmp = q; 
	
	q = tmp->next; // point the head to the 2nd item in the queue
	free(tmp); // free old head
} //remove_from_queue()


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
