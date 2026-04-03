#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>

#define V3


int main(int argc, char* argv[]) {
    
    #ifdef V1
    for(int i = 1; i < argc; i++) {
        printf("%s\n", argv[i]);
    }
    #endif
    
    #ifdef V2
    for(int i = 1; i < argc; i++) {
        write(1, argv[i], strlen(argv[i]));
        write(1, "\n", 1);
    }
    #endif
    
    #ifdef V3
    for(int i = 1; i < argc; i++) {
        syscall(SYS_write, 1, argv[i], strlen(argv[i]));
        syscall(SYS_write, 1, "\n", 1);
    }
    #endif
    
    return 0;
}