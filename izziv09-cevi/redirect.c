#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/errno.h>

int main(int argc, char* argv[]) {
    
    if(argc < 4) {
        fprintf(stderr, "Usage: %s vhod/- izhod/- prog arg1? arg2? ...", argv[0]);
        exit(1);
    }
    
    int pid = fork();
    if(pid < 0) {
        int err = errno;
        perror("fork");
        exit(err);
    } else if(pid == 0){
        if(strcmp(argv[1], "-") != 0) { // vhod preusmerjen
            int redirInFd = open(argv[1], O_RDONLY);
            if(redirInFd == -1) {
                int err = errno;
                perror("open");
                exit(err);
            }
            if(dup2(redirInFd, STDIN_FILENO) == -1) {
                int err = errno;
                perror("dup2");
                exit(err);
            }
            close(redirInFd);
        }
        if(strcmp(argv[2], "-") != 0) { // izhod preusmerjen
            int redirOutFd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(redirOutFd == -1) {
                int err = errno;
                perror("open");
                exit(err);
            }
            if(dup2(redirOutFd, STDOUT_FILENO) == -1) {
                int err = errno;
                perror("dup2");
                exit(err);
            }          
            close(redirOutFd);
        }
        
        execvp(argv[3], &argv[3]);
        int err = errno;
        perror("execvp");
        exit(err);
    }
    
    int wstatus;
    if(waitpid(pid, &wstatus, 0) < 0) {
        int err = errno;
        perror("waitpid");
        exit(err);
    }
    
    if(WIFEXITED(wstatus)) {
        exit(WEXITSTATUS(wstatus));
    }
    
    if (WIFSIGNALED(wstatus)) {
        fprintf(stderr, "Child terminated by signal %d\n", WTERMSIG(wstatus));
        exit(128 + WTERMSIG(wstatus));
    }

    return 1;
}