#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

// Text color values
#define TXTCYAN "\x1B[36m"
#define TXTGREEN "\x1B[32m"
#define TXTRED "\x1B[31m"
#define TXTRESET "\x1B[0m"

// Parameter for nanosleep function
struct timespec ts = {0, 500000};
// Mutex locks
sem_t mutex;
sem_t full;
sem_t empty;
// Buffered queue
int* QUEUE;
int first = -1; // Next item in the queue
int top = -1; // Last item in the queue
// Parameter global variables
int N; // Buffer size
int P; // Producers
int C; // Consumers
int X; // Items per producer
int XC; // Items per consumer
int OC; // Over consume amount
int Ptime; // Producer sleep time
int Ctime; // Consumer sleep time
// Testing strategy arrays
int* PRODUCED;
int* CONSUMED;

/* FUNCTION DEFINITIONS */
void* produce(void*);
void* consume(void*);
int enqueue_item(int);
int dequeue_item();
void printTime(time_t);

int main(int argc, char** argv) {
    // If less than required parameters, exit
    if (argc < 6)
    {
        printf("Error: Not enough arguments.");
        return -1;
    }

    // Save in-line parameters to variables
    N = atoi(argv[1]);
    P = atoi(argv[2]);
    C = atoi(argv[3]);
    X = atoi(argv[4]);
    Ptime = atoi(argv[5]);
    Ctime = atoi(argv[6]);
    XC = P*X/C; // Items consumed by each consumer
    OC = P*X - C*XC;
    // Initialize Buffered Queue
    QUEUE = calloc(N, sizeof(int));
    // Initialize testing arrays
    PRODUCED = calloc(P*X, sizeof(int));
    CONSUMED = calloc(P*X, sizeof(int));

    // Start time output
    time_t t1 = time(NULL);
    printTime(t1);
    // Parameter output
    printf("\n\tNumber of Buffers: %d\n", N);
    printf("\tNumber of Producers: %d\n", P);
    printf("\tNumber of Consumers: %d\n", C);
    printf("\tNumber of Items Produced: %d\n", X);
    printf("\tNumber of Items Consumed: %d\n", XC);
    printf("\tOver consume amount: %d\n", OC);
    printf("\tProducer sleep time: %d\n", Ptime);
    printf("\tConsumer sleep time: %d\n\n", Ctime);

    // Initialize Semaphores
    sem_init(&mutex, 0, 1);
    sem_init(&empty, 0, 0); // Starts locked
    sem_init(&full, 0, N); // Starts unlocked dependent on the buffer slots
    // Thread arrays for Ps and Cs
    pthread_t prod_ids[P];
    pthread_t cons_ids[C];

    // Spawn producer threads
    for (int i = 1; i <= P; ++i)
    {
        int idx = i;
        pthread_create(&prod_ids[i], NULL, produce, (void *)&idx);
        // ! My computer is fast enough that it generates duplicate threads
        // ! This nanosleep ensures that each thread has separate id's
        nanosleep(&ts, NULL);
    }
    // Spawn consumer threads
    for (int i = 1; i <= C; ++i)
    {
        int idx = i;
        pthread_create(&cons_ids[i], NULL, consume, (void *)&idx);
        // ! My computer is fast enough that it generates duplicate threads
        // ! This nanosleep ensures that each thread has separate id's
        nanosleep(&ts, NULL);
    }

    // Join all threads
    for (int i = 1; i <= P; ++i)
    {
        pthread_join(prod_ids[i], NULL);
        printf(TXTGREEN "Producer Thread joined: %d\n" TXTRESET, i);
    }
    for (int i = 1; i <= C; ++i)
    {
        pthread_join(cons_ids[i], NULL);
        printf(TXTCYAN "Consumer Thread joined: %d\n" TXTRESET, i);
    }

    // Destroy semaphores
    sem_destroy(&mutex);
    sem_destroy(&empty);
    sem_destroy(&full);

    // End time output and elapsed runtime
    time_t t2 = time(NULL);
    printTime(t2);

    int i;
    printf("\n___Testing Arrays___\n");
    printf("Produced || Consumed\n");
    // Test if P and C arrays match
    for (i = 0; i < P*X; ++i)
    {
        if (PRODUCED[i] != CONSUMED[i])
        {
            printf(TXTRED "Arrays don't match.\n" TXTRESET);
            break;
        }
        else
        {
            printf(" %5d   ||   %d\n", PRODUCED[i], CONSUMED[i]);
        }
    }

    if (i == P * X)
    {
        printf("Consume and Produce arrays match.\n");
    }
    printf("\nElapsed time: %ld secs\n", t2 - t1);

    return 0;
}

