#include "logging.h"
#include <stdio.h>
#include <string.h>
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
        if (strcmp(command, "create") == 0) {
            buffer = parse_mesage(OP_CODE_CREATE_BOX_ANSWER, pipe_name, box_name);
        } else {
            buffer = parse_mesage(OP_CODE_REMOVE_BOX, pipe_name, box_name);
        }

        

    } else if (strcmp(command, "list") == 0) {
        if (argc != 4) {
            print_usage();
            return -1;
        }
        buffer = parse_mesage(OP_CODE_LIST_BOXES, pipe_name, NULL);
    } else {
        print_usage();
        return -1;
    }

    return -1;
}
