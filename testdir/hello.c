#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void* routine() {
    printf("Hello from threads\n");
    sleep(3);
    printf("Ending thread\n");
}

int main(int argc, char* argv[]) {
    pthread_t p1, p2;
    pthread_create(&p1, NULL, &routine, NULL);
    pthread_create(&p2, NULL, &routine, NULL);
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    return 0;
}