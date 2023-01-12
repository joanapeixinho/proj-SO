#include "logging.h"
#include <stdbool.h>
#include <common/common.h>
#include <stdint.h>
#include <signal.h>

int message_count = 0;
int pipe_fd;

int main(int argc, char **argv) {
  
    if (argc != 4) {
        fprintf(stderr, "Usage: sub <register_pipe> <pipe_name> <box_name>\n");
        return -1;
    }
    
    char* register_pipe = argv[1];
    char* pipe_name = argv[2];
   
    char* buffer = parse_mesage(OP_CODE_REGIST_SUB, pipe_name, argv[3]);
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
    char* message[MESSAGE_LENGTH + 1];      //The message has max 255 chars + a '\0' at the end
    size_t len = MESSAGE_LENGTH; 
    
    while(true){
        //Each message from the pipe is a new line
        read_pipe(pipe_fd, &op_code, sizeof(uint8_t));
        read_pipe(pipe_fd, message, MESSAGE_LENGTH*sizeof(char));
        message_count++;
        fprintf(stdout, "%s\n", message);
    }
    return -1;
}


void sigint_handler(int sig) {
    if (sig == SIGINT) {

    fflush(stdout);
    
    char *error_message = "Received SIGINT. Closing pipe and exiting...\n";
    write(1, error_message, strlen(error_message) + 1);
    
    //close session
    safe_close(pipe_fd);
    //print number of messages received
    char *final_message = "Received ";
    final_message = concat(final_message, int_to_string(message_count));
    final_message = concat(final_message, " messages\n");
    write(1, final_message, strlen(final_message) + 1);

    //restore default handler
    signal(sig, SIG_DFL);
    raise(sig);
    
    }
    else {

        fflush(stderr);
        char* error_message = "Received unexpected signal ";
        error_message = concat(error_message, int_to_string(sig));
        
        //write error message to stderr
        write(2, error_message, strlen(error_message) + 1);
        safe_close(pipe_fd);

        //restore default handler
        signal(sig, SIG_DFL);
        raise(sig);
    }
}


//Path: subscriber/sub.c
