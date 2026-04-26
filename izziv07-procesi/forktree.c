#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    int N = 3;

    if (argc > 1) {
        N = atoi(argv[1]);
    }

    int id = 0;

    for (int level = 0; level < N; level++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            id = id * 2 + 1;
        } else {
            id = id * 2;
        }
    }

    printf("%d\n", id);

    sleep(13);

    return 0;
}