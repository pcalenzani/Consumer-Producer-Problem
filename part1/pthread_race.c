#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <semaphore.h>

//  Threads and Iterations
#define MAX_THREADS 16
#define MAX_ITERATIONS 40
// Text color values
#define TXTCYAN "\x1B[36m"
#define TXTYELLOW "\x1B[33m"
#define TXTRESET "\x1B[0m"

// Parameter for nanosleep function
struct timespec ts = {0, 250000};

void* thread_func(void*);
void t_add(int);
void t_sub(int);

// GLOBAL will be accessed and shared by every thread created
int GLOBAL = 0;
sem_t mutex;

int
main(int argc, char** argv)
{
    // Mutex semaphore initialized
    sem_init(&mutex, 0, 1);
    pthread_t ids[MAX_THREADS];

    for (int i = 0; i < MAX_THREADS; ++i)
    {
        int idx = i;
        pthread_create(&ids[i], NULL, thread_func, (void *)&idx);
        // My computer has a problem with id numbers getting duplicated
        // This sleep removes that problem nearly every time
        nanosleep(&ts, NULL);
    }
        
    for (int i = 0; i < MAX_THREADS; ++i)
    {
        pthread_join(ids[i], NULL);
    }

    printf("Final Value of Shared Variable: %d\n", GLOBAL);
    // Destroy semaphore after use
    sem_destroy(&mutex);
    return 0;
}

// Thread function will save thread id and call the corresponding function if its odd or even
void* thread_func(void* threads)
{
    int idx = *((int*)threads);
    
    // The mutex lock only allows one process in the critical section at a time
    sem_wait(&mutex);
    /* Critical Section */
    if (idx % 2 == 0)
    {   // EVEN
        for (int i = 0; i < MAX_ITERATIONS; ++i)
        {
            t_add(idx);
        }
    }
    else
    {   // ODD
        for (int i = 0; i < MAX_ITERATIONS; ++i)
        {
            t_sub(idx);
        }
    }
    /* ----------- */
    sem_post(&mutex);
}

// Thread adder will add 10 to global variable
void t_add(int idx)
{
    nanosleep(&ts, NULL);

    int temp = GLOBAL;
    nanosleep(&ts, NULL);
    temp += 10;
    GLOBAL = temp;

    printf(TXTYELLOW "Current Value written to Global Variables by ADDER thread id: %d is %d\n" TXTRESET, idx, GLOBAL);
}

// Thread subtractor will subtract 10 from global variable
void t_sub(int idx)
{
    int temp = GLOBAL;
    temp -= 10;
    GLOBAL = temp;

    printf(TXTCYAN "Current Value written to Global Variables by SUBTRACTOR thread id: %d is %d\n" TXTRESET, idx, GLOBAL);
}
