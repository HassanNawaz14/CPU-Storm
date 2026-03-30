#ifndef CANARY_H
#define CANARY_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>

typedef struct
{
    pthread_t thread;
    char logfile[256];
    int active;
    int *attack_flag; // pointer to global attack_active flag
} Canary;

void canary_init(Canary *canary, const char *logfile, int *attack_flag);
void canary_start(Canary *canary);
void canary_stop(Canary *canary);
void canary_analyze_log(const char *logfile, double *baseline_avg, double *attack_avg, int *baseline_count, int *attack_count);

#endif /* CANARY_H */