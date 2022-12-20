#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


/* This creates a file, a soft link for it and a hard link for it and 
tries to create more links with same paths  */

char const target_path1[] = "/f1";
char const hard_link_path_1[] = "/l1";
char const hard_link_path_2[] = "/l1";
char const soft_link_path_1[] = "/l2";
char const soft_link_path_2[] = "/l2";

int main() {
    assert(tfs_init(NULL) != -1);

    // create file
    int f = tfs_open(target_path1, TFS_O_CREAT);
    assert(f != -1);
    assert(tfs_close(f) != -1);

    // create hard link
    assert(tfs_link(target_path1, hard_link_path_1) != -1);

    //try to create another hard link with same path
    assert(tfs_link(target_path1, hard_link_path_2) == -1);

    //try to create symlink with same path as hard link
    assert(tfs_sym_link(target_path1, hard_link_path_1) == -1);

    //create symlink
    assert(tfs_sym_link(target_path1, soft_link_path_1) != -1);
    
    //try to create another symlink with same path
    assert(tfs_sym_link(target_path1, soft_link_path_2) == -1);
    
   //try to create hard link with same path as symlink
    assert(tfs_link(target_path1, soft_link_path_1) == -1);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
