#include  <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/file.h>

struct pthreadData {
    int id;
    pthread_rwlock_t* lock;
    FILE* f;
};

void* inc(void* arg) {

    struct pthreadData data = *(struct pthreadData*)arg;
    pthread_rwlock_t* lock = data.lock;
    int id = data.id;
    FILE* f = data.f;

    char buffer[256];

    pthread_rwlock_wrlock(lock);

    rewind(f);
    fgets(buffer, sizeof(buffer)/sizeof(buffer[0]), f);
    int prebrano = atoi(buffer);
    printf("Inc: %d: prebrano %d\n", id, prebrano);

    prebrano++;
    sleep(1);

    rewind(f);
    fprintf(f, "%d", prebrano);
    fflush(f);
    printf("Inc: %d: zapisano %d\n", id, prebrano);

    pthread_rwlock_unlock(lock);

    return NULL;
}

void* dec(void* arg) {

    struct pthreadData data = *(struct pthreadData*)arg;
    pthread_rwlock_t* lock = data.lock;
    int id = data.id;
    FILE* f = data.f;

    char buffer[256];

    pthread_rwlock_wrlock(lock);

    rewind(f);
    fgets(buffer, sizeof(buffer)/sizeof(buffer[0]), f);
    int prebrano = atoi(buffer);
    printf("Dec: %d: prebrano %d\n", id, prebrano);

    prebrano--;
    sleep(1);

    rewind(f);
    fprintf(f, "%d", prebrano);
    fflush(f);
    printf("Dec: %d: zapisano %d\n", id, prebrano);

    pthread_rwlock_unlock(lock);

    return NULL;
}

int main(int argc, char* argv[]) {

    int N = 100;
    if (argc > 1) N = atoi(argv[1]);

    pthread_t pthreads[2*N];
    struct pthreadData threadsData[2*N];

    FILE* f = fopen("dat.txt", "r+");

    pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

    for (int i = 0; i < 2*N; i++)
    {
        threadsData[i].id = i;
        threadsData[i].lock = &lock;
        threadsData[i].f = f;
        if (i % 2 == 0)
        {
            pthread_create(&pthreads[i], NULL, inc, &threadsData[i]);
        } else
        {
            pthread_create(&pthreads[i], NULL, dec, &threadsData[i]);
        }
    }

    for (int i = 0; i < 2*N; i++)
    {
        pthread_join(pthreads[i], NULL);
    }

    fclose(f);

    return 0;
}