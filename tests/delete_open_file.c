#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>

int main() {
    char *path1 = "/f1";

    assert(tfs_init(NULL) != -1);

    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);
    //try to delete a file that is still open    
    assert(tfs_unlink(path1) == -1);

    assert(tfs_close(f1) != -1);
    //try to delete the same file that is now closed
    assert(tfs_unlink(path1) != -1);

    printf("Successful test.\n");

    return 0;
}
