#include "mbroker.h"

static unsigned long int max_sessions;
static int num_boxes;
static int server_pipe;
static char *pipename;

static pc_queue_t pc_queue;

static client_t *clients;
static box_t boxes[MAX_BOXES];
static bool free_boxes[MAX_BOXES];
static pthread_mutex_t boxes_lock;

int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr,
                "Please specify mbroker pipename and number of max sessions\n");
        return -1;
    }

    max_sessions = strtoul(argv[2], NULL, 10);

    // Initialize mbroker
    if (mbroker_init() != 0) {
        printf("Failed to init mbroker\n");
        return -1;
    }

    tfs_params params = {
        .max_inode_count = MAX_BOXES,
        .max_block_count = MESSAGE_LENGTH * MAX_BOXES,
        .max_open_files_count = MAX_BOXES,
        .block_size = BOX_SIZE,
    };

    if (tfs_init(&params) != 0) {
        printf("Failed to init tfs\n");
        return -1;
    }

    pipename = argv[1];

    if (unlink(pipename) < 0 && errno != ENOENT) {
        printf("Failed to unlink pipe %s\n", pipename);
        return -1;
    }

    if (mkfifo(pipename, 0777) < 0) {
        printf("Failed to create pipe %s\n", pipename);
        return -1;
    }

    if ((server_pipe = open(pipename, O_RDONLY)) < 0) {
        printf("Failed to open server pipe %s\n", pipename);
        tfs_unlink(pipename);
        return -1;
    }

    printf("Mbroker started...\n");

    // Start the server
    for (;;) {
        /* Open and close a temporary pipe to avoid having active wait for
         * another process to open the pipe. The 'open' function blocks until
         * the pipe is openned on the other side, therefore doing exactly what
         * we want.
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

        bytes_read = try_read(server_pipe, &op_code, sizeof(uint8_t));

        while (bytes_read > 0) {
            printf("Waiting for request...\n");
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
    clients = (client_t *)malloc(max_sessions * sizeof(client_t));

    if (pcq_create(&pc_queue, max_sessions) != 0) {
        free(clients);
        printf("Failed to create producer-consumer queue\n");
        return -1;
    }

    for (int i = 0; i < max_sessions; ++i) {
        clients[i].session_id = i;
        if (pthread_create(&clients[i].thread_id, NULL, client_session,
                           &clients[i]) != 0) {
            printf("Failed to create thread for client %d", i);
            free(clients);
            pcq_destroy(&pc_queue);
            return -1;
        }
    }

    if (pthread_mutex_init(&boxes_lock, NULL) != 0) {
        free(clients);
        pcq_destroy(&pc_queue);
        printf("Failed to initialize boxes_lock\n");
        return -1;
    }

    for (int i = 0; i < MAX_BOXES; ++i) {
        free_boxes[i] = true; // all boxes are free in the beginning
    }
    num_boxes = 0;
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE to treat the closed pipe
    return 0;
}

void *client_session(void *client_in_array) {
    client_t *client = (client_t *)client_in_array;
    while (true) {
        request_t *request = (request_t *)pcq_dequeue(&pc_queue);

        if (request == NULL) {
            printf("Failed to dequeue request\n");
            return NULL;
        }
        client->opcode = request->opcode;

        memcpy(client->client_pipename, request->client_pipename,
               CLIENT_NAMED_PIPE_PATH_LENGTH + 1);

        if (request->opcode != OP_CODE_LIST_BOXES) {
            memcpy(client->box_name, request->box_name, BOX_NAME_LENGTH + 1);
        }

        int result = 0;

        switch (client->opcode) {

        case OP_CODE_REGIST_PUB:
            result = handle_tfs_register(client);
            printf("Ended publisher session\n");
            break;
        case OP_CODE_REGIST_SUB:
            result = handle_tfs_register(client);
            printf("Ended subscriber sessions\n");
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

        // Major error ocurred and we should close the server
        if (result == -1) {

            close_server(EXIT_FAILURE);
            return NULL;
        }
    }
    return NULL;
}

int handle_tfs_register(client_t *client) {
    // Open client pipe
    int client_pipe;
    if (client->opcode == OP_CODE_REGIST_PUB) { // publisher
        printf("Registering publisher ...\n");
        client_pipe = open(client->client_pipename, O_RDONLY);
    } else { // subscriber
        printf("Registering subscriber ...\n");
        client_pipe = open(client->client_pipename, O_WRONLY);
    }
    printf("Client pipe opened: %s\n", client->client_pipename);
    if (client_pipe < 0) {
        printf("Failed to open pipe %s\n", client->client_pipename);
        return -1; // Major error, close server
    }

    client->client_pipe = client_pipe;

    // Try to get the box
    box_t *box = get_box(client->box_name);
    if (box == NULL) {
        printf("Box %s does not exist\n", client->box_name);
        safe_close(client_pipe);
        return 0;
    }
    client->box = box;

    // Check if the box already has a publisher
    if (client->opcode == OP_CODE_REGIST_PUB) {
        if (client->box->n_publishers == 1) {
            printf("Box %s already has a publisher\n", client->box_name);
            return finish_client_session(client);
        }
        client->box->n_publishers++;
        printf("Registered publisher %s for box %s\n", client->client_pipename,
               client->box_name);
        return handle_messages_from_publisher(client);
    } else {
        client->box->n_subscribers++;
        printf("Registered subscriber %s for box %s\n", client->client_pipename,
               client->box_name);
        if (handle_messages_until_now(client) == -1) {
            return -1;
        }
        return handle_messages_to_subscriber(client);
    }

    return -1;
}

void close_server(int exit_code) {

    printf("Major error ocurred, closing server...\n");

    for (int i = 0; i < max_sessions; ++i) {
        free_client(i);
        exit(EXIT_FAILURE);
    }
    pcq_destroy(&pc_queue);
    free(clients);
    pthread_mutex_destroy(&boxes_lock);
    exit(exit_code);
}
// destroy client session
int free_client(int session_id) {
    client_t *client = &clients[session_id];
    if (pthread_join(client->thread_id, NULL) != 0) {
        perror("Failed to join thread");
        return -1;
    }
    if (close(client->client_pipe) == -1) {
        perror("Failed to close pipe");
        return -1;
    }

    return 0;
}

int finish_client_session(client_t *client) {
    // close client pipe
    safe_close(client->client_pipe);

    if (client->opcode != OP_CODE_LIST_BOXES && (client->box_fd) == -1) {
        printf("Failed to close box %s\n", client->box_name);
        return -1;
    }

    if (client->box != NULL) {
        if (client->opcode == OP_CODE_REGIST_PUB) {
            client->box->n_publishers--;
        } else if (client->opcode == OP_CODE_REGIST_SUB) {
            client->box->n_subscribers--;
        }
    }
    client->box_name[0] = '\0';
    client->client_pipename[0] = '\0';
    client->box = NULL;
    client->box_fd = -1;

    return 0;
}

int parser(uint8_t op_code) {

    request_t *request = (request_t *)malloc(sizeof(request_t));

    if (request == NULL) {
        printf("Failed to allocate memory for request\n");
        return -1;
    }
    // check if op_code is valid
    if (op_code > 7) {
        printf("Invalid op_code\n");
        return -1;
    }
    request->opcode = op_code;
    char buffer[CLIENT_NAMED_PIPE_PATH_LENGTH];
    read_pipe(server_pipe, buffer, sizeof(buffer));
    strncpy(request->client_pipename, buffer, sizeof(buffer) + 1);

    if (op_code != OP_CODE_LIST_BOXES) {
        char buffer2[BOX_NAME_LENGTH];
        read_pipe(server_pipe, buffer2, sizeof(buffer2));
        strncpy(request->box_name, buffer2, sizeof(buffer2) + 1);
    }

    // Sends request to the queue to wait to be popped by a client session
    if (pcq_enqueue(&pc_queue, request) == -1) {
        printf("Failed to enqueue request\n");
        return -1;
    }

    return 0;
}

int remove_box(char *box_name) {
    for (int i = 0; i < MAX_BOXES; i++) {
        if (strcmp(boxes[i].box_name, box_name) == 0) {
            free_boxes[i] = true;
            if (tfs_unlink(boxes[i].box_name) == -1) {
                return -1;
            }
            boxes[i].box_name[0] = '\0';
            boxes[i].box_size = 0;
            boxes[i].n_publishers = 0;
            boxes[i].n_subscribers = 0;
            num_boxes--;
            return 0;
        }
    }
    return -1;
}

int get_free_box() {
    for (int i = 0; i < MAX_BOXES; ++i) {
        if (free_boxes[i] == true) {
            free_boxes[i] = false;
            return i;
        }
    }
    return -1;
}

box_t *get_box(char *box_name) {
    for (int i = 0; i < MAX_BOXES; ++i) {
        if (strcmp(boxes[i].box_name, box_name) == 0) {
            return &boxes[i];
        }
    }
    return NULL;
}

int handle_tfs_remove_box(client_t *client) {

    safe_mutex_lock(&boxes_lock);
    char error_msg[MESSAGE_LENGTH + 1] = {0};
    int32_t return_code = 0;
    uint8_t op_code = OP_CODE_REMOVE_BOX_ANSWER;

    int pipe = open(client->client_pipename, O_WRONLY);

    if (pipe == -1) {
        printf("Failed to open pipe: %s\n", client->client_pipename);
        safe_mutex_unlock(&boxes_lock);
        return -1;
    }

    client->client_pipe = pipe; // manager pipe

    // if box doesnt exist send error message to manager
    if (get_box(client->box_name) == NULL) {
        strcpy(error_msg, "Box does not exist");
        return_code = -1;
    }

    char box_name[BOX_NAME_LENGTH + 1] = {0};
    strcpy(box_name, client->box_name);

    // end all client sessions with this box
    if (return_code != -1) {
        for (int i = 0; i < max_sessions; ++i) {
            if (strcmp(clients[i].box_name, box_name) == 0) {
                if (i != client->session_id &&
                    finish_client_session(&clients[i]) == -1) {
                    return -1;
                }
            }
        }
    }

    if (return_code != -1 && remove_box(box_name) == -1) {
        return -1;
    }

    write_pipe(client->client_pipe, &op_code, sizeof(uint8_t));
    write_pipe(client->client_pipe, &return_code, sizeof(uint32_t));
    write_pipe(client->client_pipe, error_msg, sizeof(char) * MESSAGE_LENGTH);

    safe_mutex_unlock(&boxes_lock);

    return finish_client_session(client);
}

int handle_tfs_list_boxes(client_t *client) {
    // open client pipe
    int client_pipe = open(client->client_pipename, O_WRONLY);
    if (client_pipe < 0) {
        printf("Failed to open pipe\n");
        return -1;
    }

    client->client_pipe = client_pipe;

    char buffer[sizeof(uint8_t) * 2 + BOX_NAME_LENGTH + sizeof(uint64_t) * 3] =
        {0};
    uint8_t last = 0;

    if (num_boxes == 0) {
        last = 1;
    }
    int count = 0;
    int i = 0;
    uint8_t opcode = OP_CODE_LIST_BOXES_ANSWER;

    while (i < MAX_BOXES) {
        if (free_boxes[i] == true && num_boxes != 0) {
            i++;
            continue;
        }
        box_t box = boxes[i];
        count++;

        memcpy(buffer, &opcode, sizeof(uint8_t));

        if (count == num_boxes) {
            last = 1;
        }

        memcpy(buffer + sizeof(uint8_t), &last, sizeof(uint8_t));
        memcpy(buffer + sizeof(uint8_t) * 2, &box.box_name,
               sizeof(char) * BOX_NAME_LENGTH);
        if (num_boxes != 0) {
            memcpy(buffer + sizeof(uint8_t) * 2 +
                       sizeof(char) * BOX_NAME_LENGTH,
                   &box.box_size, sizeof(uint64_t));
            memcpy(buffer + sizeof(uint8_t) * 2 +
                       sizeof(char) * BOX_NAME_LENGTH + sizeof(uint64_t),
                   &box.n_publishers, sizeof(uint64_t));
            memcpy(buffer + sizeof(uint8_t) * 2 +
                       sizeof(char) * BOX_NAME_LENGTH + sizeof(uint64_t) * 2,
                   &box.n_subscribers, sizeof(uint64_t));
            write_pipe(client_pipe, buffer,
                       sizeof(uint8_t) * 2 + BOX_NAME_LENGTH +
                           sizeof(uint64_t) * 3);
        } else {
            write_pipe(client_pipe, buffer,
                       sizeof(uint8_t) * 2 + BOX_NAME_LENGTH);
        }

        i++;
    }

    // close client pipe
    if (finish_client_session(client) == -1) {
        printf("Failed to finish client session\n");
        return -1;
    }

    return 0;
}

int handle_tfs_create_box(client_t *client) {
    safe_mutex_lock(&boxes_lock);
    uint8_t opcode = OP_CODE_CREATE_BOX_ANSWER;
    int32_t return_code = 0;
    char error_msg[MESSAGE_LENGTH + 1] = {0};

    if (client->box_name == NULL) {
        snprintf(error_msg, MESSAGE_LENGTH, "Box name is null");
        return_code = -1;
    }

    if (return_code == 0 && num_boxes == MAX_BOXES) {
        snprintf(error_msg, MESSAGE_LENGTH, "Reached max number of boxes");
        return_code = -1;
    }

    if (return_code == 0 && get_box(client->box_name) != NULL) {
        snprintf(error_msg, MESSAGE_LENGTH, "Box already exists");
        return_code = -1;
    }

    if (return_code == 0 && create_box(client->box_name) < 0) {
        snprintf(error_msg, MESSAGE_LENGTH, "Failed to create box");
        return_code = -1;
    }
    safe_mutex_unlock(&boxes_lock);

    client->client_pipe = open(client->client_pipename, O_WRONLY);
    if (client->client_pipe < 0) {
        snprintf(error_msg, MESSAGE_LENGTH, "Failed to open pipe");
        return_code = -1;
    }

    int fhandle = tfs_open(client->box_name, TFS_O_CREAT);
    if (fhandle < 0) {
        printf("Failed to open box\n");
        return -1;
    } else {
        client->box_fd = fhandle;
    }

    write_pipe(client->client_pipe, &opcode, sizeof(uint8_t));
    write_pipe(client->client_pipe, &return_code, sizeof(uint32_t));
    write_pipe(client->client_pipe, error_msg, sizeof(char) * MESSAGE_LENGTH);

    // If at least one of them is -1 (not zero == true), return -1
    return finish_client_session(client);
}

int create_box(char *box_name) {

    box_t *tmp_box = (box_t *)malloc(sizeof(box_t));
    if (tmp_box == NULL) {
        return -1;
    }

    // put name in box
    memcpy(tmp_box->box_name, box_name, BOX_NAME_LENGTH + 1);
    tmp_box->box_name[BOX_NAME_LENGTH] = '\0';
    tmp_box->box_size = 0;
    tmp_box->n_publishers = 0;
    tmp_box->n_subscribers = 0;
    // initiate threadlock
    pthread_mutex_init(&tmp_box->lock, NULL);
    pthread_cond_init(&tmp_box->cond, NULL);

    num_boxes++;

    int box_id = get_free_box();
    if (box_id == -1) {
        printf("Can't create box: Max boxes already reached.\n");
        return -1;
    }
    boxes[box_id] = *tmp_box;

    return 0;
}

int handle_messages_from_publisher(client_t *client) {

    uint8_t opcode;
    char message[MESSAGE_LENGTH + 1];
    message[MESSAGE_LENGTH] = '\0';
    ssize_t bytes_read;
    ssize_t bytes_written;
    printf("Starting to read from publisher pipe (stdin) %s\n",
           client->client_pipename);
    while (true) {
        // Check if it returns EOF
        bytes_read = try_read(client->client_pipe, &opcode, sizeof(uint8_t));
        if (bytes_read == 0 || (bytes_read < 0 && errno == EPIPE)) { // EOF
            finish_client_session(client);
            return 0;
        } else if (bytes_read < 0) { // Error
            printf("Failed to read from pipe %s\n", client->client_pipename);
            finish_client_session(client);
            return -1;
        }

        bytes_read = try_read(client->client_pipe, message, MESSAGE_LENGTH);
        if (bytes_read == 0 || (bytes_read < 0 && errno == EPIPE)) { // EOF
            finish_client_session(client);
            return 0;
        } else if (bytes_read < 0) { // Error
            printf("Failed to read from pipe %s\n", client->client_pipename);
            finish_client_session(client);
            return -1;
        }

        box_t *box = client->box;

        safe_mutex_lock(&box->lock);
        int fhandle = tfs_open(box->box_name, TFS_O_APPEND);
        if (fhandle < 0) {
            printf("Failed to open box %s in tfs\n", box->box_name);
            finish_client_session(client);
            return -1; // Close the server
        }

        bytes_written = tfs_write(fhandle, message, strlen(message) + 1);
        if (bytes_written < 0) {
            printf("Failed to write to box %s in tfs\n", box->box_name);
            finish_client_session(client);
            return -1; // Close the server
        } else if (bytes_written < strlen(message) + 1) {
            printf("Box %s is full\n", box->box_name);
            finish_client_session(client);
            return 0; // Finish this session but the box can still be used
        }
        box->box_size += strlen(message) + 1;
        // Signal all subscribers
        if (pthread_cond_broadcast(&box->cond)) {
            printf("Failed to broadcast condition\n");
            finish_client_session(client);
            return -1; // Close the server
        }
        safe_mutex_unlock(&box->lock);
    }

    if (finish_client_session(client) == -1) {
        printf("Failed to finish client session\n");
        return -1;
    }
    return -1;
}

// read messages from subscriber's box and send to client
int handle_messages_to_subscriber(client_t *client) {
    char message[MESSAGE_LENGTH + 1];
    uint8_t opcode = OP_CODE_SUBSCRIBER;
    ssize_t bytes_read;
    ssize_t bytes_written;
    box_t *box = client->box;

    // Read each message from box and send to client
    // each message is terminated by '\0'
    while (true) {
        safe_mutex_lock(&box->lock);
        // Wait until there is more to read
        while ((box->box_size - (uint64_t)client->offset) == 0) {
            if (pthread_cond_wait(&box->cond, &box->lock)) {
                printf("Failed to wait on condition\n");
                finish_client_session(client);
                return -1; // Close the server
            }
        }
        safe_mutex_unlock(&box->lock);

        memset(message, 0, MESSAGE_LENGTH);
        bytes_read = tfs_read(client->box_fd, message, MESSAGE_LENGTH);
        if (bytes_read == 0) { // EOF
            break;
        } else if (bytes_read < 0) { // Error
            printf("Failed to read from box %s in tfs\n", box->box_name);
            finish_client_session(client);
            return -1; // Close the server
        }
        bytes_written =
            try_write(client->client_pipe, &opcode, sizeof(uint8_t));
        if (bytes_written == 0 || (bytes_written < 0 && errno == EPIPE)) {
            printf("Subscriber pipe closed: %d\n", client->client_pipe);
            finish_client_session(client);
            return 0; // Safely end this session
        } else if (bytes_written < 1) {
            printf("Failed to write to pipe %d\n", client->client_pipe);
            finish_client_session(client);
            return -1; // Close the server
        }

        bytes_written = try_write(client->client_pipe, message, MESSAGE_LENGTH);

        if (bytes_written == 0 || (bytes_written < 0 && errno == EPIPE)) {
            printf("Subscriber pipe closed: %d\n", client->client_pipe);
            finish_client_session(client);
            return 0; // Safely end this session
        } else if (bytes_written < MESSAGE_LENGTH) {
            printf("Failed to write to pipe %d\n", client->client_pipe);
            finish_client_session(client);
            return -1; // Close the server
        }
        client->offset +=
            bytes_read; // It's supposed to be equal to bytes_written
    }

    if (finish_client_session(client) == -1) {
        printf("Failed to finish client session\n");
        return -1; // Close the server
    }
    return 0;
}

int handle_messages_until_now(client_t *client) {
    safe_mutex_lock(&client->box->lock);

    // Have this size be the max nº of chars that can fit in a box (tfs file)
    char buffer[BOX_SIZE];
    char message[MESSAGE_LENGTH + 1];
    uint8_t opcode = OP_CODE_SUBSCRIBER;
    ssize_t bytes_read;
    ssize_t tmp_offset;
    client->box_fd = tfs_open(client->box_name, TFS_O_CREAT);
    if (client->box_fd < 0) {
        printf("Failed to open box %s in tfs\n", client->box->box_name);
        finish_client_session(client);
        return -1; // Close the server
    }
    while ((client->box->box_size - (uint64_t)client->offset) > 0) {

        bytes_read = tfs_read(client->box_fd, buffer, BOX_SIZE);
        if (bytes_read == 0) { // EOF
            break;
        } else if (bytes_read < 0) { // Error
            printf("Failed to read from box %s in tfs\n",
                   client->box->box_name);
            finish_client_session(client);
            return -1; // Close the server
        }

        tmp_offset = 0;
        while (tmp_offset < bytes_read) {
            memset(message, 0, MESSAGE_LENGTH + 1);
            // Copy starting from the last '\0' character until the next '\0'
            // character
            strcpy(message, buffer + tmp_offset);

            write_pipe(client->client_pipe, &opcode, sizeof(uint8_t));
            write_pipe(client->client_pipe, message, MESSAGE_LENGTH);
            tmp_offset += (ssize_t)strlen(message) + 1;
        }

        client->offset += bytes_read;
    }
    safe_mutex_unlock(&client->box->lock);

    return 0;
}
