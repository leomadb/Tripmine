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
#include <errno.h>      // 
#include <utils.h>

#define STACK_SIZE (1024 * 1024)

struct CubeConfig {
    int pipe_write_fd;
    char *binary_path;
    char **binary_args;
    char *hostname;
};

// Inside the Cube Namespace
static int child_function(void *arg) {
    struct CubeConfig *config = (struct CubeConfig *)args;

    if (dup2(config->pipe_write_fd, STDOUT_FILENO) == -1) perror("dup2 stdout");
    if (dup2(config->pipe_write_fd, STDERR_FILENO) == -1) perror("dup2 stderr");
    close(config->pipe_write_fd);

    // Active Flushing
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

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

    printf("[Cube] Launching %s inside %s...\n", config->binary_path, config->hostname);

    // Execute
    execve(config->binary_path, config->binary_args, env);

    // If we get here, execve failed.
    perror("Execve Failed");
    return 1;
}

void parse_args(/*int argc,*/ /*char *argv[],*/ struct CubeConfig *cfg) {
    // Defaults
    cfg->binary_path = "./shell";
    cfg->hostname = randomName();
    cfg->binary_args = malloc(2 * sizeof(char*));
    cfg->binary_args[0] = "./shell";
    cfg->binary_args[1] = NULL;

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

    return 0;
}