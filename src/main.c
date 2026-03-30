/*
 * CPUStorm - CPU Exhaustion Attack Simulator
 * main.c — Entry point and argument parsing
 *
 * Usage:
 *   ./cpustorm              → run for 10 seconds (default)
 *   ./cpustorm -t 30        → run for 30 seconds
 *   ./cpustorm -t 0         → run until Ctrl+C
 *   ./cpustorm --info       → print system info and exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "attack_engine.h"
#include "dashboard.h"
#include "canary.h"

/* ── Platform-specific sleep & time ────────────────────────────── */
#ifdef CPUSTORM_WINDOWS
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* ── Global engine pointer for signal handler ───────────────────── */
static AttackEngine *g_engine = NULL;

/* ── Global attack active flag for canary logging ──────────────── */
static int g_attack_active = 0;

/* ── SIGINT handler (Ctrl+C) ────────────────────────────────────── */
static void handle_signal(int sig)
{
    (void)sig;
    printf("\n\n[CPUStorm] Signal received — initiating shutdown...\n");
    if (g_engine)
    {
        engine_stop(g_engine);
    }
    printf("[CPUStorm] All threads stopped. System restored.\n");
}

/* ── Print system info ──────────────────────────────────────────── */
static void print_info(void)
{
    int cores = engine_get_core_count();
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║        CPUStorm  v%-17s ║\n", CPUSTORM_VERSION);
    printf("╠══════════════════════════════════════╣\n");
    printf("║  Logical CPU Cores  : %-14d ║\n", cores);

#if defined(CPUSTORM_LINUX)
    printf("║  Platform           : %-14s ║\n", "Linux");
    printf("║  Affinity Support   : %-14s ║\n", "Yes (sched_setaffinity)");
#elif defined(CPUSTORM_MACOS)
    printf("║  Platform           : %-14s ║\n", "macOS");
    printf("║  Affinity Support   : %-14s ║\n", "No (Apple restriction)");
#elif defined(CPUSTORM_WINDOWS)
    printf("║  Platform           : %-14s ║\n", "Windows");
    printf("║  Affinity Support   : %-14s ║\n", "Yes (SetThreadAffinityMask)");
#else
    printf("║  Platform           : %-14s ║\n", "Unknown POSIX");
    printf("║  Affinity Support   : %-14s ║\n", "Best-effort");
#endif

    printf("╚══════════════════════════════════════╝\n\n");
}

/* ── Print usage ────────────────────────────────────────────────── */
static void print_usage(const char *prog)
{
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("  -t <seconds>   Duration of the attack simulation (default: 10)\n");
    printf("                 Use 0 to run until Ctrl+C\n");
    printf("  --info         Display system info and exit\n");
    printf("  --help         Show this message\n\n");
    printf("Example: %s -t 15\n\n", prog);
    printf("WARNING: This tool is for EDUCATIONAL use only.\n");
    printf("         Run only on systems you own or have permission to test.\n\n");
}

