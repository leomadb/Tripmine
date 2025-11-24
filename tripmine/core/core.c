#define _GNU_SOURCE // Required for unshare, pivot_root definitions
#include <sched.h>   // For unshare()
#include <sys/mount.h> // For mount()
#include <unistd.h>  // For fork(), exec*, chdir(), syscall()
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h> // For syscall(SYS_pivot_root, ...)
#include <sys/wait.h>
#include <sys/stat.h>

struct CubeConfig {
    int in_pipe;
    int cube_id;
    char* image_name;
    char* commands[16];
    int memory;
};

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
    char* system_path = "/var/tripmine/images/%s/system";
    char target_system[256];

    int cube_id = config->cube_id;
    char* image_name = config->image_name;

    // Prepare for the new namespace
    sprintf(host_mirrors, "/var/tripmine/images/%s/mirrors/", image_name);
    sprintf(host_frame, "/var/tripmine/images/%s/frames/", image_name);
    sprintf(cube_root, "/var/tripmine/cubes/%d/", cube_id);
    sprintf(target_bin, "%s/bin/", cube_root);
    sprintf(target_lib, "%s/lib/", cube_root);
    sprintf(target_shell, "%s/bin/sh", cube_root);
    sprintf(system_path, "/var/tripmine/images/%s/system", image_name);
    sprintf(target_system, "%s/usr/", cube_root);
    
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

    // Mount system into /usr/ (READ WRITE)
    if (mount(system_path, target_system, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("System mounting failed");
        exit(1);
    }

    printf("[Cube %d] FS Steup done.\n", cube_id);
    printf("  %s -> /bin (RO)\n", host_mirrors);
    printf("  %s -> /lib (RW)\n", host_frame);
    printf("[Cube %d] Cube is ready to proceed.\n", cube_id);
}

static int child_function(void *args) {

    fprintf(stderr, "!!! CHILD IS ALIVE (PID: %d) !!!\n", getpid());

    struct CubeConfig *config = (struct CubeConfig *)args;

    setup_fs(config);
    enter_jail(config->cube_id);

    // Pipe
    dup2(config->in_pipe, STDIN_FILENO);
    
    close(config->in_pipe);

    setvbuf(stdout, NULL, _IONBF, 0); 
    setvbuf(stderr, NULL, _IONBF, 0);

    char* env[] = {
        "TRIP_PATH=/bin",
        "PROMPT=false",
        NULL
    };

    execve("/bin/sh", NULL, env);
    
    return 0;
}

int main(int argc, char *argv[]) {
    struct CubeConfig config;
    config.cube_id = atoi(argv[1]);
    printf("[Core] Cube ID: %d\n", config.cube_id);
    config.image_name = argv[2];
    printf("[Core] Image Name: %s\n", config.image_name);
    config.memory = atoi(argv[3]);
    printf("[Core] Memory Limit: %d\n", config.memory);
    
    size_t x = 4;
    while (x < argc) {
        config.commands[x - 4] = argv[x];
        x++;
    }
    config.commands[x - 4] = NULL;
    for (size_t i = 0; i < x - 4; i++) {
        printf("[Core] Command %zu: %s\n", i, config.commands[i]);
    }

    int pipein[2];
    if (pipe(pipein) == -1) {
        perror("pipe");
        exit(1);
    }

    config.in_pipe = pipein[0];

    printf("[Core] Spawning cube %d...\n", config.cube_id);

    char stack[config.memory];
    pid_t pid = clone(
        child_function, 
        stack + sizeof(stack), 
        CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNET, 
        &config);
    if (pid == -1) {
        perror("clone");
        exit(1);
    }

    for (size_t i = 0; i < x - 4; i++) {
        write(pipein[1], config.commands[i], strlen(config.commands[i]));
    }
    
    close(pipein[0]); 

    waitpid(pid, NULL, 0);
    close(pipein[1]);

    return 0;
}
