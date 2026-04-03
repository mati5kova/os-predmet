#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>

#define V3


int main(int argc, char* argv[]) {
    int vhod = 0;   // stdin
    int izhod = 1;  // stdout

    // vhod
    if (argc > 1) {
        if (strcmp(argv[1], "-") != 0) {
            vhod = open(argv[1], O_RDONLY);
            if (vhod < 0) {
                perror("Napaka pri odpiranju vhodne datoteke");
                exit(1);
            }
        }
    }

    // izhod
    if (argc > 2) {
        izhod = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (izhod < 0) {
            perror("Napaka pri odpiranju izhodne datoteke");
            exit(1);
        }
    }
    
    char buff[1] = "";
    while(read(vhod, buff, 1)) {
        write(izhod, buff, 1);
    }
    
    return 0;
}