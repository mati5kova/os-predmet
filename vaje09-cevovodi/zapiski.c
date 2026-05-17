#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

//ls | grep xyz > bla.txt
int main() {

    int fd[2];
    pipe(fd);
    
    if(fork() == 0) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        execlp("ls", "ls", NULL);
        perror("exec"); exit(42);
    }
    
    if(fork() == 0) {
        int bla_fd = open("bla.txt", O_WRONLY | O_CREAT, 0644);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        dup2(bla_fd, STDOUT_FILENO);
        close(bla_fd);
        close(fd[1]);
        execlp("grep", "grep", "zap", NULL);
        perror("exec"); exit(42);
        
    }
    
    close(fd[0]);
    close(fd[1]);
    
    if (0) printf("here\n");
    if (1) printf("there\n");
    
    wait(NULL);
    wait(NULL);
    
        
}