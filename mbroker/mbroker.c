#include "logging.h"


int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr, "Usage: mbroker <pipename> <max_sessions>\n");
        return -1;
    }    

    int max_sessions = atoi(argv[2]);
    // add server init with max_sessions

    if (tfs_init() != 0) {
        printf("Failed to init tfs\n");
        return EXIT_FAILURE;
    }
    
    char* pipename = argv[1];

    //TODO: implement
    printf("mbroker %s %d\n", pipename, max_sessions);

    return -1;
}
