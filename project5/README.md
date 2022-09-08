# **Project 5: Concurrency Control**

Modifications are made onto the previous on-disk B+ tree implementation by adding concurrency control.
<br/><br/>
Concurrency control is important to manage simultaneous operations without conflicting with each other. It allows multiple database transactions that require access to the same data to execute concurrently without violating data integrity.
<br/><br/>
In this database, a lock-based protocol is used to implement concurrency control. All transactions are unable to perform read or write operations until it acquires an appropriate lock on it. Lock requests are made to the lock manager. A strict two-phase locking mechanism is used in this protocol using shared/exclusive locks.
<br/><br/>
Deadlock detection and recovery is also implemented in this project for concurrency control. Deadlocks are detected through wait-for graphs and allocations will be rolled back to get the program into their previous safe states.
<br/>
<br/>
**Lock Manager Implementation: Data Structure**
<br/><br/>
This lock-based protocol is implemented using the following data structures.
<br/><br/>
A linked list data structure is used to maintain a list of actions. Each node holds information about the type of action: update and read. The data structure is shown as follows.
```c
typedef struct xact_t {
	char act_name; //U: update, R: read
	int table_id;
	int64_t key;
	char * ori_value;
	char * new_value;
	struct lock_t * lock_obj;
	struct xact_t * next;
} xact_t;
```
A linked list data structure is used to maintain a list of transactions. Each node holds information about its lock mode: shared and exclusive lock. The data structure is shown as follows.
```c
typedef struct trx_t {
	int tid;
	int lock_mode; //0:Shared, 1:Xclusive
	struct xact_t * actions; //list of actions for this transaction
	struct trx_t * next;
} trx_t;
```
The lock table is implemented as a hash table, where the table IDs and record IDs (key) are used to generate a unique hashing index. Each lock_t item has a queue associated to it. Every node in the queue represents the transaction which requested for lock. The queue links lock objects of multiple threads. Every new lock request for the data item will be added at the end of queue as a new node. The queue follows First In First Out order (FIFO).
<br/><br/>
The data structure of a lock table object is shown as follows:
```c
typedef struct lock_t {
	int id;
	int locked; //0:not locked, 1:locked
	int qlen;
	struct queue_t * q;
	
} lock_t;
```
The data structure of a queue is shown as follows:
```c
typedef struct queue_t {
	int qid; //queue id
	int tid; //transaction id
	int lock_mode; //0:Shared, 1:Xclusive
	struct queue_t *next;
} queue_t;
```
A linked list data structure is used to implement a wait-for graph for deadlock detection. The data structure of a graph node is shown as follows:
```c
typedef struct gnode_t { 
    int id;
    int w[GTSIZE];  
    struct gnode_t * next; 
} gnode_t;
```
<br/>

### **Compilation Method**

A Makefile is provided for compiling convenience. To compile, the following input is entered at command prompt:

    make

<br/>

### **Call Path for Database APIs Find & Update**

Find and update database operations in this program supports:
*	Conflict-serializable schedule for transactions
*	Record-level locking with shared/exclusive locks
*	Deadlock detection
*	Abort and rollback

**Finding a Key**

First, the program calls `trx_begin` function. A new transaction of type `trx_t` will be initialized. 
*	Transaction ID is assigned
*	**Shared lock** is given by default
*	A linked list of action is initialized
*	Current transaction object is linked to a null transaction object (next node)
*	If the transaction table is empty, this transaction is appointed as the head of the table (linked list) and the transaction ID is returned
*	If there is an existing transaction table, the linked list of transactions is traversed until the end and current transaction is appended to the list; transaction ID is returned

Then, `db_find` function is called. If the record is found, `add_to_trx_arr` function is called.
*	Transaction table (linked list) is traversed until target transaction is found.
*	A new action object `xact_t` is allocated. Properties of the object is initialized as per passed in function parameter.
*	A shared lock on the object is acquired by calling `lock_acquire` function.
    *	A hash number is generated with table ID and key passed as arguments.
    *   The mutex object `lock_table_latch` will be locked; if the mutex is already locked, the calling thread will be blocked until the mutex becomes available.
    *	If lock object is locked, it will be added to a queue using `add_to_queue` function.
        *	Wait-for graph is used to check for deadlocks by calling `chk_wfg` function. If a **deadlock** (cycle in wait-for graph) is detected, the last transaction is removed from queue and mutex object is released. Function is exited immediately by returning null.
        *	The lock object is put to sleep.
    *	If the lock object is not locked, the object is acquired and locked.
    *	The mutex object is released.
    *	If successful, the acquired object is returned. Else, return null.
