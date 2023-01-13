#include "logging.h"
#include <stdbool.h>
#include "../utils/common.h"
#include <stdint.h>



int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: pub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }

    char* register_pipe = argv[1];
    char* pipe_name = argv[2];

    char* buffer = parse_message(OP_CODE_REGIST_PUB, pipe_name, argv[3]);
    int register_pipe_fd = open(register_pipe, O_WRONLY);
   
    if (register_pipe_fd < 0) {
        fprintf(stderr, "Failed to open register pipe %s\n", register_pipe);
        return -1;
    }
    //Try to register the publisher
    write_pipe(register_pipe_fd, buffer, sizeof(uint8_t) + (CLIENT_NAMED_PIPE_PATH_LENGTH+BOX_NAME_LENGTH)*sizeof(char));
   //Delete the pipe if it already exists
   
    if (unlink(pipe_name) < 0) {
        fprintf(stderr, "Failed to delete pipe %s\n", pipe_name);
        return -1;
    }
    
    //create the pipe
    if (mkfifo(pipe_name, 0777) < 0) {
        fprintf(stderr, "Failed to create pipe %s\n", pipe_name);
        return -1;
    }

    int pipe_fd = open(pipe_name, O_WRONLY);
   
    char* message[MESSAGE_LENGTH + 1];              //The message is composed by the op_code(+1) and the message text
    char* message_text = message + sizeof(uint8_t); //The message text starts after the op_code
    uint8_t op_code = OP_CODE_PUBLISHER;
    memcpy(message, &op_code, sizeof(uint8_t));
    size_t len = MESSAGE_LENGTH; 

    //Read from stdin until reaches EOF
    ssize_t bytes_written;
    while(true){
        //Each line from stdin is a message_text
        memset(message_text,0,MESSAGE_LENGTH*sizeof(char)); //Clear the message text
        if (getline(&message_text, &len, stdin) < 0) {
            printf("Failed to read line\n");
        }
        if (strcmp(message_text, EOF ) == 0) {
            safe_close(pipe_fd);
            return 0;
        }

        bytes_written =  try_write(pipe_fd, message, sizeof(uint8_t) + MESSAGE_LENGTH*sizeof(char));
        if(bytes_written < 0){ //An error occurred during writing
            return -1;
        } else if (bytes_written == 0){ //The pipe is closed
            printf("The pipe is closed. Exiting ...\n");
            return 0;
        }
    }
    return -1;
}

