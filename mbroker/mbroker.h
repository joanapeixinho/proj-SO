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
    char client_pipe[PIPE_STRING_LENGTH + 1];
    char box_name[PIPE_STRING_LENGTH + 1];
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
    box_t *box;
    int fdclient;
    client_type type;

} client_t;



#endif