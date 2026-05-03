#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

int main(const int argc, char* argv[]) {
    const char* path = ".";
    if (argc > 1) path = argv[1];

    DIR* dirp = NULL;
    if ((dirp = opendir(path)) == NULL)
    {
        const int saved_errno = errno;
        perror("opendir");
        return saved_errno;
    }

    struct dirent* dirent;
    errno = 0;
    while ((dirent = readdir(dirp)) != NULL)
    {
        char type = 'x';
        switch (dirent->d_type)
        {
        case DT_BLK: {
            type = 'b';
            break;
        }
        case DT_CHR: {
            type = 'c';
            break;
        }
        case DT_DIR: {
            type = 'd';
            break;
        }
        case DT_FIFO: {
            type = 'p';
            break;
        }
        case DT_LNK: {
            type = 'l';
            break;
        }
        case DT_REG: {
            type = 'r';
            break;
        }
        case DT_SOCK: {
            type = 's';
            break;
        }
        default: {
            type = 'u';
            break;
        }
        }

        fprintf(stdout, "[d_ino: %10llu] [d_type: %c] [d_name: %s]\n", (unsigned long long)dirent->d_ino, type, dirent->d_name);
    }

    if (errno != 0) {
        const int saved_errno = errno;
        perror("readdir");
        closedir(dirp);
        return saved_errno;
    }

    if (closedir(dirp) < 0)
    {
        const int saved_errno = errno;
        perror("closedir");
        return saved_errno;
    }

    return 0;
}