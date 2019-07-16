#include <picoro/picoro.h>

#include "kmalloc.h"
#include "jobq.h"
#include "../process.h"

#define MAX_JOBS 255

struct queue
{
    int size;
    int capacity;
    job_t *jobs[MAX_JOBS];
};

static struct queue *jobq;

void jobq_init()
{
    jobq = (struct queue *) kmalloc(sizeof(struct queue));
    jobq->size = 0;
    jobq->capacity = MAX_JOBS;
}

int jobq_push(job_t *job)
{
    if (jobq->size == MAX_JOBS) { return 0; }
    jobq->jobs[jobq->size++] = job;
    return 1;
}

int jobq_empty()
{
    return (jobq->size == 0);
}

job_t *jobq_front()
{
    if (jobq->size > 0) {
        return jobq->jobs[0];
    }
    return NULL;
}

void jobq_pop()
{
    if (jobq->size == 0) return;
    jobq->size--;
    for (int i = 0; i < jobq->size; i++) {
        jobq->jobs[i] = jobq->jobs[i+1];
    }
}
