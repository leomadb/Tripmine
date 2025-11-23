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
    printf("[Cube] You are inside a Tripmine Cube! How cool is that?\n");

    // TODO: Setup Hostname
    // TODO: Mount /proc
    // TODO: Pivot Root
    // TODO: Execve bash

    char *args[] = {
        "/bin/bash",
        NULL
    };

    char *env[] = {
        "PATH=/.vscode/",
        "HOME=/",
        NULL
    };

    int result = execve("/bin/bash", args, env);

    if (result == -1) {
        perror("[Cube] Failed!\n");
        exit(1);
    }

    return 0;
}

int main() {
    printf("[Host] Starting cube...\n");

    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        printf("Malloc Failed!\n");
        exit(1);
    }

    pid_t pid = clone(child_function,
        stack + STACK_SIZE,
        CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD,
        NULL);
    
    if (pid == -1) {
        perror("Clone Failed.\n");
        exit(1);
    }

    printf("[Host] Cube spawned with PID: %d\n", pid);

    // Wait for the cube to finish
    waitpid(pid, NULL, 0);
    
    printf("[Host] Cube terminated.\n");
    free(stack);
    return 0;
}