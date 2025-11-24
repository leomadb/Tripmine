#define _GNU_SOURCE // Required for unshare, pivot_root definitions
#include <sched.h>   // For unshare()
#include <sys/mount.h> // For mount()
#include <unistd.h>  // For fork(), exec*, chdir(), syscall()
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h> // For syscall(SYS_pivot_root, ...)

struct CubeConfig {
    int in_pipe;
    int out_pipe;
    int cube_id;
    char* image_name;
    char** commands;
};

static int child_function(void *args) {
    struct CubeConfig *config = (struct CubeConfig *)args;

    // Pipe
    dup2(config->in_pipe, STDIN_FILENO);
    dup2(config->out_pipe, STDOUT_FILENO);
    dup2(config->out_pipe, STDERR_FILENO);
    
    close(config->in_pipe);
    close(config->out_pipe);

    setup_fs(config);
    enter_jail(config->cube_id);

    char* env[] = {
        "TRIP_PATH=/bin",
        "PROMPT=false",
        NULL
    };

    execve("/bin/sh", NULL, env);
    
    return 0;
}

// Recursive directory function
void mkdir_p(const char *path) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755); // Ignore error if exists
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// Define the pivot_root system call wrapper since glibc doesn't have one
int pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

// Enter the jail
void enter_jail(int cube_id) {
    char new_root[256];
    char old_root[256];
    
    sprintf(new_root, "/var/tripmine/cubes/%d/", cube_id);
    sprintf(old_root, "%s/old_root", new_root);

    mkdir(old_root, 0755);
    
    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("Self bind-mount failed");
        exit(1);
    }
    
    printf("[Cube %d] Pivotting root...\n", cube_id);
    if (pivot_root(new_root, old_root) != 0) {
        perror("Pivot failed");
        exit(1);
    }

    chdir("/");
    printf("[Cube %d] Pivot done.\n", cube_id);
    
    if (umount2("/old_root", MNT_DETACH) != 0) {
        perror("Old root unmount failed");
        exit(1);
    }

    rmdir("/old_root");

    printf("[Cube %d] Locked inside. Ready to proceed.\n", cube_id);
}

// Setup the filesystem for the cube.
void setup_fs(struct CubeConfig *config) {
    // Important 
    char host_mirrors[256], host_frame[256];
    char cube_root[256], target_bin[256], target_lib[256];
    char* shell_path = "/var/tripmine/bin/shell";
    char target_shell[256];

    /*
        TODO:
        1. Parse args
        2. Create a new mount namespace *
        3. Mount *
        4. Load the dependencies into /bin/ of the namespace *
        5. Pivot the root to the new root *
        6. Execute the command
    */

    int cube_id = config->cube_id;
    char* image_name = config->image_name;

    // Prepare for the new namespace
    sprintf(host_mirrors, "/var/tripmine/images/%s/mirrors/", image_name);
    sprintf(host_frame, "/var/tripmine/images/%s/frames/", image_name);
    sprintf(cube_root, "/var/tripmine/cubes/%d/", cube_id);
    sprintf(target_bin, "%s/bin/", cube_root);
    sprintf(target_lib, "%s/lib/", cube_root);
    sprintf(target_shell, "%s/bin/sh", cube_root);
    
    mkdir_p(cube_root);
    mkdir_p(target_bin);
    mkdir_p(target_lib);

    // Mount Mirrors->/bin/ (READ ONLY)
    if (mount(host_mirrors, target_bin, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("Mirror mounting failed");
        exit(1);
    }
    mount(NULL, target_bin, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY, NULL);

    // Mount Frame->/lib/ (READ WRITE)
    if (mount(host_frame, target_lib, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("Frame mounting failed");
        exit(1);
    }

    // Mount shell into /bin/sh
    int fd = open(target_shell, O_CREAT, 0755);
    if (fd >= 0) {
        close(fd);
    }

    if (mount(shell_path, target_shell, NULL, MS_BIND, NULL) != 0) {
        perror("Shell mounting failed");
        exit(1);
    }
    mount(NULL, target_shell, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY, NULL);

    printf("[Cube %d] FS Steup done.\n", cube_id);
    printf("  %s -> /bin (RO)\n", host_mirrors);
    printf("  %s -> /lib (RW)\n", host_frame);
    printf("[Cube %d] Cube is ready to proceed.\n", cube_id);
}

int main(int argc, char *argv[]) {
    struct CubeConfig config;
    config.cube_id = argv[1];
    config.image_name = argv[2];
    for (int i = 3; i < argc; i++) {
        config.commands[i - 3] = argv[i];
    }
    config.commands[argc - 3] = NULL;

    int pipein[2];
    int pipeout[2];
    if (pipe(pipein) == -1 || pipe(pipeout) == -1) {
        perror("pipe");
        exit(1);
    }

    config.in_pipe = pipein[0];
    config.out_pipe = pipeout[1];

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        close(pipein[0]);
        close(pipeout[1]);
        child_function(&config);
    } else {
        close(pipein[1]);
        close(pipeout[0]);
    }

    char buffer[1024];
    int n;
    while ((n = read(pipeout[0], buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, n);
    }
    close(pipeout[0]); 

    waitpid(pid, NULL, 0);
    close(pipein[0]);
    close(pipeout[1]);

    return 0;
}
