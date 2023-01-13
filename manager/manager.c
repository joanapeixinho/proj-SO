#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <../utils/common.h>
//include mbroker headerfile
#include "../mbroker/mbroker.h"

int list_boxes(int pipefd);

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

    int register_pipefd = open(register_pipe_name, O_WRONLY);
    if (register_pipefd < 0) {
        fprintf(stderr, "Error opening register pipe %s for writing\n", register_pipe_name);
        return -1;
    }

    int pipefd = open(pipe_name, O_RDONLY);
    if (pipefd < 0) {
        fprintf(stderr, "Error opening pipe %s for reading\n", pipe_name);
        return -1;
    }

    if (strcmp(command, "create") == 0 || strcmp(command, "remove") == 0) {
        if (argc != 5) {
            print_usage();
            close(register_pipefd);
            close(pipefd);
            return -1;
        }
        char* box_name = argv[4];
        uint8_t return_op_code;
        int32_t return_code;
        char error_message[MESSAGE_LENGTH + 1];

        if (strcmp(command, "create") == 0) {
            buffer = parse_message(OP_CODE_CREATE_BOX_ANSWER, pipe_name, box_name);
        } else {
            buffer = parse_message(OP_CODE_REMOVE_BOX, pipe_name, box_name);
        }
       
        write_pipe(register_pipefd, buffer, sizeof(uint8_t) + (CLIENT_NAMED_PIPE_PATH_LENGTH+BOX_NAME_LENGTH)*sizeof(char));
        
        read_pipe(pipefd, &return_op_code, sizeof(uint8_t));
        read_pipe(pipefd, &return_code, sizeof(int32_t));
        read_pipe(pipefd, error_message, MESSAGE_LENGTH*sizeof(char));
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
            close(register_pipefd);
            close(pipefd);
            return -1;
        }
        buffer = parse_message(OP_CODE_LIST_BOXES, pipe_name, NULL);
        write_pipe(register_pipefd, buffer, sizeof(uint8_t) + CLIENT_NAMED_PIPE_PATH_LENGTH*sizeof(char));
        if (list_boxes(pipefd) == -1 ) {
            close(register_pipefd);
            close(pipefd);
            return -1;
        }
        return 0;
    } else {
        print_usage();
        close(register_pipefd);
        close(pipefd);
        return -1;
    }

    close(register_pipefd);
    close(pipefd);
    return -1;
}

int list_boxes (int pipefd) {

        uint8_t op_code;
        uint8_t last = 0;
        size_t count = 0;
        box_t boxes[MAX_BOXES];
       
        while (last == 0) {
            
            read_pipe(pipefd, &op_code, sizeof(uint8_t));
            if (op_code != OP_CODE_LIST_BOXES_ANSWER) {
                fprintf(stderr, "ERROR: Unexpected operation code %d in list_boxes\n", op_code);
                return -1;
            }
            read_pipe(pipefd,&last, sizeof(uint8_t));
            if (count == 0 && last == 1 ){
                fprintf(stdout, "NO BOXES FOUND\n");
                return -1;
            } 
            read_pipe(pipefd, boxes[count].box_name, BOX_NAME_LENGTH*sizeof(char));
            boxes[count].box_name[BOX_NAME_LENGTH] = '\0';
            read_pipe(pipefd, &boxes[count].box_size, sizeof(uint64_t));
            read_pipe(pipefd, &boxes[count].n_publishers, sizeof(uint64_t));
            read_pipe(pipefd, &boxes[count].n_subscribers, sizeof(uint64_t));
            count++;
        }
        
        //sort boxes by name
        qsort(boxes, count, sizeof(box_t), compare_boxes);

        //print boxes
        for (int i = 0; i < count; i++) {
            fprintf(stdout, "%s %zu %zu %zu\n", boxes[i].box_name, boxes[i].box_size, boxes[i].n_publishers, boxes[i].n_subscribers);
        }

        return 0;
}

int compare_boxes(const void* box_a, const void* box_b) {
    box_t* a = (box_t*) box_a;
    box_t* b = (box_t*) box_b;
    return strcasecmp(a->box_name, b->box_name);
}
