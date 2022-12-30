#include "logging.h"
#include <stdbool.h>
#include <common/common.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: pub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }

    char* register_pipe = argv[1];
    char* pipe_name = argv[2];
    char* box_name = argv[3];

    //TODO: Try to register the publisher



    //Read from stdin until user presses ctrl+d
    char* message[MESSAGE_LENGTH + 1] = {0};
    size_t len = MESSAGE_LENGTH;
    while(true){
        //Each line from stdin is a message
        if (getline(&message, &len, stdin) < 0) {
            printf("Failed to read line\n");
        }
        write_pipe(pipe_name, OP_CODE_PUBLISHER, sizeof(int) );
        write_pipe(pipe_name, message, MESSAGE_LENGTH * sizeof(char));
    }

    // TODO: finish implementing
    printf("pub %s %s %s", register_pipe, pipe_name, box_name);
    


    return -1;
}
