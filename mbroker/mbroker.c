#include "logging.h"
#include "mbroker.h"
#include <fcntl.h>
#include <stdbool.h>

static int max_sessions;
static int fdserver;
static char *pipename;

int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr, "Please specify mbroker pipename and number of max sessions\n");
        return -1;
    }    

    max_sessions = atoi(argv[2]);

    // add server init with max_sessions

    if (tfs_init() != 0) {
        printf("Failed to init tfs\n");
        return -1;
    }
    
    pipename = argv[1];

    if (unlink(pipename) == -1 ) {
        printf("Failed to unlink pipe %s\n", pipename);
        return -1;
    }

    if (mkfifo(pipename, 0777) < 0) {
        printf("Failed to create pipe %s\n", pipename);
        return -1;
    }
    
    if (fdserver = open(pipename, O_RDONLY ) < 0 ) {
        printf("Failed to open server pipe %s\n", pipename);
        unlink(pipename);
        return -1;
    }

    while (true) {
        /* Open and close a temporary pipe to avoid having active wait for another
         * process to open the pipe. The 'open' function blocks until the pipe
         * is openned on the other side, therefore doing exactly what we want.
         */

        int pipe_temp = open(pipename, O_RDONLY);

        if (pipe_temp < 0) {
            printf("Failed to open pipe %s\n", pipename);
            close_server();
            return -1;
        }

        if (close(pipe_temp) < 0) {
            printf("Failed to close pipe %s\n", pipename);
            close_server();
            return -1;
        }

        ssize_t bytes_read;
        char op_code;

        bytes_read = read(fdserver, &op_code, sizeof(char));

        while (bytes_read > 0) {
            if (bytes_read < 0) {
            printf("Failed to read from pipe %s\n", pipename);
            close_server();
            return -1;
            }

            switch (op_code) {
                case OP_CODE_REGIST_PUB:
                    // add pub register
                    break;
                case OP_CODE_REGIST_SUB:
                    // add tfs init
                    break;
                case OP_CODE_CREATE_BOX:
                    break;
                case OP_CODE_CREATE_BOX_ANSWER:
                    break;
                case OP_CODE_REMOVE_BOX:
                    break;
                case OP_CODE_REMOVE_BOX_ANSWER:
                    break;
                case OP_CODE_LIST_BOXES:
                    break;
                case OP_CODE_LIST_BOXES_ANSWER:
                    break;
                default:
                    printf("Invalid operation code %c\n", op_code);
                    close_server();
                    return -1;
            }
                    // add client init
                    break;
                case 't':
                    // add tfs init
                    break;
                case 'q':
                    close_server();
                    break;
                default:
                    printf("Invalid operation code %c\n", op_code);
                    close_server();
                    return -1;
            }
        }
    }

    

    return -1;
}


void close_server() {
    if (close(fdserver) < 0) {
        perror("Failed to close pipe");
        exit(-1);
    }

    if (unlink(pipename) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        exit(-1);
    }

    printf("\nSuccessfully ended the server.\n");
    exit(-1);
}