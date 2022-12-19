#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#define THREAD_NUM 10
#define BUFFER_LEN 128
#define INPUT_FILE "tests/large_file.txt"
#define TFS_FILE "/f1"	

/* This test fills a file with large random content, and then attempts to read 
from it simultaneously in multiple threads, comparing the content 
with the original source. */


void write_fn() {

    FILE *fd = fopen(INPUT_FILE, "r");
    assert(fd != NULL);

    char buffer[BUFFER_LEN];

    int f = tfs_open(TFS_FILE, TFS_O_CREAT | TFS_O_TRUNC);
    assert(f != -1);

    ssize_t r;
    size_t bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);

    while (bytes_read > 0) {
        // write the contents of the file to the tfs file
        r = tfs_write(f, buffer, bytes_read);
        assert(r == bytes_read);
        bytes_read = fread(buffer, sizeof(char), BUFFER_LEN, fd);
    }

    assert(tfs_close(f) == 0);
    assert(fclose(fd) == 0);
}

void *read_fn(void *input) {
    (void)input; // ignore parameter

    FILE *fd = fopen(INPUT_FILE, "r");
    assert(fd != NULL);

    char buffer_external[BUFFER_LEN];
    char buffer_tfs[BUFFER_LEN];

    int f = tfs_open(TFS_FILE, 0);
    assert(f != -1);

    size_t bytes_read_external =
        fread(buffer_external, sizeof(char), BUFFER_LEN, fd);
    ssize_t bytes_read_tfs = tfs_read(f, buffer_tfs, BUFFER_LEN);
    while (bytes_read_external > 0 && bytes_read_tfs > 0) {
        assert(strncmp(buffer_external, buffer_tfs, BUFFER_LEN) == 0);
        bytes_read_external =
            fread(buffer_external, sizeof(char), BUFFER_LEN, fd);
        bytes_read_tfs = tfs_read(f, buffer_tfs, BUFFER_LEN);
    }

    // check if both files reached the end
    assert(bytes_read_external == 0);
    assert(bytes_read_tfs == 0);

    assert(tfs_close(f) == 0);
    assert(fclose(fd) == 0);

    return NULL;
}

int main() {
    assert(tfs_init(NULL) != -1);

    pthread_t tid[THREAD_NUM];

    write_fn();
    for (int i = 0; i < THREAD_NUM; i++) {
        assert(pthread_create(&tid[i], NULL, read_fn, NULL) == 0);
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        assert(pthread_join(tid[i], NULL) == 0);
    }

    assert(tfs_destroy() == 0);
    printf("Successful test.\n");

    return 0;
}
