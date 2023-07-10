#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "error.h"


static struct loglist {
    struct loglist *next;

    TOMO_LOGFILE *lf;
} *logs = NULL;


int tomo_log_add(TOMO_LOGFILE *lf)
{
    static const wchar_t *failmsg = L"Failed to allocate logger node";
    struct loglist *node;

    node = malloc(sizeof *node);
    if (!node) {
        tomo_error_raise(TOMO_ERROR_SYS, NULL, failmsg);
        return 1;
    }
    node->next = logs;
    node->lf = lf;
    logs = node;
    return 0;
}


void tomo_log_remove(const TOMO_LOGFILE *lf)
{
    struct loglist **node, *rm;

    for (node = &logs; *node; node = &(*node)->next) {
        if ((*node)->lf == lf) {
            rm = *node;
            *node = (*node)->next;
            free(rm);
            break;
        }
    }
}


void tomo_logs(TOMO_LOGLVL lvl, const wchar_t *msg)
{
    struct loglist *node;

    for (node = logs; node; node = node->next) {
        if (node->lf->threshold <= lvl) {
            node->lf->proc(msg, node->lf->data, lvl);
        }
    }
}


void tomo_logf(TOMO_LOGLVL lvl, const wchar_t *fmt, ...)
{
    wchar_t buf[256];
    va_list args;

    va_start(args, fmt);
    vswprintf(buf, BUFLEN(buf), fmt, args);
    va_end(args);
    tomo_logs(lvl, buf);
}


void tomo_log_error(TOMO_LOGLVL lvl)
{
    const wchar_t *msg, *ctx;

    tomo_error_strings(&msg, &ctx);
    if (*msg) {
        tomo_logf(lvl, L"%s: %s", ctx, msg);
    } else {
        tomo_logs(lvl, ctx);
    }
}
