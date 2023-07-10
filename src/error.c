#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <winsock2.h>

#include <csv.h>

#include "error.h"


/** Thread-local error state buffer */
static thread_local struct {
    int type;
    wchar_t msg[256];
    wchar_t ctx[256];
    union {
        int   erno;
        int   wsock2;
        int   csv;
        DWORD win32;
    } data;
} state = { 0 };


/** @brief Removes the last CRLF in @p s, replacing it with a nul terminator
 *  @param s
 *      String to modify
 */
static void remove_crlf(wchar_t *s)
{
    static const wchar_t *crlf = L"\r\n";
    wchar_t *nptr;

    /* I feel like there's a much better way to do this */
    s = wcsstr(s, crlf);
    if (!s) {
        return;
    }
    nptr = wcsstr(s, crlf);
    while (nptr) {
        s = nptr;
        nptr = wcsstr(s + 2, crlf);
    }
    *s = L'\0';
}


/** @brief Raise a C stdlib error with errno
 *  @param erno
 *      Possibly NULL pointer to the offending error value
 */
static void tomo_error_system(int *erno)
{
    if (erno) {
        state.data.erno = *erno;
    } else {
        state.data.erno = errno;
    }
    _wcserror_s(state.msg, BUFLEN(state.msg), state.data.erno);
}


/** @brief Raise an error state from the native Windows API
 *  @param lasterr
 *      Possibly NULL pointer to the offending error value
 */
static void tomo_error_win32(DWORD *lasterr)
{
    if (lasterr) {
        state.data.win32 = *lasterr;
    } else {
        state.data.win32 = GetLastError();
    }
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL,
                   state.data.win32,
                   LANG_USER_DEFAULT,
                   state.msg,
                   BUFLEN(state.msg),
                   NULL);
}


/** @brief Raise an error state from Windows Sockets 2
 *  @param wserr
 *      Possibly NULL pointer to the Windows Sockets error code
 */
static void tomo_error_sock(int *wserr)
{
    if (wserr) {
        state.data.wsock2 = *wserr;
    } else {
        state.data.wsock2 = WSAGetLastError();
    }
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL,
                   (DWORD)state.data.wsock2,
                   LANG_USER_DEFAULT,
                   state.msg,
                   BUFLEN(state.msg),
                   NULL);
}


/** @brief Raise an error state from libcsv
 *  @param csvcode
 *      Pointer to libcsv error code. This *cannot* be NULL!
 */
static void tomo_error_csv(int *csvcode)
{
    assert(csvcode != NULL);
    state.data.csv = *csvcode;
    swprintf(state.msg, BUFLEN(state.msg), L"%S", csv_strerror(*csvcode));
}


/** @brief The message string was potentially supplied by the caller
 *  @param msg
 *      Possibly NULL pointer to a message string
 */
static void tomo_error_user(const wchar_t *msg)
{
    if (msg) {
        swprintf(state.msg, BUFLEN(state.msg), L"%s", msg);
    }
}


void tomo_error_raise(int type, void *data, const wchar_t *ctx, ...)
{
    va_list args;

    tomo_error_reset();
    state.type = type;
    switch (type) {
    case TOMO_ERROR_SYS:
        tomo_error_system(data);
        break;
    case TOMO_ERROR_WIN32:
        tomo_error_win32(data);
        break;
    case TOMO_ERROR_SOCK:
        tomo_error_sock(data);
        break;
    case TOMO_ERROR_CSV:
        tomo_error_csv(data);
        break;
    case TOMO_ERROR_USER:
        tomo_error_user(data);
        break;
    default:
        return;
    }
    va_start(args, ctx);
    vswprintf(state.ctx, sizeof state.ctx, ctx, args);
    va_end(args);
    remove_crlf(state.msg);
}


void tomo_error_reset(void)
{
    memset(&state, 0, sizeof state);
}


void tomo_error_strings(const wchar_t **msg, const wchar_t **ctx)
{
    if (msg) {
        *msg = state.msg;
    }
    if (ctx) {
        *ctx = state.ctx;
    }
}


void tomo_error_set_ctx(const wchar_t *ctx, ...)
{
    va_list args;

    va_start(args, ctx);
    vswprintf(state.ctx, BUFLEN(state.ctx), ctx, args);
    va_end(args);
}


bool tomo_error_state(void)
{
    return state.type != TOMO_ERROR_NONE;
}
