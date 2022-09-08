# **Project 3: Buffer Management**

Modifications are made onto the previous on-disk B+ tree implementation by adding a buffer management layer. 
<br/><br/>
The main functions of buffer manager are to speed up data processing and increase efficiency. It reads disk pages into a main memory as needed and makes calls to the file manager to perform these functions on disk pages.
<br/><br/>
The data structure of the buffer manager is as shown:
```c
typedef struct buffer_t {
        struct node * page_n; 
        struct page_internal_t page_i;
        struct page_leaf_t page_l;
        int table_id; 
        pagenum_t page_num; 
        bool is_dirty; 
        bool is_pinned; 
        bool has_data; 
        bool refbit;
} buffer_t;
```
<br/>

**Design Overview of Buffer Manager**
<br/><br/>
The buffer manager manages a buffer pool: a collection of page-sized buffer frames. The buffer pool is stored as an array `frame_table[buf_size]` of `buffer_t` objects. Each buffer frame is a `buffer_t` object that contains:
*	the unique table ID of loaded data file
*	target page number within data file
*	dirty bit (whether the block is updated or not)
*	pinned bit (whether the block is accessed at the moment or not)
*	data bit (whether the block has data or is empty)
*	reference bit (LRU bit)

When a page is requested for pinning, the buffer manager will check buffer pool to see if any buffer block contains the requested page or not. If the page is not available in the frames, it should be brought in as follows:
<br/>
1.  A frame is chosen to be replaced using the clock replacement policy. The least recently used page is selected; checked by LRU bit. <br/>
2.  If the frame chosen for replacement is dirty, its contents is written to disk using the appropriate file manager API. <br/>
3.  The requested page is read by file manager into the frame chosen for replacement. The pinned bit and dirty bit for the frame are both set to false. <br/>
4.  The entry of the old page is removed from the buffer pool, updated with an entry for the new page. The frame_table array is updated accordingly. <br/>
5.  The requested page for this frame is pinned (set pinned bit to true). The ID of said buffer frame is returned. <br/>
<br/>

### **Compilation Method**

A Makefile is provided for compiling convenience. To compile, the following input is entered at command prompt:

    make

<br/>

### **Summary of Program Workflow**

Diagram below shows a detailed program workflow.
<br/><br/>
![Capture](diag.PNG)
<br/><br/>
This is a summary of the workflow of the on-disk B+ tree implemented with buffer manager program.
<br/><br/>
The program is executed with the following input at command prompt:
```c
./main <filename>
```

The program will initialize the buffer to a size specified by user (eg. 10). Then it will proceed to open the datafile. A unique table ID will be assigned to the data file. If the datafile is empty, the program will create and initialize the header page of a B+ tree for said datafile. If the file contains data in it, the program will read the header page and build an in-memory B+ tree based on the contents of the data file. It then returns the unique table ID of the file.
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
When the program reaches the end of the datafile provided, it will proceed to take the user’s manual input to execute more commands (i.e. insert, delete, find, print tree, etc.). 
<br/><br/>
If user inputs `q` as command, all data stored in the buffer frames will be written to disk if the frame is dirty. The buffer frames will then be reset and freed. The in-memory tree will be destroyed, followed by the allocated buffer. The program will then be terminated.

<br/>

### **Sample Output**

This is a rundown of how the disk-based B+ tree (with buffer management layer implemented) program is executed.
<br/><br/>
The following text file named **file1.txt** is used as a sample input file for the disk-based B+ tree program.
``` .txt file
initialise 10
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
User inputs the following at command prompt:
```
./main file1.txt
```

The following image shows the output of the program.
<br/><br/>
![output](output.PNG)
<br/><br/>
The program processes the text file by first initializing buffer to 10 frames, then opens a datafile named **bpt_data.db** (data file will be created if it does not exist). A table ID is assigned to the data file, in this case table ID is 1. Then, the program carries out insert and delete commands in the text file. Buffer is updated accordingly. Subsequent find and delete commands are carried out by the program.
<br/><br/>
After that, the program receives user input. Command `t` prints the in-memory B+ tree. Key 11 with value “test” is inserted into the B+ tree. Using command `f 11`, key 11 is found and its value ("test") is printed. The command `t` is inputted again, and the updated B+ tree is printed. Command `j` shows the status of the current buffer frames of buffer layer. At the moment, only frame 1 is used.
<br/><br/>
Command `q` is entered, and the program proceeds to termination. All dirty frames are written to disk and the allocated buffer is destroyed. Exit is successful.

