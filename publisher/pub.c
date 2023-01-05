#include "logging.h"
#include <stdbool.h>
#include <common/common.h>
#include <stdint.h>



int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: pub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }


    char* buffer = parse_mesage(OP_CODE_REGIST_PUB, argv[2], argv[3]);
    //Try to register the publisher
    write_pipe(register_pipe, buffer, sizeof(uint8_t) + (CLIENT_NAMED_PIPE_PATH_LENGTH+BOX_NAME_LENGTH)*sizeof(char));


    char* message[MESSAGE_LENGTH + 1];              //The message is composed by the op_code(+1) and the message text
    char* message_text = message + sizeof(uint8_t); //The message text starts after the op_code
    uint8_t op_code = OP_CODE_PUBLISHER;
    memcpy(message, &op_code, sizeof(uint8_t));
    size_t len = MESSAGE_LENGTH; 

    //Read from stdin until user presses ctrl+d
    while(true){
        //Each line from stdin is a message_text
        memset(message_text,0,MESSAGE_LENGTH*sizeof(char)); //Clear the message text
        if (getline(&message_text, &len, stdin) < 0) {
            printf("Failed to read line\n");
        }
        write_pipe(pipe_name, message, sizeof(uint8_t) + MESSAGE_LENGTH*sizeof(char)); 
    }
    
    return -1;
}

