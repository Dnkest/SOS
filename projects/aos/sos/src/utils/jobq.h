#pragma once

typedef struct job {
    coro c;
    void *proc;
} job_t;

void jobq_init();
int jobq_empty();
int jobq_push(job_t *job);
job_t *jobq_front();
void jobq_pop();
