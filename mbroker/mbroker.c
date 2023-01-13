#include "mbroker.h"




static int max_sessions;
static int num_boxes;
static int max_boxes;
static int server_pipe;
static char *pipename;


static client_t *clients;
static node_t *boxes;
static pthread_mutex_t boxes_lock;

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

    if (tfs_init(NULL) != 0) {
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
    
    if (server_pipe = open(pipename, O_RDONLY) < 0 ) {
        printf("Failed to open server pipe %s\n", pipename);
        unlink(pipename);
        return -1;
    }

    for (;;) {
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
                    parser(op_code, parse_client_and_box);
                    break;
                case OP_CODE_REGIST_SUB:
                    parser(op_code, parse_client_and_box);
                    break;
                case OP_CODE_CREATE_BOX:
                    parser(op_code, parse_client_and_box);
                    break;
                case OP_CODE_REMOVE_BOX:
                    parser(op_code, parse_client_and_box);
                    break;
                case OP_CODE_LIST_BOXES:
                    parser(op_code, parse_client);
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

int mbroker_init() {
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
    mutex_init(&boxes_lock);
    return 0;
}

void *client_session(void *client_in_array) {
    client_t *client = (client_t *)client_in_array;
    while (true) {
        safe_mutex_lock(&client->lock);

        while (!client->to_do) {
            if (pthread_cond_wait(&client->cond, &client->lock) != 0) {
                perror("Failed to wait for condition variable");
                close_server(EXIT_FAILURE);
            }
        }

        int result = 0;

        switch (client->opcode) { // TODO: implement handle functions

        case OP_CODE_REGIST_PUB:
            result = handle_tfs_register(client);
            break;
        case OP_CODE_REGIST_SUB:
            result = handle_tfs_register(client);
            break;
        case OP_CODE_CREATE_BOX:
            result = handle_tfs_create_box(client);
            break;
        case OP_CODE_REMOVE_BOX:
            result = handle_tfs_remove_box(client);
            break;
        case OP_CODE_LIST_BOXES:
            result = handle_tfs_list_boxes(client);
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
        safe_mutex_unlock(&client->lock);
    }
}


int handle_tfs_register(client_t *client) {
    //Open client pipe
    int client_pipe;
    if(client->opcode == OP_CODE_REGIST_PUB ){ // publisher
        client_pipe = open(client->client_pipename, O_RDONLY);
    } else { // subscriber
        client_pipe = open(client->client_pipename, O_WRONLY);
    }

    if (client_pipe < 0) {
        perror("Failed to open pipe");
        return -1;
    }
    client->client_pipe = client_pipe;

    //Check if the box already has a publisher
    if(client->opcode == OP_CODE_REGIST_PUB){
        if(get_box(client->box_name)->n_publishers == 1){
            printf("Box %s already has a publisher\n", client->box_name);
            safe_close(client_pipe);
            return -1;
        }

        return handle_messages_from_publisher(client);
    } else{
        //TODO: handle_messages_to_subscriber()
    }

    return 0;
}

box_t* get_box(char *box_name) {
    safe_mutex_lock(&boxes_lock);
    //get box from linkedlist using get_data_by_value
    box_t *box = (box_t *)get_data_by_value(boxes, box_name, compare_box_names);
    //return box
    safe_mutex_unlock(&boxes_lock);
    return box;
}

int compare_box_names(node_t * node, char *box_name) {
    box_t *box = (box_t *)node->data;
    return strcmp(box->box_name, box_name);
}

int free_client_session(int session_id) {
    safe_mutex_lock(&free_clients_lock);
    if (free_clients[session_id] == true) {
        safe_mutex_unlock(&free_clients_lock);
        return -1;
    }
    free_clients[session_id] = true;
    safe_mutex_unlock(&free_clients_lock);
    return 0;
}

int get_free_client_session() {
    safe_mutex_lock(&free_clients_lock);
    for (int i = 0; i < max_sessions; ++i) {
        if (free_clients[i] == true) {
            free_clients[i] = false;
            safe_mutex_unlock(&free_clients_lock);
            return i;
        }
    }
    safe_mutex_unlock(&free_clients_lock);
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

int parse (char op_code, int parser_fnc (client_t *)) {
    int session_id = get_free_client_session();
    if (session_id == -1) {
        printf("No free sessions\n");
        return -1;
    }
    printf("The session number %d was created with success.\n", session_id);

    client_t *client = &clients[session_id];
    client->opcode = op_code;
    int result = parser_fnc(client);
    if (result != 0) {
        printf("Failed to parse message\n");
        return -1;
    }
    safe_mutex_lock(&client->lock);
    client->to_do = true;
    if (pthread_cond_signal(&client->cond) != 0) {
        perror("Failed to signal condition variable");
        return -1;
    }
    safe_mutex_unlock(&client->lock);
    return 0;
}

int parse_client(client_t *client) {
    //read opcode to client from pipe
    read_pipe(server_pipe, &client->opcode, sizeof(uint8_t));
    //read client pipename to client from pipe
    read_pipe(server_pipe, &client->client_pipename, sizeof(char)* CLIENT_NAMED_PIPE_PATH_LENGTH);
    //make sure the strings are null terminated
    client->client_pipename[CLIENT_NAMED_PIPE_PATH_LENGTH] = '\0';
    return 0;
}

int parse_client_and_box(client_t * client) {
    //read opcode to client and client pipename to client from pipe
    parse_client(client);
    //read box name to client from pipe
    char box_name[BOX_NAME_LENGTH + 1];
    read_pipe(server_pipe, box_name, sizeof(char)* BOX_NAME_LENGTH);

    //make sure the strings are null terminated
    box_name[BOX_NAME_LENGTH] = '\0';

    if(client->opcode == 3){ //If you need to create this box
        box_t *tmp_box = (box_t *) malloc(sizeof(box_t)); //Temporary box used to store a box name
        strcpy(client->box_name, box_name);
    } else if((client->box_name = get_box(box_name)) == NULL) { //If you need to register to this box
        printf("Box %s does not exist\n", box_name);
        return -1;
    }
    
    return 0;
}
int parse_list (client_t *client) {
    //read opcode to client from pipe
    read_pipe(server_pipe, &client->opcode, sizeof(uint8_t));
    //read client pipename to client from pipe
    read_pipe(server_pipe, &client->client_pipename, sizeof(char)* CLIENT_NAMED_PIPE_PATH_LENGTH);

}



int handle_tfs_remove_box(client_t *client) {
    //remove box from linkedlist using remove_by_value
    safe_mutex_lock(&boxes_lock);

    //if box doesnt exist print error
    if (get_box(client->box_name) == NULL) {
        printf("Box %s does not exist\n", client->box_name);
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }

    remove_by_value(&boxes, get_box(client->box_name), compare_box_names);

    client->box_name = NULL;

    safe_mutex_unlock(&boxes_lock);

    return 0;
}

int handle_list_response (client_t client) {
    //open client pipe
    int client_pipe = open(client.client_pipename, O_WRONLY);
    if (client_pipe < 0) {
        perror("Failed to open pipe");
        return -1;
    }
    
    char buffer[sizeof(uint8_t) * 2 + BOX_NAME_LENGTH + sizeof(uint64_t) * 3]; 
    uint8_t last = 0;
    
    while (boxes != NULL) {
        box_t *box = boxes->data;
        memcpy(buffer, &client.opcode, sizeof(uint8_t));
        
        if (boxes->next == NULL) {
            last = 1;
        }

        memcpy(buffer + sizeof(uint8_t),&last, sizeof(u_int8_t));
        memcpy(buffer + sizeof(uint8_t)*2, &box->box_name, sizeof(char)*BOX_NAME_LENGTH);
        memcpy(buffer + sizeof(uint8_t)*2 + sizeof(char)*BOX_NAME_LENGTH, &box->box_size, sizeof(uint64_t));
        memcpy(buffer + sizeof(uint8_t)*2 + sizeof(char)*BOX_NAME_LENGTH + sizeof(uint64_t), &box->n_publishers, sizeof(uint64_t));
        memcpy(buffer + sizeof(uint8_t)*2 + sizeof(char)*BOX_NAME_LENGTH + sizeof(uint64_t)*2, &box->n_subscribers, sizeof(uint64_t));
        write_pipe(client_pipe, buffer, sizeof(uint8_t) * 2 + BOX_NAME_LENGTH + sizeof(uint64_t) * 3);
        
        free(buffer);
        boxes = boxes->next;
    }
    //close client pipe
    safe_close(client_pipe);

    return 0;
}
   
int handle_tfs_create_box(client_t *client) {
    
    safe_mutex_lock(&boxes_lock);

    if (client->box_name == NULL) {
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }

    if(num_boxes == MAX_BOXES) {
        printf("Max number of boxes reached\n");
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }

    if (get_box(client->box_name) != NULL) {
        printf("Box %s already exists\n", client->box_name);
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }


    if (create_box(client->box_name) < 0) {
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }
    
    safe_mutex_unlock(&boxes_lock);
    return 0;
}

int create_box(char * box_name) {
    
    int fhandle = tfs_open(box_name, TFS_O_CREAT);
    
    if (fhandle < 0) {
        perror("Failed to create box in tfs\n");
        return -1;
    }

    tfs_close(fhandle);
    
    box_t *tmp_box = (box_t *) malloc(sizeof(box_t));

    tmp_box->fhandle = fhandle;
    tmp_box->box_size = 0;
    tmp_box->n_publishers = 0;
    tmp_box->n_subscribers = 0;

    num_boxes++;
    push(&boxes, tmp_box);

    return 0;
}

int handle_messages_from_publisher(client_t *client){
    uint8_t opcode;
    char message[MESSAGE_LENGTH + 1];
    message[MESSAGE_LENGTH] = '\0';
    ssize_t bytes_read;
    ssize_t bytes_written;
    while(true){
        //Check if it returns EOF
        bytes_read = try_read(client->client_pipe, &opcode, sizeof(uint8_t));
        if(bytes_read ==0){ //EOF 
            safe_close(client->client_pipe);
            return 0;
        } else if (bytes_read < 0){ //Error
            printf("Failed to read from pipe %d\n", client->client_pipe);
            return -1;
        }
        read_pipe(client->client_pipe, &message, MESSAGE_LENGTH);
        box_t* box = get_box(client->box_name);
        //Write message with '\0' at the end to box
        bytes_written = tfs_write(box->fhandle, message, strlen(message) + 1);
        if(bytes_written < 0){
            printf("Failed to write to box %s in tfs\n", box->box_name);
            return -1;
        } else if(bytes_written < strlen(message) + 1){
            printf("Box %s is full\n", box->box_name);
            return -1;
        }
        box->box_size += bytes_written;

    }
}