#ifndef ATTACK_ENGINE_H
#define ATTACK_ENGINE_H

/*
 * CPUStorm - CPU Exhaustion Attack Simulator
 * attack_engine.h — Thread engine interface
 *
 * Simulates CWE-400: Uncontrolled Resource Consumption
 * by spawning one worker thread per logical CPU core,
 * each pinned (where supported) to saturate the scheduler.
 */

#include <stdint.h>

/* ── Platform detection ─────────────────────────────────────────── */
#if defined(_WIN32) || defined(_WIN64)
#define CPUSTORM_WINDOWS
#include <windows.h>
#else
#define CPUSTORM_POSIX
#include <pthread.h>
#include <unistd.h>
#if defined(__linux__)
#define CPUSTORM_LINUX
#include <sched.h>
#elif defined(__APPLE__)
#define CPUSTORM_MACOS
#endif
#endif

/* ── Constants ──────────────────────────────────────────────────── */
#define CPUSTORM_MAX_CORES 256 /* hard ceiling for thread array  */
#define CPUSTORM_VERSION "1.0.0"

/* ── Per-thread state ────────────────────────────────────────────── */
typedef struct
{
    int core_id;         /* logical core this thread targets   */
    uint64_t iterations; /* busy-loop counter (proof of work)  */
    int running;         /* 1 = active, 0 = should stop        */
    int affinity_set;    /* 1 = successfully pinned to core    */
} CoreWorker;

/* ── Engine state ────────────────────────────────────────────────── */
typedef struct
{
    int core_count;                         /* detected logical cores */
    CoreWorker workers[CPUSTORM_MAX_CORES]; /* per-core state         */
    int active;                             /* engine running flag    */

#ifdef CPUSTORM_WINDOWS
    HANDLE threads[CPUSTORM_MAX_CORES];
#else
    pthread_t threads[CPUSTORM_MAX_CORES];
#endif
} AttackEngine;

/* ── Public API ─────────────────────────────────────────────────── */

/**
 * engine_init()
 * Detects logical CPU count and zeroes out the engine struct.
 * Returns 0 on success, -1 if core detection fails.
 */
int engine_init(AttackEngine *eng);

/**
 * engine_launch()
 * Spawns one thread per core. Each thread runs a tight busy-loop
 * and attempts to pin itself to its assigned core where the OS
 * allows it (Linux: sched_setaffinity, others: best-effort).
 * Returns 0 on success, -1 on thread creation failure.
 */
int engine_launch(AttackEngine *eng);

/**
 * engine_stop()
 * Signals all threads to exit and joins them.
 * Safe to call multiple times.
 */
void engine_stop(AttackEngine *eng);

/**
 * engine_get_core_count()
 * Cross-platform logical CPU count query.
 * Falls back to 1 if detection is unavailable.
 */
int engine_get_core_count(void);

#endif /* ATTACK_ENGINE_H */