*	If linked list of actions is empty, the current action object is made the head of the list. Else, the linked list is traversed to the end and the object will be appended to the end.
*	If successful, return 1.
*	Else, return 0; the program performs **rollback** action since deadlock is detected.
The record value of given key will be printed. After the search operation ends, the program calls `trx_commit` function.
*	Transaction table (linked list) is traversed until the target transaction is found.
*	If the target transaction is found, the locks in the linked list of actions is released one at a time by calling `lock_release` function.
    *	The mutex object `lock_table_latch` will be locked; if the mutex is already locked, the calling thread will be blocked until the mutex becomes available.
    *	If there is no queue requesting for the lock object, it will be released. 
    *	Else, wake the sleeping thread and process the items in the queue using First In First Out (FIFO) order before releasing the lock.

The mutex object is released. **Strict 2PL** holds all locks until the commit point and release them all at once.
<br/><br/>
**Updating a Key**

First, the program calls `trx_begin` function. The call path is as stated in the above description. Then, `db_update` function is called. Target record is searched. 
<br/><br/>
If record is found, `add_to_trx_arr` function is called. The call path is as described above, except that the lock mode is set to exclusive lock. If deadlock is detected when requesting for exclusive lock, rollback action is performed to return to previous safe state.
<br/><br/>
The old record value is replaced by new user-input value. If success, return 0. After the value is updated, the program calls `trx_commit` function. The call path is as described before.

<br/>

### **Summary of Program Workflow**

The diagram below shows the flowchart of this program.

![Capture](Capture.PNG)
<br/><br/>
This is a summary of the workflow of the on-disk B+ tree implemented with buffer and lock manager program.
<br/><br/>
The program is executed with the following input at command prompt:
```
./main
```
The program will then prompt for user’s input.
<br/><br/>
**Opening a datafile** 
```
o sample_10000.db
```
The program will open a datafile with a filename specified by user (e.g. sample_10000.db). It first initializes buffer of default size 1000.  Then it assigns a unique table ID to the datafile. The program then initializes the data structures required to implement a lock manager (i.e. lock table and hash table).
<br/><br/>
**Finding a key**
```
f 1000
```
The search operation requests for a **shared lock** from lock manager since it only reads and returns the record value of a given key (e.g. 1000). The program then proceeds to execute the following:
1.	Allocates and initializes a transaction structure by calling `trx_begin` function
2.	Searches for given key and updates the record value
3.	Cleans up transaction by calling `trx_commit` function

If deadlock is detected, rollback action will be performed to return to previous safe state.
<br/><br/>
**Updating a key**
```
u 1000 new_value
```
The update operation requests for an **exclusive lock** from lock manager since it writes a new value to the existing record value of a given key. The program then proceeds to execute the following:
1.	Allocates and initializes a transaction structure by calling `trx_begin` function
2.	Searches and prints for the record value of given key
3.	Cleans up transaction by calling `trx_commit` function

If deadlock is detected, rollback action will be performed to return to previous safe state. Record value of respective key will not be updated.
<br/><br/>
If user inputs `q` as command, all data stored in the buffer frames will be written to disk if the frame is dirty. The buffer frames will then be reset and freed. The in-memory tree will be destroyed, followed by the allocated buffer. The program will then be terminated. 

<br/>

### **Sample Output**

This is a rundown of how the disk-based B+ tree program is executed.
<br/><br/>
Datafile *sample_10000.db* (provided in project5 folder) is used as sample input file for this program.
<br/><br/>
User inputs `./main` at command prompt. 
<br/><br/>
The following image shows the output of the program.

![output1](output1.PNG) <br/>
![output2](output2.PNG)

The program opens and processes datafile *sample_10000.db*. Then it receives user input. Command `f 1000` returns the key and its value 1000 is printed. Command `u 1000 new_value` updates the key with specified new value. Using command `f 1000`, the program searches said key and returns the updated value.
<br/><br/>
Command `q` is entered, and the program proceeds to termination. Exit is successful.
