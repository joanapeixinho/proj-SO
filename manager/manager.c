#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <common/common.h>

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}

int main(int argc, char **argv) {
    if (argc < 4) {
        print_usage();
        return -1;
    }

    char* register_pipe_name = argv[1];
    char* pipe_name = argv[2];
    char* command = argv[3];
    char* buffer;

    if (strcmp(command, "create") == 0 || strcmp(command, "remove") == 0) {
        if (argc != 5) {
            print_usage();
            return -1;
        }
        char* box_name = argv[4];
        uint8_t return_op_code;
        int32_t return_code;
        char error_message[MESSAGE_LENGTH + 1];

        if (strcmp(command, "create") == 0) {
            buffer = parse_mesage(OP_CODE_CREATE_BOX_ANSWER, pipe_name, box_name);
        } else {
            buffer = parse_mesage(OP_CODE_REMOVE_BOX, pipe_name, box_name);
        }
        write_pipe(register_pipe_name, buffer, sizeof(uint8_t) + (CLIENT_NAMED_PIPE_PATH_LENGTH+BOX_NAME_LENGTH)*sizeof(char));
        
        //read [] code | return_code | error_message ] from mbroker
        read_pipe(pipe_name, &return_op_code, sizeof(uint8_t));
        read_pipe(pipe_name, &return_code, sizeof(int32_t));
        read_pipe(pipe_name, error_message, MESSAGE_LENGTH*sizeof(char));
        error_message[MESSAGE_LENGTH] = '\0';

        if (return_code == 0) {
            fprintf(stdout, "OK\n");
            return 0;
        } else {
            fprintf(stderr, "ERROR: %s\n", error_message);
            return -1;
        }

    } else if (strcmp(command, "list") == 0) {
        if (argc != 4) {
            print_usage();
            return -1;
        }
        buffer = parse_mesage(OP_CODE_LIST_BOXES, pipe_name, NULL);
        write_pipe(register_pipe_name, buffer, sizeof(uint8_t) + CLIENT_NAMED_PIPE_PATH_LENGTH*sizeof(char));
    } else {
        print_usage();
        return -1;
    }

    return -1;
}
