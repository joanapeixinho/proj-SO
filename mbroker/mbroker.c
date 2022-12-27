#include "logging.h"


int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr, "Usage: mbroker <pipename> <max_sessions>\n");
        return -1;
    }    

    
    char* pipename = argv[1];
    int max_sessions = atoi(argv[2]);

    //TODO: implement
    printf("mbroker %s %d\n", pipename, max_sessions);

    return -1;
}
