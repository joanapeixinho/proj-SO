#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include "common.h"

ssize_t try_read(int fd, void *buf, size_t count) {
    ssize_t bytes_read;
    do {
        bytes_read = read(fd, buf, count);
    } while (bytes_read < 0 && errno == EINTR);
    return bytes_read;
}

ssize_t try_write(int fd, const void *buf, size_t count) {
    ssize_t bytes_written;
    do {
        bytes_written = write(fd, buf, count);
    } while (bytes_written < 0 && errno == EINTR);
    return bytes_written;
}

char * parse_message(u_int8_t opcode, char * pipename, char * box_name ) {
    
    char buffer [CLIENT_NAMED_PIPE_PATH_LENGTH  + BOX_NAME_LENGTH + 1] = {0};
    char * tmp = buffer;
    
    memcpy(tmp, &opcode, sizeof(uint8_t));
    tmp++;                 //Move the pointer to the next position
    memcpy(tmp, pipename, CLIENT_NAMED_PIPE_PATH_LENGTH);
    tmp += CLIENT_NAMED_PIPE_PATH_LENGTH;   //Move the pointer to the next position
    
    if (box_name != NULL) {
        memcpy(tmp, box_name, BOX_NAME_LENGTH);
    }
    
    return buffer;
}