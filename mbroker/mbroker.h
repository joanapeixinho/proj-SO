#ifndef MBROKER_H
#define MBROKER_H

#include "common/common.h"
#include "config.h"
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>



/* box_t is the structure that is used to communicate between the broker and the
 * clients. */

typedef struct {
    uint8_t opcode; 
    char box_name[BOX_NAME_LENGTH + 1]; 
    int fhandle; //file handle for the box
    uint64_t box_size; //total length of the messages in the box
    char *buffer; //buffer to store the message
    uint64_t n_publishers; //0 or 1 max
    uint64_t n_subscribers; 
} box_t;

/* client_t is the structure that is used to store the information about the
 * client sessions */

typedef struct client {
    int session_id;
    box_t box;
    char client_pipename[CLIENT_NAMED_PIPE_PATH_LENGTH + 1];
    int client_pipe;
    bool to_do;
    pthread_t thread_id;
    pthread_cond_t cond;
    pthread_mutex_t lock;
} client_t;


// server functions
int mbroker_init();
void close_server(int exit_code);

// client session functions
void *client_session(void *client_in_array);
int free_client(int session_id);
int get_free_client();

//handle functions
int handle_tfs_register(client_t *client);
int handle_tfs_create_box(client_t *client);
int handle_tfs_create_box_answer(client_t *client);
int handle_tfs_remove_box(client_t *client);
int handle_tfs_remove_box_answer(client_t *client);
int handle_tfs_list_boxes(client_t *client);
int handle_tfs_list_boxes_answer(client_t *client);
int handle_tfs_write(client_t *client);

// parser functions
int parser(char op_code, int parser_fnc(client_t *client));
int parse_box(client_t *client);
int parse_list(client_t *client);

#endif