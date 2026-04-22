#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

/*
sudo chown root:root superdog
sudo chmod u+s superdog
*/

int main(int argc, char* argv[])
{
    int fd;

    if(argc > 1)
    {
        fd = open(argv[1], O_RDONLY);
    }
    else
    {
        fd = open("/etc/shadow", O_RDONLY);
    }

    if(fd < 0)
    {
        perror("fd je negativen?");
        exit(1);
    }
    
    printf("UID: %d\n", getuid());
	printf("EUID: %d\n", geteuid());    
    
    char buf[4096];
    ssize_t prebrano = 0;
    
    while((prebrano = read(fd, buf, 4096)) > 0)
    {
        write(STDOUT_FILENO, buf, prebrano);
    }
    
    
    return 0;
}