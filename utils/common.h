#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "betterassert.h"


#define PIPE_BUFFER_MAX_LEN (PIPE_BUF)
#define CLIENT_NAMED_PIPE_PATH_LENGTH (255)
#define BOX_NAME_LENGTH (31)
#define ERROR_MESSAGE_LENGTH (1024)
#define MESSAGE_LENGTH (1023)
#define MAX_BOXES (64) //max number of boxes == max files of file system


/* operation codes */
enum {
    OP_CODE_REGIST_PUB = 1,
    OP_CODE_REGIST_SUB = 2,
    OP_CODE_CREATE_BOX = 3,
    OP_CODE_CREATE_BOX_ANSWER = 4,
    OP_CODE_REMOVE_BOX = 5,
    OP_CODE_REMOVE_BOX_ANSWER = 6,
    OP_CODE_LIST_BOXES = 7,
    OP_CODE_LIST_BOXES_ANSWER = 8,
    OP_CODE_PUBLISHER = 9,
    OP_CODE_SUBSCRIBER = 10,
}; 

/*  TécnicoFS file opening modes */
typedef enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
} tfs_file_mode_t;


/*
 * Same as POSIX's read, but handles EINTR correctly.
 */

ssize_t try_read(int fd, void *buf, size_t count);

/*
 * Same as POSIX's write, but handles EINTR correctly.
 */
ssize_t try_write(int fd, const void *buf, size_t count);

char * parse_message(u_int8_t opcode, char * pipename, char * box_name );


/* check if all the content was read from the pipe. */
#define read_pipe(pipe, buffer, size)                                          \
    if (try_read(pipe, buffer, size) != size) {                                \
        printf("Failed to read from pipe %d", pipe);                           \
        return -1;                                                             \
    }

#define write_pipe(pipe, buffer, size)\
    if (try_write(pipe, buffer, size) != size) {\
        printf("Failed to write to pipe %d", pipe);\
        return -1;\
}

#define safe_close(fd)\
    if (close(fd) < 0)  {\
        printf("Failed to write to close file %d", fd);\
        return -1;\
    }

#define safe_mutex_lock(mutex)\
    ALWAYS_ASSERT(pthread_mutex_lock(mutex) != 0, "Failed to lock mutex %p", mutex)
                                                                

#define safe_mutex_unlock(mutex)\
    ALWAYS_ASSERT(pthread_mutex_unlock(mutex) != 0, "Failed to unlock mutex %p", mutex)

#endif /* COMMON_H */