void* produce(void* threads)
{
    // Counter will be shared among all producer threads and incremented by each
    // Items produced will be unique this way
    static int counter = 0;
    int item;
    int idx = *((int*)threads);

    // Loop until X items produced
    for (int i = 0; i < X; ++i)
    {
        sem_wait(&full); // Wait if queue is full
        sem_wait(&mutex); // Wait
        item = enqueue_item(++counter);
        PRODUCED[counter - 1] = item; // Save item to the producer test array
        sem_post(&mutex); // Signal
        sem_post(&empty); // Signal that queue is not empty

        printf(TXTGREEN "\tItem [%d] was produced by: %d\n" TXTRESET, item, idx);
        sleep(Ptime);
    }
}

void* consume(void* threads)
{
    // Counter will be shared among all consumer threads and incremented by each
    // Counter will serve as index for destination of consumed items
    static int counter = 0;
    int item;
    int amount = XC; // Amount consumed by each consumer
    int idx = *((int*)threads);
    
    // The first consumer to reach this block will be the 'over-consumer'
    static int overconsumer = 1;
    if (overconsumer)
    {
        overconsumer = 0; // No other consumer can enter this block
        // This consumer will loop for the regular amount + the produced remainder
        amount = XC + OC;
    }
    
    // Loop until 'amount' items are consumed
    for (int i = 0; i < amount; ++i)
    {
        sem_wait(&empty); // Wait if queue is empty
        sem_wait(&mutex); // Wait
        item = dequeue_item();
        CONSUMED[counter++] = item; // Save item to the Consumer test array
        sem_post(&mutex); // Signal
        sem_post(&full); // Signal that queue is not full

        printf(TXTCYAN "\tItem [%d] was consumed by: %d\n" TXTRESET, item, idx);
        sleep(Ctime);
    }
    
}

/* 
 * Function to remove item.
 * Item removed is returned
 */
int dequeue_item()
{
    // First and top will be different values if there are more than one items in queue
    if (first != top)
    {
        int ret = first;
        first = (++first) % N;
        return QUEUE[ret];
    }
    // First and top will be the same nonnegative value if the queue has a single item
    else if (first == top && first >= 0)
    {
        int ret = first;
        first = -1;
        top = -1;
        return QUEUE[ret];
    }
    // First and top will both be negative if the queue is empty
    else
    {
        // Empty queue
        printf(TXTRED "Error: Empty queue.\n" TXTRESET);
        return -1;
    }
}

/* 
 * Function to add item.
 * Item added is returned.
 */
int enqueue_item(int item)
{
    // First and top will both be negative if the queue is empty
    if (first < 0 && first == top)
    {
        first = 0;
        top = 0;
        QUEUE[first] = item;
    }
    // Top's next value will not be First's value if queue is not full
    else if ( first <= top && ((top + 1) % N) != first )
    {
        top = (++top) % N;
        QUEUE[top] = item;
    }
    // Top's next value will be more than First if queue is not full
    else if ( (top + 1) < first )
    {
        top = (++top) % N;
        QUEUE[top] = item;
    }
    // Top's next value will equal First if the queue is full
    else
    {
        // Queue is full
        printf(TXTRED "Error: Full queue.\n" TXTRESET);
        return -1;
    }
    
    return item;
}

// Function prints current time as YEAR MONTH DAY TIME
void printTime(time_t t)
{
    struct tm ttime = *localtime(&t);
    printf("Time: %d-%d-%d", ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday);
    printf(" %d:%02d:%02d\n", ttime.tm_hour, ttime.tm_min, ttime.tm_sec);
}