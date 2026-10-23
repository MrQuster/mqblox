/* Darling/x86_64 only. Its sys_exit first calls native libc exit(), which
 * executes inherited Mesa atexit callbacks in forked children. _exit must
 * terminate without callbacks; use the same Linux exit_group syscall as
 * Darling's final sys_exit step, without native_exit.
 */
extern void _exit(int);
extern void _Exit(int);
extern void exit(int);
extern void abort(void);
extern char *getenv(const char *);
extern int write(int, const void *, unsigned long);
extern int backtrace(void **, int);
extern void backtrace_symbols_fd(void *const *, int, int);

static void print_num(long long n) {
    char buf[32];
    int i = 0;
    if (n < 0) {
        write(2, "-", 1);
        n = -n;
    }
    if (n == 0) {
        write(2, "0", 1);
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        write(2, &buf[j], 1);
    }
}

static int in_exit_trace = 0;

__attribute__((noreturn)) void macoblox_immediate_exit(int status) {
    if (!in_exit_trace) {
        in_exit_trace = 1;
        write(2, "\n[MacOBlox] _exit(", 18);
        print_num(status);
        write(2, ") called! Backtrace:\n", 21);
        void *frames[48];
        int n = backtrace(frames, 48);
        backtrace_symbols_fd(frames, n, 2);
    }
    __asm__ volatile("syscall" : : "a"(231L), "D"((long)status) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
__attribute__((used,section("__DATA,__interpose")))
static const void *exit_interpose[] = {(const void *)macoblox_immediate_exit, (const void *)_exit};

__attribute__((used,section("__DATA,__interpose")))
static const void *exit_capital_interpose[] = {(const void *)macoblox_immediate_exit, (const void *)_Exit};

static void macoblox_traced_exit(int status) {
    if (!in_exit_trace) {
        in_exit_trace = 1;
        write(2, "\n[MacOBlox] exit(", 17);
        print_num(status);
        write(2, ") called! Backtrace:\n", 21);
        void *frames[48];
        int n = backtrace(frames, 48);
        backtrace_symbols_fd(frames, n, 2);
    }
    exit(status);
}
__attribute__((used,section("__DATA,__interpose")))
static const void *exit_trace_interpose[] = {(const void *)macoblox_traced_exit, (const void *)exit};

static void macoblox_abort(void) {
    if (!in_exit_trace) {
        in_exit_trace = 1;
        write(2, "\n[MacOBlox] abort() called! Backtrace:\n", 39);
        void *frames[48];
        int n = backtrace(frames, 48);
        backtrace_symbols_fd(frames, n, 2);
    }
    macoblox_immediate_exit(134);
}
__attribute__((used,section("__DATA,__interpose")))
static const void *abort_interpose[] = {(const void *)macoblox_abort, (const void *)abort};

