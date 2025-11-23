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
    int in_pipe;
    int out_pipe;
    char **command_list;
    char *hostname;
    char *cwd;
    char *env[];
};

// Inside the Cube Namespace
static int child_function(void *arg) {
    struct CubeConfig *config = (struct CubeConfig *)arg;

    // TODO: Execution
    

    return 1;
}

void parse_args(int argc, char *argv[] struct CubeConfig *cfg) {
    // Defaults
    cfg->env = [
        "TRIP_PATH=/",
        "HOME=/",
        "TERM=xterm-256color",
        "PROMPT=false",
        NULL
    ];
    cfg->hostname = "tripmine";
    cfg->cwd = "/tripmine/";
    
    // TODO: Parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hostname") == 0) {
            cfg->hostname = argv[i + 1];
            cfg->cwd = malloc(strlen("/" + strlen(cfg->hostname) + strlen("/"));
            cfg->cwd = strcat("/", cfg->hostname, "/");
        } else if (strcmp(argv[i], "--env") == 0) {
            cfg->env = argv[i + 1];
        } else if (strcmp(argv[i], "--command") == 0) {
            cfg->command_list = argv[i + 1];
        }
    }
}

int main(int argc, char *argv[]) {
    // Define and structure
    struct CubeConfig config;
    parse_args(argc, argv, &config);
    
    char *stack = malloc(STACK_SIZE);
    pid_t pid = clone(child_function,
        stack + STACK_SIZE,
        CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | SIGCHLD,
        &config
    );

    if (pid == -1) { perror("Clone Failed"); exit(1); }

    close(log_pipe[1]);
    waitpid(pid, NULL, 0);

    return 0;
}