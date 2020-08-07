
/**
 ** Written by Amit Sides
 **/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

int random_range(int min, int max)
{
    return (rand() % (max - min + 1)) + min;
}

void * printer(void * n_pointer)
{
    int n;
    pthread_t tid;
    int count = random_range(2, 10);

    // Getting argument given to pthread_create
    n = *((int *)n_pointer);

    // Getting the thread's id
    tid = pthread_self();

    // Sleeping so main can finish running...
    usleep(100);

    // Printing information
    for (int i = 0; i < count; i++)
    {
        printf("Thread 0x%lx got argument %d\n", tid, n);
        usleep(3);
    }

    // Exiting thread...
    pthread_exit(NULL);
}

void main()
{
    int arg1=1, arg2=2;
    pthread_t t1, t2;

    // Setting seed for RNG
    srand(time(NULL));

    // Creating first thread
    errno = pthread_create(&t1, NULL, printer, (void *)&arg1);
    if (0 != errno)
    {
        perror("Error while creating thread 1");
        exit(errno);
    }
    printf("Created first thread:\t0x%lx\n", t1);

    // Creating second thread
    errno = pthread_create(&t2, NULL, printer, (void *)&arg2);
    if (0 != errno)
    {
        perror("Error while creating thread 2");
        exit(errno);
    }
    printf("Created second thread:\t0x%lx\n", t2);

    // Waiting for first thread to finish
    errno = pthread_join(t1, NULL);
    if (0 != errno)
    {
        perror("Error while waiting thread 1");
        exit(errno);
    }

    // Waiting for second thread to finish
    errno = pthread_join(t2, NULL);
    if (0 != errno)
    {
        perror("Error while waiting thread 2");
        exit(errno);
    }
}
