#include <stdio.h>
#include <stdlib.h>
#include "multiplex.h"
#include "error.h"
#include "log.h"


/** @brief realloc(3) wrapper for propagating error states more simply
 *  @param ptr
 *      Address of pointer to block being reallocated
 *  @param newsize
 *      Size passed to realloc(3)
 *  @returns Nonzero on error
 */
static int tomo_multiplexer_realloc(void **ptr, size_t newsize)
{
    void *newptr;

    newptr = realloc(*ptr, newsize);
    if (!newptr) {
        tomo_error_raise(TOMO_ERROR_SYS, NULL, L"Cannot reallocate multiplexer");
        return 1;
    }
    *ptr = newptr;
    return 0;
}


int tomo_multiplexer_reserve(TOMO_MULTIPLEXER *muxer, ULONG newsize)
{
    ULONG newcap = muxer->cap;
    size_t datalen, fdslen;

    if (newcap >= newsize) {
        return 0;
    }
    do {
        newcap = newcap * 2 + 1;
    } while (newcap < newsize);
    datalen = sizeof *muxer->endpts * newcap;
    fdslen = sizeof *muxer->fds * newcap;
    if (tomo_multiplexer_realloc((void **)&muxer->endpts, datalen)
     || tomo_multiplexer_realloc((void **)&muxer->fds, fdslen)) {
        return 1;
    }
    muxer->cap = newcap;
    return 0;
}


int tomo_multiplexer_add(TOMO_MULTIPLEXER   *muxer,
                         ULONG               nfds,
                         const TOMO_ENDPOINT endpts[],
                         const SHORT         evts[])
{
    ULONG need = muxer->size + nfds;
    unsigned i;

    if (tomo_multiplexer_reserve(muxer, need)) {
        return 1;
    }
    for (i = 0; i < nfds; i++) {
        muxer->endpts[muxer->size + i] = endpts[i];
        muxer->fds[muxer->size + i].fd = endpts[i].sock;
        muxer->fds[muxer->size + i].events = evts[i];
    }
    muxer->size += nfds;
    return 0;
}


void tomo_multiplexer_close(TOMO_MULTIPLEXER *muxer, unsigned idx)
{
    const unsigned end = --muxer->size;
    wchar_t ip[65];

    tomo_sockaddr_str(ip, BUFLEN(ip), &muxer->endpts[idx].addr);
    tomo_logf(TOMO_LOG_INFO, L"Closing connection from %s", ip);
    tomo_endpoint_close(&muxer->endpts[idx]);
    memmove(&muxer->endpts[idx], &muxer->endpts[end], sizeof *muxer->endpts);
    memmove(&muxer->fds[idx], &muxer->fds[end], sizeof *muxer->fds);
}


/** hack */
static bool evt_has_data(SHORT evt)
{
    return evt & POLLERR || evt & POLLHUP || evt & POLLIN;
}


/** @brief Checks all file descriptors and invokes the relevant callbacks
 *  @param muxer
 *      Multiplexer
 */
static void tomo_multiplexer_update(TOMO_MULTIPLEXER *muxer)
{
    unsigned i;
    SHORT evt;
    int res;

    for (i = 0; i < muxer->size; i++) {
        evt = muxer->fds[i].revents;
        if (!evt) {
            continue;
        }
        if (evt_has_data(evt)) {
            res = tomo_endpoint_exec(&muxer->endpts[i]);
            switch (res) {
            case TOMO_ENDPT_ERROR:
                tomo_log_error(TOMO_LOG_ERROR);
                tomo_error_raise(TOMO_ERROR_NONE, NULL, NULL);
                /* FALL THRU */
            case TOMO_ENDPT_CLOSED:
                tomo_multiplexer_close(muxer, i--);
                /* FALL THRU */
            default:
                break;
            }
        }
    }
}


int tomo_multiplexer_poll(TOMO_MULTIPLEXER *muxer, int timeout)
{
    int res;

    res = WSAPoll(muxer->fds, muxer->size, timeout);
    if (res < 0) {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, L"Failed polling sockets");
        return 1;
    } else if (res) {
        tomo_multiplexer_update(muxer);
    }
    return 0;
}


void tomo_multiplexer_clear(TOMO_MULTIPLEXER *muxer)
{
    static const TOMO_MULTIPLEXER zero = { 0 };
    unsigned i;

    for (i = 0; i < muxer->size; i++) {
        tomo_endpoint_close(&muxer->endpts[i]);
    }
    free(muxer->endpts);
    free(muxer->fds);
    *muxer = zero;
}
