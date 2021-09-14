/* common/threads.c */

#include <cstdint>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <vector>

#include <common/log.hh>
#include <common/threads.hh>

/* Make the locks no-ops if we aren't running threads */
static bool threads_active = false;

static int dispatch;
static int workcount;
static int oldpercent = -1;

/*
 * =============
 * GetThreadWork
 * =============
 */
int GetThreadWork_Locked__(void)
{
    int ret;
    int percent;

    if (dispatch == workcount)
        return -1;

    percent = 50 * dispatch / workcount;
    while (oldpercent < percent) {
        oldpercent++;
        logprint_locked__("%c", (oldpercent % 5) ? '.' : '0' + (oldpercent / 5));
    }

    ret = dispatch;
    dispatch++;

    return ret;
}

int GetThreadWork(void)
{
    int ret;

    ThreadLock();
    ret = GetThreadWork_Locked__();
    ThreadUnlock();

    return ret;
}

void InterruptThreadProgress__(void)
{
    if (oldpercent != -1) {
        logprint_locked__("\\\n");
        oldpercent = -1;
    }
}

/*
 * ===================================================================
 *                              WIN32
 * ===================================================================
 */

#ifdef USE_WIN32THREADS

#include <windows.h>
#define HAVE_THREADS

void LowerProcessPriority()
{
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
}

#elif USE_PTHREADS

#define HAVE_THREADS

/* not implemented for now */
void LowerProcessPriority()
{
}

#else

void LowerProcessPriority(void)
{
}

#endif

#ifdef HAVE_THREADS

int numthreads = 1;
std::mutex crit;

int GetDefaultThreads()
{
    return std::thread::hardware_concurrency();
}

void ThreadLock()
{
    if (threads_active)
        crit.lock();
}

void ThreadUnlock()
{
    if (threads_active)
        crit.unlock();
}

/*
 * =============
 * RunThreadsOn
 * =============
 */
void RunThreadsOn(int start, int workcnt, void *(func)(void *), void *arg)
{
    std::vector<std::thread> threadhandle;

    dispatch = start;
    workcount = workcnt;
    oldpercent = -1;

    /* run threads in parallel */
    threads_active = true;

    for (int32_t i = 0; i < numthreads; i++)
        threadhandle.push_back(std::thread(func, arg));

    for (int32_t i = 0; i < numthreads; i++)
        threadhandle[i].join();

    threads_active = false;
    oldpercent = -1;

    logprint("\n");
}

/*
 * =======================================================================
 *                                SINGLE THREAD
 * =======================================================================
 */

#else

int numthreads = 1;

void ThreadLock(void) { }
void ThreadUnlock(void) { }

/*
 * =============
 * RunThreadsOn
 * =============
 */
void RunThreadsOn(int start, int workcnt, void *(func)(void *), void *arg)
{
    dispatch = start;
    workcount = workcnt;
    oldpercent = -1;

    func(arg);

    logprint("\n");
}

#endif /* HAVE_THREADS */
