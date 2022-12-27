#include "logging.h"

int main(int argc, char **argv) {
  
    if (argc != 3) {
        fprintf(stderr, "Usage: sub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }

    char* register_pipe = argv[1];
    char* pipe_name = argv[2];
    char* box_name = argv[3];

    printf("sub %s %s %s", register_pipe, pipe_name, box_name); //TODO: implement 
    
    return -1;
}
