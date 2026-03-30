#define _GNU_SOURCE

#include "dashboard.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Move cursor up N lines, erasing each */
static void cursor_up(int n)
{
    for (int i = 0; i < n; i++)
        printf("\033[A\033[2K");
}

/* Seconds elapsed since dash->start */
static double elapsed_sec(Dashboard *dash)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - dash->start.tv_sec) + (double)(now.tv_nsec - dash->start.tv_nsec) / 1e9;
}

/* ASCII load bar: [####          ] */
static void print_bar(int fill, int width, const char *colour)
{
    printf("[%s", colour);
    for (int i = 0; i < width; i++)
        printf("%c", (i < fill) ? '#' : ' ');
    printf(ANSI_RESET "]");
}

/* ---------------------------------------------------------------- */
void dashboard_enable_ansi_windows(void)
{
#ifdef CPUSTORM_WINDOWS
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

/* ---------------------------------------------------------------- */
void dashboard_init(Dashboard *dash, AttackEngine *eng, int duration_sec)
{
    memset(dash, 0, sizeof(Dashboard));
    dash->eng = eng;
    dash->duration_sec = duration_sec;
    dash->first_draw = 1;
    clock_gettime(CLOCK_MONOTONIC, &dash->start);
}

/* ---------------------------------------------------------------- */
void dashboard_render(Dashboard *dash)
{
    AttackEngine *eng = dash->eng;
    double elapsed = elapsed_sec(dash);
    int lines = 0;

    if (!dash->first_draw)
        cursor_up(dash->lines_drawn);
    dash->first_draw = 0;

    /* Header */
    printf(ANSI_BOLD ANSI_CYAN
           "  CPUStorm  |  ATTACK ACTIVE  |  Cores: %d" ANSI_RESET "\n",
           eng->core_count);
    lines++;

    printf(ANSI_GREY
           "  --------------------------------------------------------\n" ANSI_RESET);
    lines++;

    /* Elapsed / remaining */
    int ei = (int)elapsed;
    if (dash->duration_sec > 0)
    {
        int rem = dash->duration_sec - ei;
        if (rem < 0)
            rem = 0;
        printf("  Elapsed: " ANSI_WHITE "%3ds" ANSI_RESET
               "   Remaining: " ANSI_WHITE "%3ds\n" ANSI_RESET,
               ei, rem);
    }
    else
    {
        printf("  Elapsed: " ANSI_WHITE "%3ds" ANSI_RESET
               "   Unlimited (Ctrl+C to stop)\n",
               ei);
    }
    lines++;

    /* Threat level */
    const char *tc, *tl;
    double ratio = (dash->duration_sec > 0)
                       ? elapsed / dash->duration_sec
                       : 0.0;
    if (dash->duration_sec == 0 || ratio < 0.33)
    {
        tc = ANSI_YELLOW;
        tl = "ELEVATED";
    }
    else if (ratio < 0.66)
    {
        tc = ANSI_RED;
        tl = "HIGH    ";
    }
    else
    {
        tc = ANSI_RED ANSI_BOLD;
        tl = "CRITICAL";
    }
    printf("  Threat Level: %s%s" ANSI_RESET "\n", tc, tl);
    lines++;

    printf(ANSI_GREY
           "  --------------------------------------------------------\n" ANSI_RESET);
    lines++;

    /* Column header */
    printf("  %-8s  %-5s  %-32s  %s\n",
           "CORE", "AFF", "LOAD", "RATE (M/s)");
    lines++;

    /* Per-core rows */
    for (int i = 0; i < eng->core_count; i++)
    {
        uint64_t cur = eng->workers[i].iterations;
        uint64_t prev = dash->prev_iters[i];
        uint64_t diff = (cur >= prev) ? (cur - prev) : 0;

        /* Scale per-frame diff to per-second rate */
        double rate = (double)diff * (1000.0 / DASH_REFRESH_MS);
        double rate_M = rate / 1e6;

        /* Normalise against 3 billion/sec cap for bar fill */
        int fill = (int)((rate / 3.0e9) * DASH_BAR_WIDTH);
        if (fill > DASH_BAR_WIDTH)
            fill = DASH_BAR_WIDTH;
        if (fill < 0)
            fill = 0;

        const char *bar_col =
            (fill < DASH_BAR_WIDTH / 3) ? ANSI_GREEN : (fill < (DASH_BAR_WIDTH * 2 / 3)) ? ANSI_YELLOW
                                                                                         : ANSI_RED;

        const char *aff = eng->workers[i].affinity_set
                              ? ANSI_GREEN "PIN" ANSI_RESET
                              : ANSI_GREY "---" ANSI_RESET;

        printf("  Core %3d  %s    ", i, aff);
        print_bar(fill, DASH_BAR_WIDTH, bar_col);
        printf("  %8.1f\n", rate_M);
        lines++;

        dash->prev_iters[i] = cur;
    }

    printf(ANSI_GREY
           "  --------------------------------------------------------\n" ANSI_RESET);
    lines++;

    uint64_t total = 0;
    for (int i = 0; i < eng->core_count; i++)
        total += eng->workers[i].iterations;

    printf("  Total iters : " ANSI_WHITE "%llu\n" ANSI_RESET,
           (unsigned long long)total);
    lines++;

    printf(ANSI_GREY "  Press Ctrl+C to abort early.\n" ANSI_RESET);
    lines++;

    fflush(stdout);
    dash->lines_drawn = lines;
}

/* ---------------------------------------------------------------- */
void dashboard_final(Dashboard *dash)
{
    AttackEngine *eng = dash->eng;
    double total_time = elapsed_sec(dash);
    uint64_t grand = 0;

    cursor_up(dash->lines_drawn);

    printf(ANSI_BOLD ANSI_CYAN
           "\n  CPUStorm  |  ATTACK COMPLETE\n" ANSI_RESET);
    printf(ANSI_GREY
           "  ========================================================\n" ANSI_RESET);
    printf("  Duration       : %.1f seconds\n", total_time);
    printf("  Cores targeted : %d\n\n", eng->core_count);

    printf("  %-8s  %-13s  %-22s  %s\n",
           "CORE", "AFF PINNED", "TOTAL ITERS", "AVG RATE (M/s)");
    printf(ANSI_GREY
           "  --------------------------------------------------------\n" ANSI_RESET);

    for (int i = 0; i < eng->core_count; i++)
    {
        uint64_t iters = eng->workers[i].iterations;
        double avg = (total_time > 0)
                         ? (double)iters / total_time / 1e6
                         : 0.0;
        printf("  Core %3d  %-13s  %-22llu  %.1f\n",
               i,
               eng->workers[i].affinity_set ? "YES (pinned)" : "NO  (float)",
               (unsigned long long)iters, avg);
        grand += iters;
    }

    printf(ANSI_GREY
           "  --------------------------------------------------------\n" ANSI_RESET);
    printf("  Grand total : " ANSI_WHITE ANSI_BOLD "%llu iters\n" ANSI_RESET,
           (unsigned long long)grand);
    printf("  Avg rate    : " ANSI_WHITE ANSI_BOLD "%.1f M/s\n" ANSI_RESET,
           (total_time > 0) ? (double)grand / total_time / 1e6 : 0.0);
    printf(ANSI_GREY
           "  ========================================================\n" ANSI_RESET);
    printf(ANSI_GREEN
           "  All threads stopped. System fully restored.\n\n" ANSI_RESET);
    fflush(stdout);
}

/* ---------------------------------------------------------------- */
void dashboard_cleanup(void)
{
}

/* ---------------------------------------------------------------- */
void dashboard_sleep_ms(int ms)
{
#ifdef CPUSTORM_WINDOWS
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* ---------------------------------------------------------------- */
double dashboard_get_elapsed(Dashboard *dash)
{
    return elapsed_sec(dash);
}