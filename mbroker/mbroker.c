#include "mbroker.h"




static unsigned long int max_sessions;
static int num_boxes;
static int server_pipe;
static char *pipename;

static pc_queue_t pc_queue;

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

    max_sessions = strtoul(argv[2], NULL, 10);

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

    if (unlink(pipename) <0 && errno != ENOENT ) {
        printf("Failed to unlink pipe %s\n", pipename);
        return -1;
    }

    if (mkfifo(pipename, 0777) < 0) {
        printf("Failed to create pipe %s\n", pipename);
        return -1;
    }
    
    if ((server_pipe = open(pipename, O_RDONLY)) < 0 ) {
        printf("Failed to open server pipe %s\n", pipename);
        tfs_unlink(pipename);
        return -1;
    }


    for (;;) {
        /* Open and close a temporary pipe to avoid having active wait for another
         * process to open the pipe. The 'open' function blocks until the pipe
         * is openned on the other side, therefore doing exactly what we want.
         */

        int pipe_temp = open(pipename, O_RDONLY);

        if (pipe_temp < 0) {
            if (errno == ENOENT) {
                /* if pipe does not exist, means we've exited */
                return 0;
            }
            printf("Failed to open pipe %s\n", pipename);
            close_server(EXIT_FAILURE);
        }

        if (close(pipe_temp) < 0) {
            printf("Failed to close pipe %s\n", pipename);
            close_server(EXIT_FAILURE);
        }

        
        ssize_t bytes_read;
        uint8_t op_code;

        printf("Waiting for request\n");

        bytes_read = try_read(server_pipe, &op_code, sizeof(uint8_t));
      

        while (bytes_read > 0) {

            if (parser(op_code) == -1) {
                printf("Failed to parse request\n");
                close_server(EXIT_FAILURE);
                return -1;
            }

            bytes_read = try_read(server_pipe, &op_code, sizeof(uint8_t));
          
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
    clients =   (client_t*) malloc(max_sessions * sizeof(client_t));
    free_clients = (bool*) malloc(max_sessions * sizeof(bool));
    
    if (free_clients == NULL) {
        perror("Failed to allocate memory for free_clients");
        free(clients);
        return -1;
    }

    if( pcq_create(&pc_queue, max_sessions) != 0 ) {
        free(clients);
        free(free_clients);
        perror("Failed to create producer-consumer queue");
        return -1;
    }

    for (int i = 0; i < max_sessions; ++i) {
        clients[i].session_id = i;
        if (pthread_create(&clients[i].thread_id, NULL, client_session,
                           &clients[i]) != 0) {
            printf("Failed to create thread for client %d", i);
            free(clients);
            free(free_clients);
            pcq_destroy(&pc_queue);
            return -1;
        }
        free_clients[i] = true; // all clients are free in the beginning
    }
    if (pthread_mutex_init(&free_clients_lock, NULL) != 0) {
        free(clients);
        free(free_clients);
        pcq_destroy(&pc_queue);
        printf("Failed to initialize free_clients_lock\n");
        return -1;
    }
    if (pthread_mutex_init(&boxes_lock, NULL) != 0) {
        free(clients);
        free(free_clients);
        pcq_destroy(&pc_queue);
        pthread_mutex_destroy(&free_clients_lock);
        printf("Failed to initialize boxes_lock\n");
        return -1;
    }
    num_boxes = 0;
    boxes = NULL;
    return 0;
}

void *client_session(void *client_in_array) {
    client_t *client = (client_t *)client_in_array;
    while (true) {
        request_t* request = (request_t*) pcq_dequeue(&pc_queue);
        
        if (request == NULL) {
            printf("Failed to dequeue request\n");
            return NULL;
        }
        client->opcode = request->opcode;
        printf("Received request %d from client %d\n", client->opcode, client->session_id);

        memcpy(client->client_pipename, request->client_pipename, CLIENT_NAMED_PIPE_PATH_LENGTH + 1);
        printf("Client pipename: %s\n", client->client_pipename);

        if( request->opcode != OP_CODE_LIST_BOXES) {
            memcpy(client->box_name, request->box_name, BOX_NAME_LENGTH + 1);
        }
        
        int result = 0;

        switch (client->opcode) { 

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
        printf("Failed to open pipe");
        return -1;
    }
    client->client_pipe = client_pipe;

    //Try to get the box
    box_t *box = get_box(client->box_name);
    if(box == NULL){
        printf("Box %s does not exist\n", client->box_name);
        safe_close(client_pipe);
        return -1;
    }
    client->box = box;

    //Check if the box already has a publisher
    if(client->opcode == OP_CODE_REGIST_PUB){
        if(box->n_publishers == 1){
            printf("Box %s already has a publisher\n", client->box_name);
            safe_close(client_pipe);
            return -1;
        }
        client->box->n_publishers++;
        return handle_messages_from_publisher(client);
    } else {
        client->box->n_subscribers++;
        return handle_messages_to_subscriber(client);
    }
    return 0;
}

box_t* get_box(char *box_name) {
    //get box from linkedlist using get_data_by_value
    box_t *box = (box_t *)get_data_by_value(boxes, box_name, compare_box_names);
    //return box
    return box;
}

int compare_box_names(void* node, void *box_name) {
    node_t* new_node = (node_t *)node;
    box_t *box = (box_t *)new_node->data;
    return strcmp(box->box_name, box_name);
}

int free_client_session(int session_id) {
    safe_mutex_lock(&free_clients_lock);
    //check if the client is already free
    if (free_clients[session_id] == true) {
        safe_mutex_unlock(&free_clients_lock);
        return -1;
    }
    //free client session
    free_clients[session_id] = true;
    safe_mutex_unlock(&free_clients_lock);
    return 0;
}

int is_client_free(int session_id) {
    safe_mutex_lock(&free_clients_lock);
    int result = free_clients[session_id];
    safe_mutex_unlock(&free_clients_lock);
    return result;
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
            exit(EXIT_FAILURE);
        }
    }
    pcq_destroy(&pc_queue);
    free(clients);
    free(free_clients);
    free(boxes);
    pthread_mutex_destroy(&boxes_lock);
    pthread_mutex_destroy(&free_clients_lock);
    exit(exit_code);
}

