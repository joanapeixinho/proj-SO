#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include "common.h"
#include <stdlib.h>
#include <string.h>


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

//parse message from client
char * parse_message(u_int8_t opcode, char * pipename, char * box_name ) {
    
    char* buffer = (char* ) malloc(sizeof(uint8_t) + CLIENT_NAMED_PIPE_PATH_LENGTH + BOX_NAME_LENGTH);
    if (buffer == NULL) {
        printf("Failed to allocate memory\n");
        return NULL;
    }

    char * tmp = buffer;
    
    memcpy(tmp, &opcode, sizeof(uint8_t));
    tmp++;  
    
    //if pipename length is bigger than CLIENT_NAMED_PIPE_PATH_LENGTH, it will be truncated              
    
    memcpy(tmp, pipename, CLIENT_NAMED_PIPE_PATH_LENGTH);
    tmp += CLIENT_NAMED_PIPE_PATH_LENGTH;   
    
    if (box_name != NULL) {
        //if box_name length is bigger than BOX_NAME_LENGTH, it will be truncated
        memcpy(tmp, box_name, BOX_NAME_LENGTH);
    }
    
    return buffer;
}