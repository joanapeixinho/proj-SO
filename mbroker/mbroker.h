#ifndef MBROKER_H
#define MBROKER_H

#include "common/common.h"
#include "config.h"
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>




/* box_t is the structure that is used to communicate between the broker and the
 * clients. */

typedef struct {
    char opcode; 
    char client_pipename[PIPE_STRING_LENGTH + 1]; 
    char box_name[BOX_NAME_LENGTH + 1]; 
    int fhandle; //file handle for the box
    size_t len; //total length of the messages in the box
    char *buffer; //buffer to store the message
    int n_publishers = 0; //0 or 1 max
    int n_subscribers = 0; 
} box_t;

/*enum client type */
typedef enum  {
    PUBLISHER,
    SUBSCRIBER, 
    MANAGER
} client_type;

typedef struct client {
    int session_id;
    box_t box;
    int client_pipe;
    client_type type;
    bool to_do;
    pthread_t thread_id;
    pthread_cond_t cond;
    pthread_mutex_t lock;
} client_t;

int mbroker_init();
void *client_session(void *client_in_array);
void close_server(int exit_code);
int free_client(int session_id);
int get_free_client();
int handle_tfs_register(client_t *client);
int handle_tfs_create_box(client_t *client);
int handle_tfs_create_box_answer(client_t *client);
int handle_tfs_remove_box(client_t *client);
int handle_tfs_remove_box_answer(client_t *client);
int handle_tfs_list_boxes(client_t *client);
int handle_tfs_list_boxes_answer(client_t *client);
int handle_tfs_write(client_t *client);


#endif