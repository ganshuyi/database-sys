#include "file.h"
//#include "file.h"
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
        
    root = NULL;
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
	
        while (1) {
	    fscanf(fp, "%s", cmd); // 2020-10-04
	    if (feof(fp))
		    break; //exit while loop
	    printf("Command: %s\n", cmd);
	    if (strcmp("insert", cmd) == 0) {
		    fscanf(fp, "%d %s", &input, input2); //2020-10-04 reads key  and value
		    printf("Key: %d, Value:%s\n", input, input2);
		    db_insert(input,input2);
		    print_tree(root);
	    }
	    else if (strcmp("delete", cmd) == 0) {
		    fscanf(fp, "%d", &input);
		    printf("Key: %d\n", input);
		    db_delete(input);
		    print_tree(root);
	    }
	    else if (strcmp("find", cmd) == 0) {
		    fscanf(fp, "%d", &input);
		    printf("Key: %d\n", input);
		    char ret_val[120];
		    if (db_find(input, ret_val) == -1)
			    printf("Record not found under key %d.\n", input);
		    else
			    printf("Record found -- key %d, value %s.\n", input, ret_val);
		    //find_and_print(root, input, instruction == 'p');
	    }
	    else if (strcmp("open", cmd) == 0) {
		    fscanf(fp, "%s", input_file2);
		    printf("Data file name: %s\n", input_file2);
	   	    open_table(input_file2);
	    }
    
	}
        fclose(fp);

	//return EXIT_SUCCESS;
	printf("Current tree:\n");
        print_tree(root);
    } //if argc > 1

    printf("> ");
    while (scanf("%c", &instruction) != EOF) {
        switch (instruction) {
	        case 'd':
	            scanf("%d", &input);
	            root = delete(root, input);
	            print_tree(root);
	            break;
	        case 'i':
	            scanf("%d %s", &input, input2); //2020-10-04 reads key  and value
		    	root = insert(root, input, input2); //2020-10-04
		    	//printf("Input2: %s\n", input2);           
	            print_tree(root);
	            break;
	        case 'f':
	        case 'p':
	            scanf("%d", &input);
	            find_and_print(root, input, instruction == 'p');
	            break;
	        case 'q':
	            while (getchar() != (int)'\n');
	            return EXIT_SUCCESS;
	            break;
	        case 't':
	            print_tree(root);
	            break;
	        case 'z':
	            print_tree_disk(root);
	            break;
	        case 'b':
	            print_db(input_file2);
	            break;
	        case 'c':
	            build_bpt(input_file2);
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
