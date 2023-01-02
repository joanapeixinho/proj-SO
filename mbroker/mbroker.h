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
    int fhandle;
    size_t len;
    char *buffer;
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
    bool bool_cond;
    pthread_t thread_id;
    pthread_cond_t cond;
    pthread_mutex_t lock;
} client_t;



#endif