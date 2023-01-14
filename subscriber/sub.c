#include "logging.h"
#include <stdbool.h>
#include "../utils/common.h"
#include <stdint.h>
#include <signal.h>

int message_count = 0;
int pipe_fd;
void sigint_handler(int sig);

int main(int argc, char **argv) {
  
    if (argc != 4) {
        fprintf(stderr, "Usage: sub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }
    
    char* register_pipe = argv[1];
    char* pipe_name = argv[2];
   
    char* buffer = parse_message(OP_CODE_REGIST_SUB, pipe_name, argv[3]);
    int register_pipe_fd = open(register_pipe, O_WRONLY);
    if (register_pipe_fd < 0) {
        fprintf(stderr, "Failed to open register pipe %s\n", register_pipe);
        return -1;
    }
    //Try to register the subscriber
    write_pipe(register_pipe_fd, buffer, sizeof(uint8_t) + (CLIENT_NAMED_PIPE_PATH_LENGTH+BOX_NAME_LENGTH)*sizeof(char));
    
    if (unlink(pipe_name) < 0) {
        fprintf(stderr, "Failed to delete pipe %s\n", pipe_name);
        return -1;
    }
    
    //create the pipe
    if (mkfifo(pipe_name, 0777) < 0) {
        fprintf(stderr, "Failed to create pipe %s\n", pipe_name);
        return -1;
    }

    pipe_fd = open(pipe_name, O_RDONLY);
    if (pipe_fd < 0) {
        fprintf(stderr, "Failed to open pipe %s\n", pipe_name);
        return -1;
    }
    
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fprintf(stderr, "Failed to register SIGINT handler\n");
        return -1;
    }

    uint8_t op_code;
    char message[MESSAGE_LENGTH + 1];      //The message has max 255 chars + a '\0' at the end

    
    while(true){
        //Each message from the pipe is a new line
        read_pipe(pipe_fd, &op_code, sizeof(uint8_t));
        read_pipe(pipe_fd, message, MESSAGE_LENGTH*sizeof(char));
        message[MESSAGE_LENGTH] = '\0';
        message_count++;
        fprintf(stdout, "%s\n", message);
    }
    return -1;
}


void sigint_handler(int sig) {
    
    if (sig == SIGINT) {
   
    fflush(stdout);
    
    char error_message[MESSAGE_LENGTH + 1] = "Received SIGINT. Closing pipe and exiting...\n";
    ssize_t bytes_written = write(1, error_message, strlen(error_message) + 1);
    
    //close session
    if (close(pipe_fd) < 0) {
        bytes_written = write(2, "Failed to close pipe\n", strlen("Failed to close pipe\n") + 1);
        return;
    }

    //print number of messages received
    char final_message[MESSAGE_LENGTH + 1] = "Received ";
    char count[sizeof(int)];
    sprintf(count,"%d",message_count);
    strcat(final_message, count);
    strcat(final_message, " messages\n");
    bytes_written = write(1, final_message, strlen(final_message) + 1);

    //ignore bytes_written (to avoid unused variable warning)
    (void) bytes_written;

    //restore default handler
    signal(sig, SIG_DFL);
    raise(sig);
    
    }
    else {
        
        fflush(stderr);
        char error_message[MESSAGE_LENGTH + 1] = "Received unexpected signal ";
        char unexpected_sig[sizeof(int)];
        sprintf(unexpected_sig,"%d",sig);
        strcat(error_message,unexpected_sig);
        
        //write error message to stderr
        ssize_t bytes_written = write(2, error_message, strlen(error_message) + 1);
        if (close(pipe_fd) < 0) {
            bytes_written= write(2, "Failed to close pipe\n", strlen("Failed to close pipe\n") + 1);
            return;
        }
        //ignore bytes_written (to avoid unused variable warning)
        (void) bytes_written;
        //restore default handler
        signal(sig, SIG_DFL);
        raise(sig);
    }
}


//Path: subscriber/sub.c
