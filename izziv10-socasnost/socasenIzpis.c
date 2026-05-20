#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

// Napiši program v C ali Javi, ki ustvari N niti, katerih imena naj bodo števila od 0 do N-1. Niti naj zažene in nato počaka, da se vse končajo. Število niti N je prvi argument programa.
// 
// a) Vsaka nit naj M-krat izpiše svoje ime (nato se konča). Število izpisov M je drugi argument programa. Med izpis dodaje kakšen sleep ali drug sistemski klic.
// 
// b) Vsaka nit naj neprestano izpisuje svoje ime v "neskončni" zanki. Zanka se izvaja, dokler je spremenljivka running enaka true. Glavni program naj po ustvarjenju niti zaspi za M (drugi argument, privzeta vrednost 5) sekund, nato naj niti nežno pokonča in jih počaka. Kje moramo uporabiti volatile?

#define PODNALOGA_B

typedef struct {
    int            thread_name;
    int            M;
    volatile bool* running;
} ThreadData;

#ifdef PODNALOGA_A
void* worker(void* arg){
    ThreadData tdata = *(ThreadData*)arg;
    for(int i = 0; i < tdata.M; i++){
        printf("%d\n", tdata.thread_name);
        sleep(1);
    }
    pthread_exit(NULL);
}
#endif

#ifdef PODNALOGA_B
void* worker(void* arg){
    ThreadData tdata = *(ThreadData*)arg;
    while(*tdata.running){
        printf("%d\n", tdata.thread_name);
        sleep(1);
    }
    pthread_exit(NULL);
}
#endif

int main(int argc, char* argv[]){
    int N = 10;
    if(argc > 1) N = atoi(argv[1]);
    int M = 5;
    if(argc > 2) M = atoi(argv[2]);
    

    pthread_t threads[N];
    ThreadData data[N];
    volatile bool running = true;
    
    for(int i = 0; i < N; i++){
        data[i].thread_name = i;
#ifdef PODNALOGA_A
        data[i].M = M;
#endif
#ifdef PODNALOGA_B
        data[i].running = &running;
#endif
        if (pthread_create(&threads[i], NULL, worker, &data[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

#ifdef PODNALOGA_B
    sleep(M);
    running = false;
#endif

    for(int i = 0; i < N; i++){
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}