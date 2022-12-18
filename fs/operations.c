#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h> //remove this later
#include  <pthread.h>
#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr; 
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }
    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    // TODO: assert that root_inode is the root directory
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    
    
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }
    
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    
    
    int inum = tfs_lookup(name, root_dir_inode);
    
    size_t offset;

    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        if(inode_get_type(inum) == T_SYMLINK && inode->i_data_block != -1) {
            name = get_target_file(inode);
            inum = tfs_lookup(name, root_dir_inode);
            if (inum == -1 ) {
                return -1;
            }
            inode = inode_get(inum);
        }

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } 
    
    else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode  
            inum = inode_create(T_FILE);
        

        if (inum == -1) {
            return -1; // no space in inode table
        }
        
        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {

    
    int inum = inode_create(T_SYMLINK);
    inode_t *root_dir = inode_get(ROOT_DIR_INUM);

    if (inum == -1 ) {
        return -1;
    }

    inode_t *inode = inode_get(inum);
    
    if (inode == NULL) {
        return -1;
    }
    

    add_dir_entry(root_dir, link_name + 1 , inum);
    

    int fhandle = tfs_open(link_name, TFS_O_APPEND);
    tfs_write(fhandle, target, strlen(target));
    
    tfs_close(fhandle); 
    
    return 0;
}


//create a hard link

int tfs_link(char const *target, char const *link_name) {
    
    if (!valid_pathname(target) || !valid_pathname(link_name) ) {
        return -1;
    } 

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    //get the inumber of the target file
    int inum = tfs_lookup(target, root_dir_inode);
    //printf( "type in creating hard link = %d\n", inode_get_type(inum));
    if (inum == -1 || inode_get_type(inum) == T_SYMLINK) {
        return -1;
    }
    if (add_dir_entry(root_dir_inode, link_name + 1, inum) == -1) {
        return -1;
    }
    inc_link_count(inum);
    return 0;
}


int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");
    
    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
     
    // From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }
    
    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }
    
    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    
    if (!valid_pathname(target)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int inum = tfs_lookup(target, root_dir_inode);

    if (inum == -1 || inum == ROOT_DIR_INUM) {
        return -1;
    }

    if (inode_get_type(inum) == T_DIRECTORY) {
        return -1;
    }

    // if file is opened its name should not be deleted
    if (is_in_open_file_table(inum) != -1 && inode_get_link_count(inum) == 1){
        return -1;
    }

    //remove the entry from the directory
    if (clear_dir_entry(root_dir_inode, target + 1) == -1) {
        return -1;
    }
    //decrement the link count
    dec_link_count(inum);

    if (inode_get_link_count(inum) == 0) {
            inode_delete(inum);
    }

    return 0;

}


    //copy contents from source_path existing on fyle system to dest_path in tfs
    //if file exists, overwrite it with new contents from source_path
    //return 0 on success, -1 on failure
int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
   
    //open source file
    FILE * source_fd = fopen(source_path, "r");
    if (source_fd == NULL) {
        return -1;
    }

    //open dest file
    int dest_fd = tfs_open(dest_path, TFS_O_CREAT | TFS_O_TRUNC);
    
    if (dest_fd == -1) {
        return -1;
    }

    //read from source file and copy to dest file
    char buffer[128];
    memset(buffer,0,sizeof(buffer));
    size_t bytes_read;
    while ((bytes_read = fread(buffer, sizeof(char), 128, source_fd)) > 0) {
        if (tfs_write(dest_fd, buffer, bytes_read* sizeof(char)) == -1) {
            return -1;
        }
    }

    //close source file
    fclose(source_fd);
    //close dest file
    tfs_close(dest_fd);
    return 0;
}
