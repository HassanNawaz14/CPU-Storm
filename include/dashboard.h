#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "attack_engine.h"
#include <time.h>

#define ANSI_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_WHITE "\x1b[37m"
#define ANSI_GREY "\x1b[90m"
#define ANSI_CLEAR "\x1b[2J"
#define ANSI_HOME "\x1b[H"
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_SHOW_CURSOR "\x1b[?25h"

#define DASH_BAR_WIDTH 30
#define DASH_REFRESH_MS 200

typedef struct
{
    struct timespec start;
    int duration_sec;
    uint64_t prev_iters[CPUSTORM_MAX_CORES];
    double iter_rate[CPUSTORM_MAX_CORES];
    AttackEngine *eng;
    int first_draw;
    int lines_drawn;
} Dashboard;

void dashboard_enable_ansi_windows(void);
void dashboard_init(Dashboard *dash, AttackEngine *eng, int duration_sec);
void dashboard_render(Dashboard *dash);
void dashboard_final(Dashboard *dash);
double dashboard_get_elapsed(Dashboard *dash);
void dashboard_cleanup(void);
void dashboard_sleep_ms(int ms);

#endif