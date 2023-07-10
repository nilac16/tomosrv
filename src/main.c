#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include "error.h"
#include "log.h"

#define PROGNAME "tomosrv"


static int wmain_filter(DWORD xcode, EXCEPTION_POINTERS *ptrs)
{
    const wchar_t *message;
    wchar_t buf[256];

    (void)ptrs;
    switch (xcode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        message = L"Segmentation fault";
        break;
    case EXCEPTION_GUARD_PAGE:
        /* This may not actually yield useful information */
        message = L"#GP violation";
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        /* Again, pretty sure this is just a #GP and Windows tracks too much */
        message = L"Page error";
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        message = L"Illegal instruction";
        break;
    case EXCEPTION_BREAKPOINT:
        message = L"Trace/breakpoint trap";
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        /* I read somewhere that Windows breaks if you enable this exception */
        message = L"Bus error";
        break;
    case EXCEPTION_INVALID_HANDLE:
        /* Do you actually get SEH exceptions for something this facile */
        message = L"Invalid handle";
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        message = L"Privileged instruction";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        message = L"Stack overflow";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        message = L"Division by zero";
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_STACK_CHECK:
        /* Since it's no longer the 90s, I DON'T THINK we'll be seeing many FPEs */
        message = L"Floating-point exception";
        break;
    default:
        swprintf(buf, BUFLEN(buf), L"Unhandled SEH exception %#x", xcode);
        message = buf;
        break;
    }
    tomo_logs(TOMO_LOG_ERROR, message);
    return ExceptionContinueSearch;
}


static TOMO_SERVER *serv = NULL;


static BOOL WINAPI wmain_interrupt_handler(DWORD ctype)
{
    if (ctype == CTRL_C_EVENT) {
        tomo_logs(TOMO_LOG_INFO, L"CTRL-C");
        tomo_server_shutdown(serv);
        return TRUE;
    }
    return FALSE;
}


static HANDLE hcons = NULL;


int wmain_log(const wchar_t *msg, void *data, TOMO_LOGLVL lvl)
{
    static const wchar_t *progname = L"tomosrv: ";
    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
    const wchar_t *prefix;
    WORD color;
    FILE *fp;

    (void)data;

    GetConsoleScreenBufferInfo(hcons, &info);
    switch (lvl) {
    case TOMO_LOG_DEBUG:
        fp = stdout;
        color = FOREGROUND_BLUE;
        prefix = L"debug: ";
        break;
    case TOMO_LOG_INFO:
        fp = stdout;
        color = info.wAttributes;
        prefix = L"";
        break;
    case TOMO_LOG_WARN:
        color = FOREGROUND_RED | FOREGROUND_GREEN;
        fp = stderr;
        prefix = L"warning: ";
        break;
    case TOMO_LOG_ERROR:
        color = FOREGROUND_RED;
        fp = stderr;
        prefix = L"error: ";
        break;
    }
    color |= FOREGROUND_INTENSITY;
    fputws(progname, fp);
    SetConsoleTextAttribute(hcons, color);
    fwprintf(fp, L"%s%s\n", prefix, msg);
    SetConsoleTextAttribute(hcons, info.wAttributes);
    return 0;
}


struct args {
    jmp_buf env;
    int argc, i;
    wchar_t **argv;
    u_short port;
    const wchar_t *path;
};


enum {
    OPT_ARG,
    OPT_SHORT,
    OPT_LONG
};

static int wmain_arg_type(const wchar_t *arg)
{
    if (arg[0] == L'-') {
        if (arg[1] == L'-') {
            if (arg[2]) {
                return OPT_LONG;
            }
        } else if (arg[1]) {
            return OPT_SHORT;
        }
    }
    return OPT_ARG;
}

static const wchar_t *wmain_next_arg(struct args *args)
{
    return args->argv[++args->i];
}


static int wmain_read_port(struct args *args)
{
    const wchar_t *op;

    op = wmain_next_arg(args);
    if (op && wmain_arg_type(op) == OPT_ARG) {
        args->port = (u_short)wcstol(op, NULL, 0);
        return 0;
    }
    return 1;
}


static void wmain_parse_short(struct args *args, const wchar_t *arg)
{
    wchar_t c = *arg;

    do {
        switch (c) {
        case L'p':
            if (wmain_read_port(args)) {
                tomo_error_raise(TOMO_ERROR_USER, NULL, L"Short option -p requires an argument");
                longjmp(args->env, 1);
            }
            return;
        default:
            tomo_logf(TOMO_LOG_WARN, L"Unrecognized short option %c", c);
            break;
        }
        c = *(++arg);
    } while (c);
}


static void wmain_parse_long(struct args *args, const wchar_t *arg)
{
    if (!wcscmp(arg, L"port")) {
        if (wmain_read_port(args)) {
            tomo_error_raise(TOMO_ERROR_USER, NULL, L"Long option --port requires an argument");
            longjmp(args->env, 1);
        }
    } else {
        tomo_logf(TOMO_LOG_WARN, L"Unrecognized long option %s", arg);
    }
}


/** @brief Read in any arguments */
static void wmain_parse_args(struct args *args)
{
    const wchar_t *arg;

    args->i = 0;
    arg = wmain_next_arg(args);
    while (arg) {
        switch (wmain_arg_type(arg)) {
        case OPT_SHORT:
            wmain_parse_short(args, arg + 1);
            break;
        case OPT_LONG:
            wmain_parse_long(args, arg + 2);
            break;
        case OPT_ARG:
        default:
            args->path = arg;
            /* Argument parsing ends after reading the CSV path */
            return;
        }
        arg = wmain_next_arg(args);
    }
}


static void wmain_print_usage(void)
{
    static const wchar_t *usage =
    L"Usage: " PROGNAME " [OPTION] CSV\n"
    L"Start an MRN lookup server. Match patient names provided by clients to their MRN\n"
    L"by using MOSAIQ schedule table CSV\n"
    L"\n"
    L"Options:\n"
    L"    -p, --port PORT        open listener on port PORT (default 6006)\n";

    fputws(usage, stdout);
}


int wmain(int argc, wchar_t *argv[])
{
    TOMO_SERVER server = { 0 };
    TOMO_LOGFILE log = {
        .threshold = TOMO_LOG_DEBUG,
        .proc = wmain_log,
        .data = NULL
    };
    struct args args = {
        .argc = argc,
        .argv = argv,
        .port = 6006,
        .path = NULL
    };
    int res;
    
    hcons = GetStdHandle(STD_OUTPUT_HANDLE);
    serv = &server;
    __try {
        tomo_log_add(&log);
        if (setjmp(args.env)) {
            tomo_log_error(TOMO_LOG_ERROR);
            return 1;
        }
        wmain_parse_args(&args);
        if (!args.path) {
            tomo_logs(TOMO_LOG_ERROR, L"CSV file is required");
            wmain_print_usage();
            return 1;
        }
        res = tomo_server_open(&server, args.port, args.path);
        if (!res) {
            SetConsoleCtrlHandler(wmain_interrupt_handler, TRUE);
            res = tomo_server_run(&server);
        }
        tomo_server_close(&server);
        if (res) {
            tomo_log_error(TOMO_LOG_ERROR);
        }
        tomo_log_remove(&log);
    } __except (wmain_filter(GetExceptionCode(), GetExceptionInformation())) {

    }
    return res;
}
