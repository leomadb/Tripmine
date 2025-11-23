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

// Inside the Cube Namespace
static int child_function(void *arg) {
    
    char *args[] = {
        "./shell",
        NULL
    };

    char *env[] = {
        "TRIP_PATH='/bin/'",
        "PROMPT='false'",
        NULL
    };

    execve("./shell", args, env);

    return 0;
}

int main() {
    printf("[Host] Starting cube...\n");

    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        printf("Malloc Failed!\n");
        exit(1);
    }

    // Prepare execution
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("Pipe creation failed");
        exit(1);
    }

    pid_t pid1, pid2;

    // Start execution
    pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        pid_t pid = clone(child_function,
        stack + STACK_SIZE,
        CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD,
        NULL);
    
        if (pid == -1) {
            perror("Clone Failed.\n");
            exit(1);
        }

        printf("[Host] Cube spawned with PID: %d\n", pid);

        // Run test command
        char *cmd = "echo Hello World!\n";
        write(pipefd[1], cmd, strlen(cmd));

        // Exit
        cmd = "exit\n";
        write(pipefd[1], cmd, strlen(cmd));

        close(pipefd[1]);
    }
    

    // Wait for the cube to finish
    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    
    printf("[Host] Cube terminated.\n");
    free(stack);
    return 0;
}