#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>


/* This test tries to delete an open file and then tries to delete 
the same file after it is closed. */

int main() {
    
    char *path1 = "/f1";

    assert(tfs_init(NULL) != -1);

    int f1 = tfs_open(path1, TFS_O_CREAT);
    assert(f1 != -1);
    
    //it's not possible to delete a file that is still open    
    assert(tfs_unlink(path1) == -1);

    //closing the file
    assert(tfs_close(f1) != -1);

    //try to delete the same file that is now closed
    assert(tfs_unlink(path1) != -1);

    assert(tfs_destroy() == 0); 

    printf("Successful test.\n");

    return 0;
}
