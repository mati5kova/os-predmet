#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#define __stdout__ ((int)1)
#define __stderr__ ((int)2)

int main(int argc, char* argv[]) {
    int count = 42;
    if(argc > 1) count = atoi(argv[1]);
    
    char const buff[] = "Juhuhu, pomlad je tu.\n";
    char const buff2[] = "Zima prihaja!\n";
    
    int i = 0;
    int j = 0;
    while(i + j < count && (i < strlen(buff) || j < strlen(buff2))) {
        if(i < strlen(buff))
            write(__stdout__, &buff[i++], 1);
        if(j < strlen(buff2))
            write(__stderr__, &buff2[j++], 1);
    }
    
    
    return i + j;
}
