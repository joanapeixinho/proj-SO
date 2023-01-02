#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/types.h>

#define PIPE_BUFFER_MAX_LEN (PIPE_BUF)
#define PIPE_NAME_LENGTH (255)
#define BOX_NAME_LENGTH (31)
#define ERROR_MESSAGE_LENGTH (1024)
#define MESSAGE_LENGTH (1024)


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

/* check if all the content was read from the pipe. */
#define read_pipe(pipe, buffer, size)                                          \
    if (try_read(pipe, buffer, size) != size) {                                \
        return -1;                                                             \
    }

/* check if all the content was written to the pipe. */
#define write_pipe(pipe, buffer, size)                                         \
    if (try_write(pipe, buffer, size) != size) {                               \
        return -1;                                                             \
    }

#endif /* COMMON_H */