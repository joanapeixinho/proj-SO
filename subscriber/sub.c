#include "logging.h"
#include <stdbool.h>
#include <common/common.h>
#include <stdint.h>

int main(int argc, char **argv) {
  
    if (argc != 4) {
        fprintf(stderr, "Usage: sub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }

    char* register_pipe = argv[1];
    char* pipe_name = argv[2];
    char* box_name = argv[3];

    printf("sub %s %s %s", register_pipe, pipe_name, box_name); //TODO: implement 
    
    //Write in stdout until user presses ctrl+d
    u_int8_t op_code;
    char* message[MESSAGE_LENGTH + 1];      //The message has max 255 chars + a '\0' at the end
    size_t len = MESSAGE_LENGTH; 
    while(true){
        //Each message from the pipe is a new line
        read_pipe(pipe_name, &op_code, sizeof(uint8_t));
        read_pipe(pipe_name, message, MESSAGE_LENGTH*sizeof(char));
        fprintf(stdout, "%s\n", message);

    }

    return -1;
}