/* ── main ───────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    int duration_sec = 10; /* default: 10-second simulation        */

    /* ── Argument parsing ──────────────────────────────────────── */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--info") == 0)
        {
            print_info();
            return 0;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: -t requires a value.\n");
                return 1;
            }
            duration_sec = atoi(argv[++i]);
            if (duration_sec < 0)
            {
                fprintf(stderr, "Error: duration must be >= 0.\n");
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* ── Banner ─────────────────────────────────────────────────── */
    printf("\n");
    printf("  ██████╗██████╗ ██╗   ██╗███████╗████████╗ ██████╗ ██████╗ ███╗   ███╗\n");
    printf(" ██╔════╝██╔══██╗██║   ██║██╔════╝╚══██╔══╝██╔═══██╗██╔══██╗████╗ ████║\n");
    printf(" ██║     ██████╔╝██║   ██║███████╗   ██║   ██║   ██║██████╔╝██╔████╔██║\n");
    printf(" ██║     ██╔═══╝ ██║   ██║╚════██║   ██║   ██║   ██║██╔══██╗██║╚██╔╝██║\n");
    printf(" ╚██████╗██║     ╚██████╔╝███████║   ██║   ╚██████╔╝██║  ██║██║ ╚═╝ ██║\n");
    printf("  ╚═════╝╚═╝      ╚═════╝ ╚══════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝\n");
    printf("\n  CPU Exhaustion Attack Simulator  |  v%s  |  Educational Use Only\n\n",
           CPUSTORM_VERSION);

    dashboard_enable_ansi_windows();

    /* Init and start canary process for baseline measurement */
    Canary canary;
    canary_init(&canary, "canary_log.csv", &g_attack_active);
    canary_start(&canary);
    usleep(500000); // Allow baseline logging before attack
    printf("[CPUStorm] Canary process started for performance monitoring.\n");

    /* ── Init engine ─────────────────────────────────────────────── */
    AttackEngine engine;
    g_engine = &engine;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (engine_init(&engine) != 0)
    {
        fprintf(stderr, "[CPUStorm] ERROR: Failed to initialise engine.\n");
        return 1;
    }

    printf("[CPUStorm] Detected %d logical CPU core(s).\n", engine.core_count);

    if (duration_sec > 0)
    {
        printf("[CPUStorm] Attack duration : %d second(s)\n", duration_sec);
    }
    else
    {
        printf("[CPUStorm] Attack duration : unlimited (Ctrl+C to stop)\n");
    }

    printf("[CPUStorm] Launching %d worker thread(s)...\n\n", engine.core_count);

    /* ── Launch ──────────────────────────────────────────────────── */
    if (engine_launch(&engine) != 0)
    {
        fprintf(stderr, "[CPUStorm] WARNING: Some threads failed to launch.\n");
        /* Non-fatal: continue with whatever threads did start       */
    }

    /* Print per-core status */
    for (int i = 0; i < engine.core_count; i++)
    {
        printf("  [Core %3d]  Thread launched  |  Affinity pinned: %s\n",
               i,
               engine.workers[i].affinity_set ? "YES" : "NO (best-effort)");
    }

    printf("\n[CPUStorm] *** ATTACK ACTIVE ***  All cores under load.\n");
    if (duration_sec > 0)
    {
        printf("[CPUStorm] Running for %d second(s). Press Ctrl+C to abort early.\n\n",
               duration_sec);
    }

    /* Init dashboard */
    Dashboard dash;
    dashboard_init(&dash, &engine, duration_sec);

    /* Set attack active flag */
    g_attack_active = 1;

    /* ── Dashboard loop ─────────────────────────────────────────── */
    if (duration_sec == 0)
    {
        while (engine.active)
        {
            dashboard_render(&dash);
            dashboard_sleep_ms(DASH_REFRESH_MS);
        }
    }
    else
    {
        int ticks = (duration_sec * 1000) / DASH_REFRESH_MS;
        for (int i = 0; i < ticks && engine.active; i++)
        {
            dashboard_render(&dash);
            dashboard_sleep_ms(DASH_REFRESH_MS);
        }
    }

    /* Set attack inactive */
    g_attack_active = 0;

    /* ── Stop ────────────────────────────────────────────────────── */
    printf("\n\n[CPUStorm] Duration complete. Stopping all worker threads...\n");
    engine_stop(&engine);

    /* Stop canary */
    canary_stop(&canary);
    printf("[CPUStorm] Canary process stopped. Log saved to canary_log.csv\n");

    /* Analyze canary data */
    double baseline_avg, attack_avg;
    int baseline_count, attack_count;
    canary_analyze_log("canary_log.csv", &baseline_avg, &attack_avg, &baseline_count, &attack_count);
    double starvation_ratio = (baseline_avg > 0) ? attack_avg / baseline_avg : 1.0;

    /* Calculate peak CPU% */
    int total_cores = engine_get_core_count();
    double peak_cpu_percent = 100.0 * engine.core_count / total_cores;

    /* Enhanced summary */
    printf("\n[CPUStorm] ── Attack Impact Summary ──\n");
    printf("  Peak CPU Usage     : %.1f%%\n", peak_cpu_percent);
    printf("  Canary Starvation  : %.2fx (attack vs baseline response time)\n", starvation_ratio);
    printf("  Baseline samples   : %d (avg: %.3f ms)\n", baseline_count, baseline_avg);
    printf("  Attack samples     : %d (avg: %.3f ms)\n", attack_count, attack_avg);

    dashboard_final(&dash);

    /* Generate report */
    FILE *report = fopen("attack_report.txt", "w");
    if (report)
    {
        fprintf(report, "CPUStorm Attack Report\n");
        fprintf(report, "======================\n\n");
        fprintf(report, "System Information:\n");
        fprintf(report, "  Total CPU Cores: %d\n", total_cores);
        fprintf(report, "  Targeted Cores: %d\n\n", engine.core_count);

        fprintf(report, "Attack Configuration:\n");
        fprintf(report, "  Duration: %.1f seconds\n", dashboard_get_elapsed(&dash));
        fprintf(report, "  Peak CPU Usage: %.1f%%\n\n", peak_cpu_percent);

        fprintf(report, "Performance Impact:\n");
        fprintf(report, "  Canary Baseline: %d samples, avg %.3f ms\n", baseline_count, baseline_avg);
        fprintf(report, "  Canary During Attack: %d samples, avg %.3f ms\n", attack_count, attack_avg);
        fprintf(report, "  Starvation Ratio: %.2fx\n\n", starvation_ratio);

        fprintf(report, "Work Summary:\n");
        uint64_t total_iters = 0;
        for (int i = 0; i < engine.core_count; i++)
        {
            fprintf(report, "  Core %d: %llu iterations\n", i, (unsigned long long)engine.workers[i].iterations);
            total_iters += engine.workers[i].iterations;
        }
        fprintf(report, "  Total: %llu iterations\n", (unsigned long long)total_iters);
        double avg_rate = (dashboard_get_elapsed(&dash) > 0) ? (double)total_iters / dashboard_get_elapsed(&dash) / 1e6 : 0.0;
        fprintf(report, "  Average Rate: %.1f M/s\n\n", avg_rate);

        fprintf(report, "Ethical Considerations:\n");
        fprintf(report, "  This simulation demonstrates CPU exhaustion attacks (CWE-400).\n");
        fprintf(report, "  Mitigation: Monitor system resources, implement rate limiting,\n");
        fprintf(report, "  use CPU affinity controls, and deploy intrusion detection.\n");
        fprintf(report, "  Always test on authorized systems only.\n");

        fclose(report);
        printf("[CPUStorm] Attack report generated: attack_report.txt\n");
    }

    return 0;
}

/*make -f build/Makefle*/