//destroy client 

int free_client (int session_id) {
    client_t *client = &clients[session_id];
    if (pthread_join(client->thread_id, NULL) != 0) {
        perror("Failed to join thread");
        return -1;
    }
    if (close(client->client_pipe) == -1) {
        perror("Failed to close pipe");
        return -1;
    }
    if (free_client_session(client->session_id) == -1) {
        perror("Failed to free client");
        return -1;
    }
    return 0;
}

void print_request (request_t* request) {
    printf("Opcode: %d\n", request->opcode);
    printf("Client pipename: %s\n", request->client_pipename);
    printf("Box name: %s\n", request->box_name);
}

int parser(uint8_t op_code) {
    
    request_t* request = (request_t*) malloc(sizeof(request_t));
    
    if (request == NULL) {
        perror("Failed to allocate memory");
        return -1;
    }
    //check if op_code is valid
    if(op_code > 7){
        printf("Invalid op_code\n");
        return -1;
    }
    request->opcode = op_code;
    char buffer[CLIENT_NAMED_PIPE_PATH_LENGTH];
    read_pipe(server_pipe, buffer, sizeof(buffer));
    strncpy(request->client_pipename, buffer, sizeof(buffer) + 1);

    if(op_code != OP_CODE_LIST_BOXES){
        char buffer2[BOX_NAME_LENGTH];
        read_pipe(server_pipe, buffer2, sizeof(buffer2));
        strncpy(request->box_name, buffer2, sizeof(buffer2) + 1);
    }
   
    //Sends request to the queue to wait to be popped by a client session
    if(pcq_enqueue(&pc_queue, request) == -1){
        printf("Failed to enqueue request\n");
        return -1;
    } 
    
    return 0;
} 



int handle_tfs_remove_box(client_t *client) {

    //remove box from linkedlist using remove_by_value
    safe_mutex_lock(&boxes_lock);
    char error_msg[MESSAGE_LENGTH +1] = {0};
    //if box doesnt exist print error
    if (get_box(client->box_name) == NULL) {
        strcpy(error_msg, "Box does not exist\n");
        //send error response to pipe
        write_pipe(client->client_pipe, error_msg, sizeof(char)* MESSAGE_LENGTH);
        safe_close(client->client_pipe);
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }

    remove_by_value(boxes, get_box(client->box_name), compare_box_names);

   //end all client sessions with this box
    for (int i = 0; i < max_sessions; ++i) {
        if (strcmp(clients[i].box_name, client->box_name) == 0) {
            if (free_client_session(i) == -1) {
                //send error response to pipe
                strcpy(error_msg, "Failed to free client\n");
                write_pipe(client->client_pipe, error_msg, sizeof(char)* MESSAGE_LENGTH);
                safe_close(client->client_pipe);
                safe_mutex_unlock(&boxes_lock);
                return -1;
            }
        }
    }

    tfs_unlink(client->box_name);

    safe_mutex_unlock(&boxes_lock);
    return 0;
}

