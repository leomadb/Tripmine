#define _GNU_SOURCE

#include <sched.h>      // clone(), unshare(), CLONE_* flags
#include <sys/mount.h>  // mount(), MS_* flags
#include <sys/wait.h>   // waitpid()
#include <sys/syscall.h>// syscall() for pivot_root
#include <unistd.h>     // execve(), sethostname()
#include <sys/stat.h>   // mkdir()
#include <fcntl.h>      // open(), O_WRONLY
#include <stdio.h>      // printf(), perror()
#include <stdlib.h>     // malloc(), free(), exit()
#include <string.h>     // strerror()
#include <errno.h>      // errno

#define STACK_SIZE (1024 * 1024)

struct CubeConfig {
    int pipe_write_fd;
    char command_list[64];
    char *hostname;
};

// Inside the Cube Namespace
static int child_function(void *arg) {
    struct CubeConfig *config = (struct CubeConfig *)arg;

    if (dup2(config->pipe_write_fd, STDIN_FILENO) == -1) perror("dup2 stdout");
    close(config->pipe_write_fd);

    // Isolation
    if (sethostname(config->hostname, strlen(config->hostname)) != 0) {
        perror("sethostname failed");
    }

    // TODO: Mounts and Pivot_Root would go here in Phase 2
    
    // Setup Environment
    char *env[] = { 
        "TRIP_PATH=/", 
        "HOME=/", 
        "TERM=xterm-256color", 
        "PROMPT='false'",
        NULL 
    };

    char *args[] = {
        "./shell",
        NULL
    };

    // Actively flush output while execve

    // Execute Commands
    if (execve("./shell", args, env) == -1) {
        perror("execve failed");
        exit(1);
    };

    for (int i = 0; config->command_list[i] != NULL; i++) {
        // This loop is a placeholder for executing commands
        // Actual command execution logic would go here
        write(config->pipe_write_fd, config->command_list[i], strlen(config->command_list[i]));
        write(config->pipe_write_fd, "\n", 1);
    }

    close(config->pipe_write_fd);

    return 1;
}

void parse_args(/*int argc,*/ /*char *argv[],*/ struct CubeConfig *cfg) {
    // Defaults
    static char *default_commands[] = { "echo", "Hello", "World!", NULL };
    memcpy(cfg->command_list, default_commands, sizeof(default_commands));
    cfg->hostname = "tripmine";
    // TODO: Parsing
}

int main(/*int argc, char *argv[]*/) {
    struct CubeConfig config;
    parse_args(&config);

    // Make pipes
    int log_pipe[2];
    if (pipe(log_pipe) == -1) { perror("pipe"); exit(1); }
    config.pipe_write_fd = log_pipe[1];

    // Clone
    char *stack = malloc(STACK_SIZE);
    pid_t pid = clone(child_function,
        stack + STACK_SIZE,
        CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD,
        &config
    );

    if (pid == -1) { perror("Clone Failed"); exit(1); }

    close(log_pipe[1]);
    waitpid(pid, NULL, 0);

    return 0;
}