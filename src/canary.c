#include "canary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* ── Canary thread function ────────────────────────────────────── */
static void *canary_thread(void *arg)
{
    Canary *canary = (Canary *)arg;
    FILE *fp = fopen(canary->logfile, "w"); // "w" to create/truncate, or "a" to append
    if (!fp)
    {
        fprintf(stderr, "[Canary] ERROR: Failed to open logfile %s\n", canary->logfile);
        return NULL;
    }

    /* Write CSV header */
    fprintf(fp, "timestamp_sec,response_time_ms,is_attack_active\n");

    while (canary->active)
    {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        /* Perform light work: simple loop to simulate computation */
        volatile uint64_t sum = 0;
        for (int i = 0; i < 1000000; i++) // Increased from 100000 to 1000000 for more impact
        {
            sum += i * i; // make it a bit more work
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        /* Calculate response time in milliseconds */
        double delta_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        double delta_ms = delta_sec * 1000.0;

        /* Log to CSV: timestamp, response_time, attack_flag */
        double timestamp = start.tv_sec + start.tv_nsec / 1e9;
        fprintf(fp, "%.6f,%.3f,%d\n", timestamp, delta_ms, *canary->attack_flag);
        fflush(fp); // Flush to ensure immediate write

        /* Sleep for 100ms before next measurement */
        usleep(100000);
    }

    fclose(fp);
    return NULL;
}

/* ── Public API ────────────────────────────────────────────────── */
void canary_init(Canary *canary, const char *logfile, int *attack_flag)
{
    memset(canary, 0, sizeof(Canary));
    strncpy(canary->logfile, logfile, sizeof(canary->logfile) - 1);
    canary->attack_flag = attack_flag;
}

void canary_start(Canary *canary)
{
    canary->active = 1;
    if (pthread_create(&canary->thread, NULL, canary_thread, canary) != 0)
    {
        fprintf(stderr, "[Canary] ERROR: Failed to create canary thread\n");
    }
}

void canary_stop(Canary *canary)
{
    canary->active = 0;
    pthread_join(canary->thread, NULL);
}

void canary_analyze_log(const char *logfile, double *baseline_avg, double *attack_avg, int *baseline_count, int *attack_count)
{
    FILE *fp = fopen(logfile, "r");
    if (!fp)
        return;

    char line[256];
    double baseline_sum = 0.0, attack_sum = 0.0;
    int b_count = 0, a_count = 0;

    // Skip header
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp))
    {
        double timestamp, response_time;
        int is_attack;
        if (sscanf(line, "%lf,%lf,%d", &timestamp, &response_time, &is_attack) == 3)
        {
            if (is_attack == 0)
            {
                baseline_sum += response_time;
                b_count++;
            }
            else
            {
                attack_sum += response_time;
                a_count++;
            }
        }
    }

    fclose(fp);

    *baseline_avg = (b_count > 0) ? baseline_sum / b_count : 0.0;
    *attack_avg = (a_count > 0) ? attack_sum / a_count : 0.0;
    *baseline_count = b_count;
    *attack_count = a_count;
}