/* _GNU_SOURCE must come first -- unlocks sched_setaffinity, CPU_SET on Linux */
#define _GNU_SOURCE

#include "attack_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * HOW THIS CONNECTS TO YOUR OS COURSE:
 * A preemptive scheduler gives every thread a time-slice. Normal threads
 * voluntarily block (I/O, sleep), freeing the CPU for others. Here every
 * thread runs a pure compute loop with NO blocking calls, so the scheduler
 * still preempts us (the OS must stay alive) but we immediately re-enter
 * the run-queue. Other processes find no free core -- scheduler starvation.
 *
 * On Linux, sched_setaffinity() pins each thread to one logical core,
 * preventing the scheduler from migrating threads and ensuring every
 * core is monopolised simultaneously.
 */

#ifdef CPUSTORM_WINDOWS
#define THREAD_RETURN_TYPE DWORD WINAPI
#define THREAD_RETURN_VAL 0
#else
#define THREAD_RETURN_TYPE void *
#define THREAD_RETURN_VAL NULL
#endif

/* engine_get_core_count()
 * Queries logical CPU core count cross-platform.
 * Logical = physical cores x hyperthreads per core.
 */
int engine_get_core_count(void)
{
#ifdef CPUSTORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
#else
    fprintf(stderr, "[CPUStorm] WARNING: Cannot detect core count. Assuming 1.\n");
    return 1;
#endif
}

/* busy_loop_worker()
 * Core thread function. Runs tight compute loop until running=0.
 *
 * WHY volatile on counter?
 *   Without it, the compiler optimises the increment away (dead write).
 *   volatile forces the memory write every iteration -- real CPU work.
 *
 * WHY no sleep/yield?
 *   Any blocking call returns control to the scheduler. We never yield.
 */
static THREAD_RETURN_TYPE busy_loop_worker(void *arg)
{
    CoreWorker *w = (CoreWorker *)arg;

#ifdef CPUSTORM_LINUX
    /* Pin this thread to its assigned logical core */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(w->core_id, &cpuset);
    /*
     * sched_setaffinity(0,...) applies to the calling thread.
     * After this the scheduler can ONLY place us on core w->core_id.
     */
    w->affinity_set = (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0) ? 1 : 0;

#elif defined(CPUSTORM_WINDOWS)
    DWORD_PTR mask = (DWORD_PTR)1 << w->core_id;
    w->affinity_set = (SetThreadAffinityMask(GetCurrentThread(), mask) != 0) ? 1 : 0;

#else
    w->affinity_set = 0; /* macOS restricts affinity -- best-effort */
#endif

    /* THE ATTACK LOOP
     * Spin until engine_stop() clears running. The counter
     * prevents the compiler from eliminating the loop body entirely.
     */
    volatile uint64_t *counter = &w->iterations;
    while (__atomic_load_n(&w->running, __ATOMIC_SEQ_CST))
    {
        (*counter)++;
    }

    return THREAD_RETURN_VAL;
}

/* engine_init()
 * Detects core count and zeroes worker structs.
 * Call before engine_launch().
 */
int engine_init(AttackEngine *eng)
{
    if (!eng)
        return -1;
    memset(eng, 0, sizeof(AttackEngine));

    eng->core_count = engine_get_core_count();
    if (eng->core_count < 1)
        return -1;
    if (eng->core_count > CPUSTORM_MAX_CORES)
        eng->core_count = CPUSTORM_MAX_CORES;

    for (int i = 0; i < eng->core_count; i++)
    {
        eng->workers[i].core_id = i;
        eng->workers[i].iterations = 0;
        __atomic_store_n(&eng->workers[i].running, 0, __ATOMIC_SEQ_CST);
        eng->workers[i].affinity_set = 0;
    }
    eng->active = 0;
    return 0;
}

/* engine_launch()
 * Spawns one thread per logical core.
 * Returns 0 on full success, -1 if any thread failed to start.
 */
int engine_launch(AttackEngine *eng)
{
    if (!eng || eng->active)
        return -1;
    eng->active = 1;
    int failures = 0;

    for (int i = 0; i < eng->core_count; i++)
    {
        __atomic_store_n(&eng->workers[i].running, 1, __ATOMIC_SEQ_CST);
#ifdef CPUSTORM_WINDOWS
        eng->threads[i] = CreateThread(NULL, 0, busy_loop_worker,
                                       &eng->workers[i], 0, NULL);
        if (!eng->threads[i])
        {
            eng->workers[i].running = 0;
            failures++;
        }
#else
        if (pthread_create(&eng->threads[i], NULL, busy_loop_worker,
                           &eng->workers[i]) != 0)
        {
            eng->workers[i].running = 0;
            failures++;
        }
#endif
    }
    return (failures == 0) ? 0 : -1;
}

/* engine_stop()
 * Signals all workers to exit, then joins every thread.
 * Guarantees no threads remain after this returns.
 */
void engine_stop(AttackEngine *eng)
{
    if (!eng || !eng->active)
        return;

    for (int i = 0; i < eng->core_count; i++)
        __atomic_store_n(&eng->workers[i].running, 0, __ATOMIC_SEQ_CST);

    for (int i = 0; i < eng->core_count; i++)
    {
#ifdef CPUSTORM_WINDOWS
        if (eng->threads[i])
        {
            WaitForSingleObject(eng->threads[i], INFINITE);
            CloseHandle(eng->threads[i]);
            eng->threads[i] = NULL;
        }
#else
        pthread_join(eng->threads[i], NULL);
#endif
    }
    eng->active = 0;
}