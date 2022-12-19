#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*This test demonstrates that our implementation of
tfs_copy_from_external_fs returns -1 when file is too big to 
copy entirely*/

int main() {
    char *path_copied_file = "/f1";
    char *path_src = "tests/file_to_copy_over1024.txt";

    assert(tfs_init(NULL) != -1);

    // try copy file over 1024 bytes 
    assert(tfs_copy_from_external_fs(path_src, path_copied_file) == -1);

    
    printf("Successful test.\n");

    return 0;
}
