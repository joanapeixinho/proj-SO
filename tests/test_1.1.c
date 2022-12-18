#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>

// C program to compare two files and report
// mismatches by displaying line number and
// position of line.
#include<stdio.h>
#include<string.h>
#include<stdlib.h>

int main() {
    char *path1 = "/f1";
    char *path2 = "/";
    char *path_src = "tests/file_to_copy.txt";
    char buffer[40];

    assert(tfs_init(NULL) != -1);

    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);
    assert(tfs_close(f1) != -1);

    // destination file is a directory
    assert(tfs_copy_from_external_fs(path_src, path2) == -1);

    // destination file does not exist
    assert(tfs_copy_from_external_fs(path_src, "/unexistent") != -1);

    // destination file is a symbolic link, test if the original file is
    // overwritten

    assert(tfs_sym_link(path1, "/l1") == 0);
    assert(tfs_copy_from_external_fs(path_src, "/l1") != -1);
   
   //idea: test if soft link file is overwritten 
    //idea: test if file is overwritten when using copy_from_external_fs on a soft link


    printf("Successful test.\n");

    return 0;
}