int handle_tfs_list_boxes (client_t *client) {
    //open client pipe
    int client_pipe = open(client->client_pipename, O_WRONLY);
    if (client_pipe < 0) {
        perror("Failed to open pipe");
        return -1;
    }
    
    char buffer[sizeof(uint8_t) * 2 + BOX_NAME_LENGTH + sizeof(uint64_t) * 3]; 
    uint8_t last = 0;
    
    while (boxes != NULL) {
        box_t *box = boxes->data;
        memcpy(buffer, &client->opcode, sizeof(uint8_t));
        
        if (boxes->next == NULL) {
            last = 1;
        }

        memcpy(buffer + sizeof(uint8_t),&last, sizeof(uint8_t));
        memcpy(buffer + sizeof(uint8_t)*2, &box->box_name, sizeof(char)*BOX_NAME_LENGTH);
        memcpy(buffer + sizeof(uint8_t)*2 + sizeof(char)*BOX_NAME_LENGTH, &box->box_size, sizeof(uint64_t));
        memcpy(buffer + sizeof(uint8_t)*2 + sizeof(char)*BOX_NAME_LENGTH + sizeof(uint64_t), &box->n_publishers, sizeof(uint64_t));
        memcpy(buffer + sizeof(uint8_t)*2 + sizeof(char)*BOX_NAME_LENGTH + sizeof(uint64_t)*2, &box->n_subscribers, sizeof(uint64_t));
        write_pipe(client_pipe, buffer, sizeof(uint8_t) * 2 + BOX_NAME_LENGTH + sizeof(uint64_t) * 3);
        
    
        boxes = boxes->next;
    }
    //close client pipe
    safe_close(client_pipe);

    return 0;
}
   
int handle_tfs_create_box(client_t *client) {
    printf("Starting handle_tfs_create_box...\n");
    safe_mutex_lock(&boxes_lock);
    int32_t return_code = 0;
    char error_msg[MESSAGE_LENGTH + 1] = {0};

    if (client->box_name == NULL) {
        snprintf(error_msg, MESSAGE_LENGTH, "Box name is null\n");
        return_code = -1;
    }

    if (num_boxes == MAX_BOXES) {
        snprintf(error_msg, MESSAGE_LENGTH, "Reached max number of boxes\n");        
        return_code = -1;
    }
    printf("Getting box %s ...\n", client->box_name);
    if (get_box(client->box_name) != NULL) {
        snprintf(error_msg, MESSAGE_LENGTH, "Box already exists\n");
        return_code = -1;
    }

    printf("Creating box %s ...\n", client->box_name);
    if (create_box(client->box_name) < 0) {
        snprintf(error_msg, MESSAGE_LENGTH, "Failed to create box\n");
        return_code = -1;
    }
    
    if (return_code == -1) {
        write_pipe(client->client_pipe, &return_code, sizeof(uint8_t));
        write_pipe(client->client_pipe, error_msg, sizeof(char)*MESSAGE_LENGTH);
        safe_close(client->client_pipe);
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }

    safe_mutex_unlock(&boxes_lock);
    
    write_pipe(client->client_pipe, &return_code, sizeof(uint8_t));
    write_pipe(client->client_pipe, error_msg, sizeof(char)*MESSAGE_LENGTH);
    safe_close(client->client_pipe);

    return 0;
}

