#include "logging.h"
#include "mbroker.h"
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <common/common.h>

static int max_sessions;
static int server_pipe;
static char *pipename;

static client_t *clients;
static bool *free_clients;
static pthread_mutex_t free_clients_lock;

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
    
    if (server_pipe = open(pipename, O_RDONLY ) < 0 ) {
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

        bytes_read = read(server_pipe, &op_code, sizeof(char));

        while (bytes_read > 0) {

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
                case OP_CODE_PUBLISHER:
                    break;
                default:
                    printf("Invalid operation code %c\n", op_code);
                    close_server();
                    return -1;
            }
            bytes_read = read(server_pipe, &op_code, sizeof(char));
        }

        if (bytes_read < 0) {
            printf("Failed to read from pipe %s\n", pipename);
            close_server();
            return -1;
        }
        
    }

    

    return -1;
}

int init_mbroker() {
    clients = malloc(max_sessions * sizeof(client_t));
    free_clients = malloc(max_sessions * sizeof(bool));

    for (int i = 0; i < max_sessions; ++i) {
        clients[i].session_id = i;
        clients[i].to_execute = false;
        mutex_init(&clients[i].lock);
        if (pthread_cond_init(&clients[i].cond, NULL) != 0) {
            return -1;
        }
        if (pthread_create(&clients[i].tid, NULL, client_session,
                           &clients[i]) != 0) {
            return -1;
        }
        free_clients[i] = false;
    }
    mutex_init(&free_clients_lock);
    return 0;
}

void *client_session(void *client_in_array) {
    client_t *client = (client_t *)client_in_array;
    while (true) {
        mutex_lock(&client->lock);

        while (!client->bool_cond) {
            if (pthread_cond_wait(&client->cond, &client->lock) != 0) {
                perror("Failed to wait for condition variable");
                close_server(EXIT_FAILURE);
            }
        }

        int result = 0;

        switch (client->box.opcode) { // TODO: implement handle functions

        case OP_CODE_REGIST_PUB:
            result = handle_tfs_register(client);
            break;
        case OP_CODE_REGIST_SUB:
            result = handle_tfs_register(client);
            break;
        case OP_CODE_CREATE_BOX:
            result = handle_tfs_create_box(client);
            break;
        case OP_CODE_CREATE_BOX_ANSWER:
            result = handle_tfs_create_box_answer(client);
            break;
        case OP_CODE_REMOVE_BOX:
            result = handle_tfs_remove_box(client);
            break;
        case OP_CODE_REMOVE_BOX_ANSWER:
            result = handle_tfs_remove_box_answer(client);
            break;
        case OP_CODE_LIST_BOXES:
            result = handle_tfs_list_boxes(client);
            break;
        case OP_CODE_LIST_BOXES_ANSWER:
            result = handle_tfs_list_boxes_answer(client);
            break;
        case OP_CODE_PUBLISHER:
            result = handle_tfs_write_box(client);
            break;
        default:
            break;
        }

        if (result != 0) {
            /* if there is an error during the handling of the message, discard
             * this session */
            if (free_client(client->session_id) == -1) {
                perror("Failed to free client");
                close_server(EXIT_FAILURE);
            }
        }

        client->bool_cond = false;
        mutex_unlock(&client->lock);
    }
}


void close_server() {
    if (close(server_pipe) < 0) {
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