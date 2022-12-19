#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


/* This test writes to a target file using a hard link for it and 
copies an empty file to that target using a soft link */

char const target_path1[] = "/f1";
char const link_path1[] = "/l1";
char const link_path2[] = "/l2";
char const empty_file_path[] = "tests/empty_file.txt";
uint8_t const file_contents[] = "AAA!";

void assert_empty_file(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[20];
    assert(tfs_read(f, buffer, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

int main() {
    assert(tfs_init(NULL) != -1);

    // Write to symlink and read original file
    {
        int f = tfs_open(target_path1, TFS_O_CREAT);
        assert(f != -1);
        assert(tfs_close(f) != -1);
        assert_empty_file(target_path1); // sanity check

    }

    // create hard link
    assert(tfs_link(target_path1, link_path2) != -1);

    //write contents in target using hard link
    write_contents(link_path2);

    //create symlink
    assert(tfs_sym_link(target_path1, link_path1) != -1);
    
    // copy empty file to symlink
    int f2 = tfs_copy_from_external_fs(empty_file_path, link_path1);

    assert(f2 == 0);
    
    // check that the original file is empty after copying 
    assert_empty_file(target_path1);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
