#ifndef MBROKER_H
#define MBROKER_H

#include "../producer-consumer/producer-consumer.h"
#include "../utils/common.h"
#include "config.h"
#include "fs/operations.h"
#include "logging.h"
#include "string.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* box_t is the structure that is used to communicate between the broker and the
 * clients. */
typedef struct {
    char box_name[BOX_NAME_LENGTH + 1];
    uint64_t box_size;     // total length of the messages in the box
    uint64_t n_publishers; // 0 or 1 max
    uint64_t n_subscribers;
    pthread_cond_t cond; // condition variable to wake up the subscribers
    pthread_mutex_t lock;
} box_t;

/* client_t is the structure that is used to store the information about the
 * client sessions */
typedef struct client {
    uint8_t opcode;
    int session_id;
    char box_name[BOX_NAME_LENGTH + 1];
    box_t *box;
    ssize_t offset; // offset in the box (for the subscribers)
    int box_fd;     // file descriptor of the box (for subscribers)
    char client_pipename[CLIENT_NAMED_PIPE_PATH_LENGTH + 1];
    int client_pipe;
    pthread_t thread_id;
} client_t;

// server functions
int mbroker_init();
void close_server(int exit_code);

// client session functions
void *client_session(void *client_in_array);
int free_client(int session_id);
int free_client_session(int session_id);
int get_free_client();
int finish_client_session(client_t *client);

// handle functions
int handle_tfs_register(client_t *client);
int handle_tfs_create_box(client_t *client);
int handle_tfs_create_box_answer(client_t *client);
int handle_tfs_remove_box(client_t *client);
int handle_tfs_remove_box_answer(client_t *client);
int handle_tfs_list_boxes(client_t *client);
int handle_tfs_list_boxes_answer(client_t *client);
int handle_tfs_write(client_t *client);
int handle_messages_from_publisher(client_t *client);
int handle_messages_to_subscriber(client_t *client);
int handle_messages_until_now(client_t *client);

// parser functions
int parser(uint8_t op_code);
int parse_box(client_t *client);
int parse_list(client_t *client);

// box functions
int create_box(char *box_name);
int compare_box_names(void *box, void *box_name);
int compare_boxes(const void *box1, const void *box2);
box_t *get_box(char *box_name);
int remove_box(char *box_name);
int get_free_box();

#endif