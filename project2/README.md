# **Project 2: Disk-based B+ Tree**

### **Project Specifications**

1. General Definition
- A Page is equivalent to a Tree Node
- One Page size is 4096 bytes
- One Page is made up of
    - 1 Page Header record (128 bytes)
    - a number of entries
- For Leaf Page:
    - up to 31 records
    - each Record is made up of key(8) + value(120), 128 bytes in size
- For Internal Page:
    - up to 248 entries
    - each Entry is made up of key(8) + page number (8), 16 bytes in size


2.  Types of Data Page
- Header Page <br/>
    This is the first page in current data file. The data structure consists of:
    - first free page number
    - root page number
    - total number of pages in current data file
- Free Page <br/>
    This is made up of 1 Page Header record.
    - the first field stores the next free page number
    - 0 value for this field if it is the last page on the list
- Leaf Page <br/>
    This page is made up of 1 Page Header (128 bytes) and up to 31 Records.
    - Each record is 128 bytes in size; key (8 bytes) + value (120 bytes)
    - The keys are stored in sorted order
- Internal Page <br/>
    This page is made up of 1 Page Header (128 bytes) and up to 248 entries.
    - Each entry is 16 bytes in size; key (8 bytes) + page number (8 bytes)


3.  Page Header Data Structure <br/>
    This data structure (128 bytes in size) is made up of:
    - Parent Page Number (integer)
        - for Internal/Leaf pages, this stores the parent page number
        - for Root page, this value is 0
        - for Free page, this stores the next free page number
    - Is Leaf (boolean)
    - Number of keys currently stored in this page (integer)
    - Reserved
    - Right Sibling Page Number
        - on Leaf page, this stores Right Sibling Page Number
            - for the last (rightmost) Leaf page, this value is 0
        - on Internal page, this stores the first (leftmost) child page number

<br/>

### **Compilation Method**

A Makefile is provided for compiling convenience. To compile, the following input is entered at command prompt:

    make

<br/>

### **Call Path of Insert or Delete Operation & Detail Flow of Structure Modification**

Program execution:
```
./main command_file
```

An example of **command_file** may look like the following;
```c
open pathname
insert 1 John
insert 2 Tim
insert 3 Mary
insert 4 Joe
insert 5 Dan
find 3
delete 5
```

Load and parse **command_file**
*  open
   *  int open_table (char ***pathname**)
   *  open existing data file using the given **pathname**
      * load Header Page into memory
      * load existing pages, creating in-memory B+ Tree
      * return the Root page number
   *  if files do not exist, create a new file and name it as **my_data_file** <br />
* insert
   *  int db_insert (int64_t **root**, int64_t **key**, char ***value**)
   *  insert record (key + value) to data file at the right page
      *  if root is empty, start a new tree
         * allocate a free page
         * create a new page if no free page is available
         * assign record (key + value) to new page
         * increment *key_count* by one
      *  if root is not empty, i.e. tree already exists
         * find leaf page
         * if entries on the leaf page is less than *maxrec*
           * insert record to this leaf
         * if entries on the leaf page reaches *maxrec*
           * split this leaf page
             * allocate a new page for a new root
             * make the old root a child of the new root
             * split the old root page into 2 pages, and move the middle key to the new root
           * insert record into leaf page
           * update root page number
   *  if insertion is successful, return 0
   *  else, return non-zero value to indicate error <br />
*  delete
   *  int db_delete (int64_t **root**, int64_t **key**)
   *  find the record matching the given **key** 
   *  if found, perform deletion
      *  remove record from page
      *  if current page is the root
         * if *key_num* > 0, do nothing and return 0
         * if *key_num* = 0,
           * free root page and update free page list
           * update Header Page –clear root page number
           * return 0
      *  if current page is not root
         * check the minimum number of records for a page
         * if total records in current page is at least the minimum number, do nothing and return 0
         * if total records in current page is less than the minimum number
           * decide either to merge or redistribute with neighbor page
           * check the number of records in the left neighbor page
           * if the total number of records in neighbor and current page is less than *maxrec*, do page merging
             * if current page is on the extreme left, swap current page with neighbor's page
             * get record insertion point
             * if current page is internal page/non-leaf page, append neighbor's records to current page
             * if current page is leaf page, append current page records to neighbor's page
             * update *num_keys*
             * return 0
           * if the total number of records in neighbor and current page is equal to/more than *maxrec*, do page redistribution
             * if current page has a left neighbor
               * take left neighbor's rightmost record and insert into current page's left end
             * if current page is the leftmost page (have no left neighbor)
               * take right neighbor's leftmost record and insert into current page's right end
             * update *num_keys* for both current and neighbor pages
             * return 0
   *  if deletion is successful, return 0
   *  else, return non-zero value to indicate error

