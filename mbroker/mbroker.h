#ifndef MBROKER_H
#define MBROKER_H

#include "../utils/common.h"
#include "../utils/linkedlist.h"
#include "../fs/operations.h"
#include "config.h"
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>
#include <fcntl.h>
#include "logging.h"
#include "string.h"
#include <errno.h>

typedef struct {
    uint8_t opcode;
    char client_pipename[CLIENT_NAMED_PIPE_PATH_LENGTH + 1];
    char box_name[BOX_NAME_LENGTH + 1];
} request_t;


/* box_t is the structure that is used to communicate between the broker and the
 * clients. */
typedef struct {
    char box_name[BOX_NAME_LENGTH + 1]; 
    uint64_t box_size; //total length of the messages in the box
    uint64_t n_publishers; //0 or 1 max
    uint64_t n_subscribers;
    bool new_message; //true if there is a message to read
    pthread_cond_t cond; //condition variable to wake up the subscribers
    pthread_mutex_t lock;
} box_t;

/* client_t is the structure that is used to store the information about the
 * client sessions */
typedef struct client {
    uint8_t opcode; 
    int session_id;
    char* box_name;
    box_t *box;
    int box_fd; //file descriptor of the box (for subscribers)
    char client_pipename[CLIENT_NAMED_PIPE_PATH_LENGTH + 1];
    int client_pipe;
    bool to_do;
    pthread_t thread_id;
    pthread_cond_t cond; //condition variable to wake up the client
    pthread_mutex_t lock;
} client_t;


// server functions
int mbroker_init();
void close_server(int exit_code);

// client session functions
void* client_session(void *client_in_array);
int free_client(int session_id);
int get_free_client();
int free_client_session(int session_id);

//handle functions
int handle_tfs_register(client_t *client);
int handle_tfs_create_box(client_t *client);
int handle_tfs_create_box_answer(client_t *client);
int handle_tfs_remove_box(client_t *client);
int handle_tfs_remove_box_answer(client_t *client);
int handle_tfs_list_boxes(client_t *client);
int handle_tfs_list_boxes_answer(client_t *client);
int handle_messages_from_publisher(client_t *client);
int handle_messages_to_subscriber(client_t *client);


// parser functions
int parser(uint8_t op_code, int parser_fnc(client_t *client));
int parse_client_and_box(client_t *client);
int parse_client(client_t *client);
int parse_list(client_t *client);

// box functions
int create_box(char *box_name);
box_t* get_box(char *box_name);
int compare_box_names(const void *box,const void *box_name);


#endif