#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>


void *thread_func_write() {
    char *str = "AAAAAAAA!";
    char *path = "/f1";

    int f;
    ssize_t r;

    f = tfs_open(path, 0);
    assert(f != -1);
    
    //write to the file
    r = tfs_write(f, str, strlen(str));
    //check if the write was successful
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);
    return  NULL;
}

//read file loop

void *thread_func_read () {
    char *path = "/f1";
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



int main() {

    pthread_t tread_1;
    pthread_t tread_2;
    pthread_t twrite_1;
    pthread_t twrite_2;

    //test if program is thread safe by creating multiple threads that read and write to the file system

    //initialize the file system
    assert(tfs_init(NULL) != -1);

    //create a thread that writes to the file
    assert(pthread_create(&twrite_1, NULL, thread_func_write, NULL) != 0);

    //create a thread that writes to the file
    assert(pthread_create(&twrite_2, NULL, thread_func_write, NULL) != 0);

    //create a thread that reads from the file
    assert(pthread_create(&tread_1, NULL, thread_func_read, NULL) != 0);

    //create a thread that reads from the file
    assert(pthread_create(&tread_2, NULL, thread_func_read, NULL) != 0);

    //wait for the threads to finish
    assert(pthread_join(twrite_1, NULL) != 0);
    assert(pthread_join(twrite_2, NULL) != 0);
    assert(pthread_join(tread_1, NULL) != 0);
    assert(pthread_join(tread_2, NULL) != 0);

    //destroy the file system
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
