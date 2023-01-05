#include "logging.h"
#include <stdio.h>
#include <string.h>
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
        if (argc != 4) {
            print_usage();
            return -1;
        }
        char* box_name = argv[4];
        if (strcmp(command, "create") == 0) {
            printf("create %s ", box_name); //TODO implement create
        } else {
            printf("remove %s ", box_name); //TODO implement remove
        }
    } else if (strcmp(command, "list") == 0) {
        if (argc != 3) {
            print_usage();
            return -1;
        }
        printf("list"); //TODO implement list
    } else {
        print_usage();
        return -1;
    }
    return -1;
}
