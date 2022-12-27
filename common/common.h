#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/types.h>

/* operation codes */
enum {
    TFS_OP_CODE_REGIST_PUB = 1,
    TFS_OP_CODE_REGIST_SUB = 2,
    TFS_OP_CODE_CREATE_BOX = 3,
    TFS_OP_CODE_CREATE_BOX_ANSWER = 4,
    TFS_OP_CODE_REMOVE_BOX = 5,
    TFS_OP_CODE_REMOVE_BOX_ANSWER = 6,
    TFS_OP_CODE_LIST = 7
    TFS_OP_CODE_LIST_ANSWER = 8
}; 

#define PIPE_STRING_LENGTH (256)
#define BOX_NAME_LENGTH (32)

#define PIPE_BUFFER_MAX_LEN (PIPE_BUF)

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