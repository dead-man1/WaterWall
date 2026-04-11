#ifndef WW_PROC_H_
#define WW_PROC_H_

/**
 * @file wproc.h
 * @brief Process/thread spawning helpers and command execution utilities.
 *
 * This module provides a small cross-platform abstraction for launching work
 * in a child process (Unix) or worker thread (Windows), plus utility helpers
 * for command execution and privilege checks.
 */

#include "wlibc.h"

/**
 * @brief Runtime context used by procSpawn/procRun callbacks.
 */
typedef struct proc_ctx_s
{
    pid_t       pid; // tid in Windows
    time_t      start_time;
    int         spawn_cnt;
    procedure_t init;
    void       *init_userdata;
    procedure_t proc;
    void       *proc_userdata;
    procedure_t exit;
    void       *exit_userdata;
} proc_ctx_t;

/**
 * @brief Execute callbacks from a process context in init/proc/exit order.
 *
 * @param ctx Process context containing callback pointers and user data.
 */
static inline void procRun(proc_ctx_t *ctx)
{
    if (ctx->init)
    {
        ctx->init(ctx->init_userdata);
    }
    if (ctx->proc)
    {
        ctx->proc(ctx->proc_userdata);
    }
    if (ctx->exit)
    {
        ctx->exit(ctx->exit_userdata);
    }
}

#ifdef OS_UNIX
// unix use multi-processes
/**
 * @brief Spawn execution for a process context.
 *
 * On Unix this uses `fork`; in the child it runs `procRun` and exits.
 *
 * @param ctx Process context to execute.
 * @return Child pid in the parent process, `0` in the child path before exit,
 *         or `-1` on failure.
 */
static inline int procSpawn(proc_ctx_t *ctx)
{
    ++ctx->spawn_cnt;
    ctx->start_time = time(NULL);
    pid_t pid       = fork();
    if (pid < 0)
    {
        printError("fork");
        return -1;
    }
    else if (pid == 0)
    {
        // child process
        ctx->pid = getpid();
        procRun(ctx);
        exit(0);
    }
    else if (pid > 0)
    {
        // parent process
        ctx->pid = pid;
    }
    return pid;
}
#elif defined(OS_WIN)
// win32 use multi-threads
static void win_thread(void *userdata)
{
    proc_ctx_t *ctx = (proc_ctx_t *) userdata;
    ctx->pid        = GetCurrentThreadId(); // tid in Windows
    procRun(ctx);
}
/**
 * @brief Spawn execution for a process context.
 *
 * On Windows this starts a worker thread that runs `procRun`.
 *
 * @param ctx Process context to execute.
 * @return Thread id on success, or `-1` on failure.
 */
static inline int procSpawn(proc_ctx_t *ctx)
{
    ++ctx->spawn_cnt;
    ctx->start_time = time(NULL);
    HANDLE h        = (HANDLE) _beginthread(win_thread, 0, ctx);
    if (h == NULL)
    {
        return -1;
    }
    ctx->pid = (pid_t) GetThreadId(h); // tid in Windows
    return (int) ctx->pid;
}
#endif

typedef struct
{
    char output[2048]; // if you modify this, update the coed below (fscanf)
    int  exit_code;
} cmd_result_t;

// blocking io
/**
 * @brief Execute a shell command and capture a short output token.
 *
 * @param str Command string passed to `popen`/`_popen`.
 * @return Command output buffer and exit code.
 */
static cmd_result_t execCmd(const char *str)
{
    FILE        *fp;
    cmd_result_t result = (cmd_result_t){{0}, -1};
    char        *buf    = &(result.output[0]);
    /* Open the command for reading. */
#if defined(OS_UNIX)
    fp = popen(str, "r");
#else
    fp               = _popen(str, "r");
#endif

    if (fp == NULL)
    {
        printf("Failed to run command \"%s\"\n", str);
        return (cmd_result_t){{0}, -1};
    }

    int read = fscanf(fp, "%2047s", buf);
    discard read;
#if defined(OS_UNIX)
    result.exit_code = pclose(fp);
#else
    result.exit_code = _pclose(fp);
#endif

    return result;
    /* close */
    // return 0 == pclose(fp);
}

/**
 * @brief Check if a command is available in the current shell environment.
 *
 * @param app Executable name to search for.
 * @return `true` if command lookup succeeds, otherwise `false`.
 */
static bool checkCommandAvailable(const char *app)
{
    char b[300];
    sprintf(b, "command -v %288s", app);
    cmd_result_t result = execCmd(b);
    return (result.exit_code == 0 && stringLength(result.output) > 0);
}

/**
* @brief Attempts to elevate the privileges of the current process.
*
* @param app_name The executable name of the application.
* @param fail_msg The error message to display if elevation fails.
* @return bool true on success, false otherwise.
*/
bool elevatePrivileges(const char *app_name, char *fail_msg);

/**
 * @brief Check whether the current process has administrator/root privileges.
 *
 * @return `true` when running with elevated privileges.
 */
bool isAdmin(void);

#endif // WW_PROC_H_
