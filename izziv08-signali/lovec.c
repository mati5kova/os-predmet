#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>


volatile int energija = 42;
volatile char znakNaSekundo = '.';

void sigchld_handler(int signum) {
    int pid, status;
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {q
        if (WIFEXITED(status)) {
            printf("Zombie caught with status: %d\n", WEXITSTATUS(status));
        }
    }
}

void sigterm_handler(int signum){
    energija += 10;
    printf("Yahoo! Bonus energy (%d)\n", energija);
}

void sigusr1_handler(int signum){    
    znakNaSekundo = znakNaSekundo == '.' ? '*' : '.';
}


void sigusr2_handler(int signum){
    int pid = fork();
    if(pid > 0) { // stars
        printf("Forked child %d\n", pid);        
    } else if(pid == 0) {
        sleep((energija % 7) + 1);
        int exStatus = (42 * energija) % 128;
        printf("Child exit with status: %d\n", exStatus);
        exit(exStatus);
    }
}


int main(int argc, char* argv[]) {
    
    int pid = getpid();
    printf("My PID: %d\n", pid);
    
    
    if(argc > 1) energija = atoi(argv[1]);
    
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);
    
    while(energija > 0) {
        energija--;
        fflush(stdout);
        printf("%c", znakNaSekundo);
        sleep(1);
    }
    
    printf("\nOut of energy, Aggghhhhhrrrr.\n");
    
    return 0;
}
