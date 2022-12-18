#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>


void *thread_read_file() {
    char *path = "tests/file_to_delete.txt";
    char buffer[5];
    
    int f;
    ssize_t r;

    f = tfs_open(path, 0);
    assert(f != -1);

    //read from the file and check if the read was successful
    while( (r = tfs_read(f, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[r] = '\0';
        assert(r == strlen(buffer));
    }

    assert(tfs_close(f) != -1);
    return NULL;

}

//delete file

void *thread_func_delete_file () {
    char *path = "tests/file_to_delete.txt";
    assert(tfs_delete(path) != -1);
    return NULL;

}



int main() {

    pthread_t tread;
    pthread_t tdelete;

    //initialize the file system
    assert(tfs_init(NULL) != -1);

    //create a thread that reads from the file
    assert(pthread_create(&tread, NULL, thread_read_file, NULL) != 0);

    //create a thread that deletes the file
    assert(pthread_create(&tdelete, NULL, thread_delete_file, NULL) != 0);

    //wait for the threads to finish
    assert(pthread_join(tread, NULL) != 0);
    assert(pthread_join(tdelete, NULL) != 0);

    //destroy the file system
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