<br/>

### **Required Changes for Building On disk B+ Tree**

**Data Structure Changes for Disk-based B+ Tree**
<br />
```c
typedef uint64_t pagenum_t;

// A Page/Node
typedef struct page_t {
     struct page_hd header;
     
     int maxrec; // maximum records/entries in this page
     struct entry * ent; // internal page/node
     struct record * rec; // leaf page/node

     // for Header Page only
     pagenum_t first_free_pg;
     pagenum_t root_pg;
} page_t;

// Page Header
typedef struct page_hd {
     pagenum_t parent; // parent (internal/leaf page/node) or next free page number (for header page)
     bool is_leaf; // checks if this is leaf page
     int num_keys; // total keys in this page or total pages for this data file
     pagenum_t sibling; // right sibling page number/ first child page number/ root page number (for header page)
} page_hd;

// Record in Leaf Page/Node
typedef struct record {
     int key;
     char value;
} record;

// Entry in Internal Page/Node
typedef struct entry {
     int key;
     pagenum_t pagenum;
} record;

```

**On-disk B+ Tree Design**

1.  When program is executed
    *  if data file is empty
       * create a header page
       * initialize the header page
    *  if data file has data
       * read the header page
       * build in-memory B+ tree based on the data file
       * return root page number<br />

2.  Insert record
    *  insert record into the correct page, in-memory B+ tree
    *  if current page's total record hits the *maxrec*, split accordingly and update on-disk data file<br />

3.  Delete record
    *  delete record from the appropriate page, in the in-memory B+ tree
    *  check the number of keys in current page, merge only when page is full (delayed merge implementation)
    *  update on-disk data file accordingly<br />

4.  Find
    *  find the *key* and print the stored value, in the in-memory B+ tree<br />

5.  Quit
    *  save in-memory B+ tree to on-disk data file

<br/>

### **Disk based B+ Tree Final Report**

Figure below shows the workflow of disk-based B+ tree.

![flowchart_final_](diag.PNG)

<br/>

**Summary of Workflow**
<br/>
The program is executed with the following input at command prompt:
```c
./main <filename>
```
The program will proceed to open the datafile. If the datafile is empty, the program will create and initialize the header page of a B+ tree for said datafile. If the file contains data in it, the program will read the header page and build an in-memory B+ tree based on the contents of the data file. It then returns the root page number of the B+ tree.
<br/><br/>
The program will execute commands that are written in the datafile.
<br/><br/>
**Inserting a record**
<br/>
The program will first perform search to make sure the key does not exist. Duplicates are not allowed in this program. If the key does not already exist in the tree, then it will add the record into the correct page in the in-memory B+ tree. If the current page’s total record reaches its maximum, it will split accordingly and update the on-disk data file.
<br/><br/>
**Deleting a record**
<br/>
The program will first perform search to find the record to be deleted. If the record exists in the tree, it will be deleted from its page in the in-memory B+ tree. Delayed merge is implemented in this program. The program will check the number of keys on current page. The program will update the on-disk datafile only if the page is full or if it is totally empty.
<br/><br/>
**Finding a record**
<br/>
The program will perform a search to find the record in the B+ tree. If the record is found, the program will print the key and its stored value in the in-memory B+ tree.
<br/><br/>
When the program reaches the end of the datafile provided, it will proceed to take the user’s manual input to execute more commands (i.e. insert, delete, find, etc.). 
<br/><br/>
If user inputs `q` as command, the program will be terminated.
<br/>


### **Disk based B+ Tree Sample Output**

This is a rundown of how the disk-based B+ tree program is executed.
<br/>
The following text file named `file1.txt` is used as a sample input file for the disk-based B+ tree program.
```
open bpt_data.db
insert 5 John
insert 6 Mary
insert 4 Dave
insert 7 Peter
insert 10 Ali
insert 2 Fiona
insert 8 Dan
insert 1 Harry
insert 3 Lou
insert 5 Will
delete 1
find 10
find 1
find 5
delete 1
```
User inputs the following command at command prompt:
```
./main file1.txt
```
The following image shows the output of the program.
<br/>

![output1](output1.PNG)
![output2](output2.PNG)
<br/>

After the program finished processing the text file, program will ask for user input to modify the B+ tree. After inputting `q`, the program is terminated.



