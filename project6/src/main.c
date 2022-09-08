#include "file.h"
#include <stdio.h>
#include <string.h>

// MAIN

int main( int argc, char ** argv ) {

    char * input_file;
    char input_file2[20]; //bpt file
    FILE * fp;
    int input; //key
    char input2[20]; //2020-10-04, for Value
    char instruction;
    char cmd[10]; // 2020-10-04 for string instruction
    char * ret_val;
    
    int my_table_id = 0; //unique table id for opened data file
    int trx_id;
    int default_buff_sz = 100;
    int log_init = 0;
        
    root = NULL;
    frame_table = NULL;
    verbose_output = false;


    order = 32;
    
    if (argc > 1) {
        input_file = argv[1];
		printf("Command File name: %s\n", input_file);
        fp = fopen(input_file, "r");	
        if (fp == NULL) {
            perror("Failure  open input file.");
            exit(EXIT_FAILURE);
        }
	
        while (!feof(fp)) {
            fscanf(fp, "%s", cmd); // 2020-10-04
		    printf("Command: %s\n", cmd);
		    if (strcmp("insert", cmd) == 0) {
			    fscanf(fp, "%d %s", &input, input2); //2020-10-04 reads key  and value
			    printf("Key: %d, Value:%s\n", input, input2);
			    db_insert(my_table_id,input,input2);
			    print_tree(root);
		    }
		    else if (strcmp("delete", cmd) == 0) {
			    fscanf(fp, "%d", &input);
			    printf("Key: %d\n", input);
			    db_delete(my_table_id,input);
			    print_tree(root);
		    }
		    else if (strcmp("find", cmd) == 0) {
			    fscanf(fp, "%d", &input);
			    printf("Key: %d\n", input);
			    find_and_print(my_table_id, root, input, instruction == 'p');
		    }
		    else if (strcmp("open", cmd) == 0) {
			    fscanf(fp, "%s", input_file2);
			    printf("Data file name: %s\n", input_file2);
				my_table_id = open_table(input_file2);
		    }            
		}
        fclose(fp);

		//return EXIT_SUCCESS;
        print_tree(root);
    } //if argc > 1

    if (!log_init) { //if not initialised
	    init_log();
	    log_init = 1;
    }
    
    printf("> ");
    while (scanf("%c", &instruction) != EOF) {
        switch (instruction) {
	        case 'd':
	            scanf("%d", &input);
	            root = delete(my_table_id,root, input);
	            //print_tree(root);
	            break;
	        case 'i':
	            scanf("%d %s", &input, input2); //2020-10-04 reads key  and value
		    	root = insert(my_table_id,root, input, input2); //2020-10-04
		    	//printf("fname: %s\n",table_id_ls[my_table_id]);
		    	sv_page(table_id_ls[my_table_id],input,input2,last_lsn);
		    	//printf("Input2: %s\n", input2);           
	            	//print_tree(root);
	            break;
	        case 'u':
	            scanf("%d %s", &input, input2); //2020-10-04 reads key  and value
	            trx_id = trx_begin();
	            
	            last_lsn++;
	            char * tmpvalue = NULL;
	            add_to_log(input,my_table_id,trx_id,0,last_lsn,activetrans[trx_id],tmpvalue,tmpvalue,0); //begin
	            
		    	db_update(my_table_id,input,input2,trx_id);
		    	
		    	last_lsn++;
				add_to_log(input,my_table_id,trx_id,2,last_lsn,activetrans[trx_id],tmpvalue,tmpvalue,0); //end
				flush_log(log_fname);

		    	trx_commit(trx_id);
		    	
		    	//printf("Input2: %s\n", input2);           
	            break;
	        case 'f':
	        case 'p':
	            scanf("%d", &input);
	            //find_and_print(my_table_id,root, input, instruction == 'p');
	            //db_find(int table_id, int64_t key, char* ret_val, int trx_id)
	            trx_id = trx_begin();
	            db_find(my_table_id,input,ret_val,trx_id);
	            trx_commit(trx_id);
	            break;
	        case 'q': //quit
	        	close_table(my_table_id); //write dirty_frame to disk
	        	shutdown_db(); //destroy bufffer
	            while (getchar() != (int)'\n');
	            return EXIT_SUCCESS;
	            break;
	        case 't':
	            print_tree(root);
	            break;
	        case 'z':
	            print_tree_disk(root);
	            break;
	        case 'h':
	            print_db(input_file2);
	            break;
	        case 'b': //initialise buffer
	        	scanf("%d", &input);
	            init_buffer(input);
	            break;
	        case 'j': //print buffer frames - to check/verify the contents/settings of each frame in buffer
	            prn_buf(frame_table);
	            break;
	        case 'o': //open data file and assigned an ID to the file
	            scanf("%s", input2); //filename
	            printf("Opening data file name: %s\n", input2);
	            init_buffer(default_buff_sz); //initialise
				my_table_id = open_table(input2);
				if (my_table_id) {
					init_lock_table();
					printf("Data file loaded, assigned to Table ID: %d\n", my_table_id);
				}
	            break;
	        case 'k': //print table_id array - check filenames associated to each table_id
	            prn_table_id(0);
	            break;
	        case 'l': //print log file
	            prn_log(log_fname);
	            break;
	        case 'r': //recovery
	            init_db(default_buff_sz,1,100,log_fname,logmsg_fname);
	            break;
	        case 'a': //abort/rollbock
	        	scanf("%d", &input);
	            trx_abort(input);
	            break;
	        case 'e': //print trx
	        	scanf("%d", &input);
	            prn_trx(input);
	            break;
	        case 'y': //print page lsn list
	            prn_pg_lsn();
	            break;
	        case 'c': //close table
	        	scanf("%d", &input);
	            close_table(input);
	            break;
	        case 'x': //destroy buffer
	            shutdown_db();
	            break;
	        default:
	            break;
    	} //switch

        while (getchar() != (int)'\n');
        printf("> ");
    } //while
    printf("\n");

    return EXIT_SUCCESS;

} //main
