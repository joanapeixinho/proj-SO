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

    //Initialize mbroker
    if (mbroker_init() != 0) {
        printf("Failed to init mbroker\n");
        return -1;
    }

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
            close_server(  EXIT_FAILURE );
            return -1;
        }

        if (close(pipe_temp) < 0) {
            printf("Failed to close pipe %s\n", pipename);
            close_server( EXIT_FAILURE );
            return -1;
        }

        ssize_t bytes_read;
        char op_code;

        bytes_read = read(server_pipe, &op_code, sizeof(char));

        while (bytes_read > 0) {

            switch (op_code) {
                case OP_CODE_REGIST_PUB:
                    parser(op_code, parse_box);
                    break;
                case OP_CODE_REGIST_SUB:
                    parser(op_code, parse_box);
                    break;
                case OP_CODE_CREATE_BOX:
                    parser(op_code, parse_box);
                    break;
                case OP_CODE_CREATE_BOX_ANSWER:
                    break;
                case OP_CODE_REMOVE_BOX:
                    parser(op_code, parse_box);
                    break;
                case OP_CODE_REMOVE_BOX_ANSWER:
                    break;
                case OP_CODE_LIST_BOXES:
                    parser(op_code, parse_list);
                    break;
                case OP_CODE_LIST_BOXES_ANSWER:
                    break;
                default:
                    printf("Invalid operation code %c\n", op_code);
                    close_server(EXIT_FAILURE);
                    return -1;
            }
            bytes_read = read(server_pipe, &op_code, sizeof(char));
        }

        if (bytes_read < 0) {
            printf("Failed to read from pipe %s\n", pipename);
            close_server(EXIT_FAILURE);
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
        clients[i].to_do = false;
        mutex_init(&clients[i].lock);
        if (pthread_cond_init(&clients[i].cond, NULL) != 0) {
            return -1;
        }
        if (pthread_create(&clients[i].thread_id, NULL, client_session,
                           &clients[i]) != 0) {
            return -1;
        }
        free_clients[i] = true; // all clients are free in the beginning
    }
    mutex_init(&free_clients_lock);
    return 0;
}

void *client_session(void *client_in_array) {
    client_t *client = (client_t *)client_in_array;
    while (true) {
        mutex_lock(&client->lock);

        while (!client->to_do) {
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
            if (free_client_session(client->session_id) == -1) {
                perror("Failed to free client");
                close_server(EXIT_FAILURE);
            }
        }

        client->to_do = false;
        mutex_unlock(&client->lock);
    }
}


int free_client_session(int session_id) {
    mutex_lock(&free_clients_lock);
    if (free_clients[session_id] == true) {
        mutex_unlock(&free_clients_lock);
        return -1;
    }
    free_clients[session_id] = true;
    mutex_unlock(&free_clients_lock);
    return 0;
}

int get_free_client_session() {
    mutex_lock(&free_clients_lock);
    for (int i = 0; i < max_sessions; ++i) {
        if (free_clients[i] == true) {
            free_clients[i] = false;
            mutex_unlock(&free_clients_lock);
            return i;
        }
    }
    mutex_unlock(&free_clients_lock);
    return -1;
}

void close_server(int exit_code) {
    for (int i = 0; i < max_sessions; ++i) {
        if (free_client(i) == -1) {
            perror("Failed to free client");
            exit(EXIT_FAILURE);
        }
    }
    free(clients);
    free(free_clients);
    exit(exit_code);
}

parse (char op_code, int parser_fnc (client_t *)) {
    int session_id = get_free_client_session();
    if (session_id == -1) {
        printf("No free sessions\n");
        return -1;
    }
    client_t *client = &clients[session_id];
    client->box.opcode = op_code;
    int result = parser_fnc(client);
    if (result != 0) {
        printf("Failed to parse message\n");
        return -1;
    }
    mutex_lock(&client->lock);
    client->to_do = true;
    if (pthread_cond_signal(&client->cond) != 0) {
        perror("Failed to signal condition variable");
        return -1;
    }
    mutex_unlock(&client->lock);
    return 0;
}

parse_box(client_t *client) {
    read_pipe(server_pipe, &client->box.client_pipename, sizeof(char)* PIPE_NAME_LENGTH);
    client->box.client_pipename[PIPE_NAME_LENGTH - 1] = '\0';
    read_pipe(server_pipe, &client->box.box_name, sizeof(char)* BOX_NAME_LENGTH);
    client->box.box_name[BOX_NAME_LENGTH - 1] = '\0';
    return 0;
}

parse_list (client_t *client) {
    read_pipe(server_pipe, &client->box.client_pipename, sizeof(char)* PIPE_NAME_LENGTH);
    client->box.client_pipename[PIPE_NAME_LENGTH - 1] = '\0';
    return 0;
}