int create_box(char * box_name) {
    
    int fhandle = tfs_open(box_name, TFS_O_CREAT);
    
    if (fhandle < 0) {
        return -1;
    }

    tfs_close(fhandle);
    
    box_t *tmp_box = (box_t *) malloc(sizeof(box_t));
    if (tmp_box == NULL) {
        return -1;
    }

    strcpy(tmp_box->box_name, box_name);
    tmp_box->box_size = 0;
    tmp_box->n_publishers = 0;
    tmp_box->n_subscribers = 0;
    //initiate threadlock
    pthread_mutex_init(&tmp_box->lock, NULL);
    pthread_cond_init(&tmp_box->cond, NULL);
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
            safe_close(client->client_pipe);
            return -1;
        }

        read_pipe(client->client_pipe, &message, MESSAGE_LENGTH);
        box_t* box = get_box(client->box_name);
        safe_mutex_lock(&box->lock);
        int fhandle = tfs_open(box->box_name, TFS_O_APPEND);
        bytes_written = tfs_write(fhandle, message, strlen(message) + 1);
        if(bytes_written < 0){
            printf("Failed to write to box %s in tfs\n", box->box_name);
            tfs_close(fhandle);
            safe_close(client->client_pipe);
            return -1;
        } else if(bytes_written < strlen(message) + 1){
            printf("Box %s is full\n", box->box_name);
            safe_close(client->client_pipe);
            tfs_close(fhandle);
            return -1;
        }
        box->box_size += strlen(message) + 1;
        tfs_close(fhandle);
        //Signal subscribers
        pthread_cond_broadcast(&box->cond);
        safe_mutex_unlock(&box->lock);
    }

    return 0;
}

//read messages from subscriber's box and send to client
int handle_messages_to_subscriber(client_t *client){
    char message[MESSAGE_LENGTH + 1];
    uint8_t opcode = OP_CODE_SUBSCRIBER;
    ssize_t bytes_read;
    ssize_t bytes_written;
    box_t* box = get_box(client->box_name);

    //Read each message from box and send to client
    //each message is terminated by '\0'

    while(true){
        safe_mutex_lock(&box->lock);

        //Wait until there is more to read
        while ((box->box_size - (uint64_t) client->offset) == 0) {
            pthread_cond_wait(&box->cond, &box->lock);
        }
        safe_mutex_unlock(&box->lock);
         
        memset(message, 0, MESSAGE_LENGTH);
        bytes_read = tfs_read(client->box_fd, message, MESSAGE_LENGTH);
        if(bytes_read == 0){ //EOF
            break;
        } else if (bytes_read < 0){ //Error
            printf("Failed to read from box %s in tfs\n", box->box_name);
            tfs_close(client->box_fd);
            return -1;
        }
        write_pipe(client->client_pipe, &opcode, sizeof(uint8_t));
        bytes_written = try_write(client->client_pipe, message , MESSAGE_LENGTH );

        if(bytes_written < 0){
            printf("Failed to write to pipe %d\n", client->client_pipe);
            tfs_close(client->box_fd);
            safe_close(client->client_pipe);
            return -1;
        } else if(bytes_written < MESSAGE_LENGTH){
            printf("Failed to write to pipe %d\n", client->client_pipe);
            tfs_close(client->box_fd);
            safe_close(client->client_pipe);
            return -1;
        }
        client->offset += bytes_read; //It's supposed to be equal to bytes_written
    }
    return 0;
}

int handle_messages_until_now(client_t *client){
    safe_mutex_lock(&client->box->lock);

    char buffer[MESSAGE_LENGTH + 1];
    char message[MESSAGE_LENGTH + 1];
    uint8_t opcode = OP_CODE_SUBSCRIBER;
    ssize_t bytes_read;
    ssize_t tmp_offset;
    client->box_fd = tfs_open(client->box->box_name, TFS_O_APPEND);
    while((client->box->box_size - (uint64_t) client->offset) > 0){

        bytes_read = tfs_read(client->box_fd, buffer, MESSAGE_LENGTH);
        if(bytes_read == 0){ //EOF
            break;
        } else if (bytes_read < 0){ //Error
            printf("Failed to read from box %s in tfs\n", client->box->box_name);
            tfs_close(client->box_fd);
            safe_close(client->client_pipe);
            return -1;
        }

        tmp_offset = 0;
        while(tmp_offset < bytes_read){
            memset(message, 0, MESSAGE_LENGTH + 1);
            //Copy starting from the last '\0' character until the next '\0' character
            strcpy(message, buffer + tmp_offset);

            write_pipe(client->client_pipe, &opcode, sizeof(uint8_t));
            write_pipe(client->client_pipe, message, MESSAGE_LENGTH);
            tmp_offset += (ssize_t) strlen(message) + 1;
        }

        client->offset += bytes_read; 
    }
    safe_mutex_unlock(&client->box->lock);
    return 0;
}