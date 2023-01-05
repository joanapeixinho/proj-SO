#include "logging.h"
#include <stdbool.h>
#include <common/common.h>
#include <stdint.h>



int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: pub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }

    //Save the arguments in variables
    uint8_t op_code = OP_CODE_REGIST_PUB;
    char register_pipe[PIPE_NAME_LENGTH] = {0};
    char pipe_name[PIPE_NAME_LENGTH] = {0};
    char box_name[BOX_NAME_LENGTH] = {0};
    memcpy(register_pipe, argv[1], strlen(argv[1])*sizeof(char));
    memcpy(pipe_name, argv[2], strlen(argv[2])*sizeof(char));
    memcpy(box_name, argv[3], strlen(argv[3])*sizeof(char));
    
    //Serialization 
    char* buffer;
    char* tmp = buffer;
    memcpy(tmp, &op_code, sizeof(uint8_t));
    tmp += sizeof(uint8_t);                 //Move the pointer to the next position
    memcpy(tmp, pipe_name, PIPE_NAME_LENGTH*sizeof(char));
    tmp += PIPE_NAME_LENGTH*sizeof(char);   //Move the pointer to the next position
    memcpy(tmp, box_name, BOX_NAME_LENGTH*sizeof(char));

    //Try to register the publisher
    write_pipe(register_pipe, buffer, sizeof(uint8_t) + (PIPE_NAME_LENGTH+BOX_NAME_LENGTH)*sizeof(char));

    //Read from stdin until user presses ctrl+d
    char* message[MESSAGE_LENGTH + 1];              //The message is composed by the op_code(+1) and the message text
    char* message_text = message + sizeof(uint8_t); //The message text starts after the op_code
    memcpy(message, &op_code, sizeof(uint8_t));
    size_t len = MESSAGE_LENGTH; 
    while(true){
        //Each line from stdin is a message_text
        memset(message_text,0,MESSAGE_LENGTH*sizeof(char)); //Clear the message text
        if (getline(&message_text, &len, stdin) < 0) {
            printf("Failed to read line\n");
        }
        write_pipe(pipe_name, message, sizeof(uint8_t) + MESSAGE_LENGTH*sizeof(char)); );

    }
    
    return -1;
}
