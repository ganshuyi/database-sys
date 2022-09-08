#include "file.h"
#include "buffer.h"
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
    
    int my_table_id = 0; //unique table id for opened data file
        
    root = NULL;
    frame_table = NULL;
    verbose_output = false;

    order = 32;
    
    if (argc > 1) {
        input_file = argv[1];
	//printf("Command File name: %s\n", input_file);
        fp = fopen(input_file, "r");	
        if (fp == NULL) {
            perror("Failure open input file.");
            exit(EXIT_FAILURE);
        }
	
        while (!feof(fp)) {
            fscanf(fp, "%s", cmd); // 2020-10-04
	    //printf("Command: %s\n", cmd);
		    if (strcmp("insert", cmd) == 0) {
			    fscanf(fp, "%d %s", &input, input2); //2020-10-04 reads key  and value
			    //printf("Key: %d, Value:%s\n", input, input2);
			    db_insert(my_table_id,input,input2);
			    //print_tree(root);
		    }
		    else if (strcmp("delete", cmd) == 0) {
			    fscanf(fp, "%d", &input);
			    //printf("Key: %d\n", input);
			    db_delete(my_table_id,input);
			    //print_tree(root);
		    }
		    else if (strcmp("find", cmd) == 0) {
			    fscanf(fp, "%d", &input);
			    //printf("Key: %d\n", input);
			    find_and_print(my_table_id, root, input, instruction == 'p');
		    }
		    else if (strcmp("open", cmd) == 0) {
			    fscanf(fp, "%s", input_file2);
			    printf("Opening data file name: %s\n", input_file2);
			    my_table_id = open_table(input_file2);
                    	    if (my_table_id) {printf("Data file loaded, assigned to Table ID: %d\n", my_table_id);}
		    }
	   	    else if (strcmp("initialise", cmd) == 0) {
			    fscanf(fp, "%d", &input);
		    	    init_db(input);
		    }
		}
        fclose(fp);

		//return EXIT_SUCCESS;
        //print_tree(root);
    } //if argc > 1

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
		    //printf("Input2: %s\n", input2);           
	            //print_tree(root);
	            break;
	        case 'f':
	        case 'p':
	            scanf("%d", &input);
	            find_and_print(my_table_id,root, input, instruction == 'p');
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
	       	    init_db(input);
	            break;
	        case 'j': //print buffer frames - to check/verify the contents/settings of each frame in buffer
	            prn_buf(frame_table);
	            break;
	        case 'o': //open data file and assigned an ID to the file
	            scanf("%s", input2); //filename
	            printf("Opening data file name: %s\n", input2);
		    my_table_id = open_table(input2);
		    if (my_table_id) {printf("Data file loaded, assigned to Table ID: %d\n", my_table_id);}
	            break;
	        case 'k': //print table_id array - check filenames associated to each table_id
	            prn_table_id(0);
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
