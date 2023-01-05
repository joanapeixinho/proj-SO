#include "logging.h"
#include <stdbool.h>
#include <common/common.h>
#include <stdint.h>

int main(int argc, char **argv) {
  
    if (argc != 3) {
        fprintf(stderr, "Usage: sub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }

    int message_count = 0;

    char* register_pipe = argv[1];
   
    char* buffer = parse_mesage(OP_CODE_REGIST_SUB, argv[2], argv[3]);
    int register_pipe_fd = open(register_pipe, O_WRONLY);
    if (register_pipe_fd < 0) {
        fprintf(stderr, "Failed to open register pipe %s\n", register_pipe);
        return -1;
    }
    //Try to register the subscriber
    write_pipe(register_pipe_fd, buffer, sizeof(uint8_t) + (CLIENT_NAMED_PIPE_PATH_LENGTH+BOX_NAME_LENGTH)*sizeof(char));
    
    //Write in stdout until user presses ctrl+d
    u_int8_t op_code;
    char* message[MESSAGE_LENGTH + 1];      //The message has max 255 chars + a '\0' at the end
    size_t len = MESSAGE_LENGTH; 
    while(true){
        //Each message from the pipe is a new line
        read_pipe(pipe_name, &op_code, sizeof(uint8_t));
        read_pipe(pipe_name, message, MESSAGE_LENGTH*sizeof(char));
        printf("%s\n", message);

    }

    return -1;
}
