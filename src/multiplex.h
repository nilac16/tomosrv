#pragma once

#ifndef TOMOSRV_MULTIPLEX_H
#define TOMOSRV_MULTIPLEX_H

#include "defines.h"
#include "endpoint.h"
#include <winsock2.h>


/** Zero-initialize this. This interface relies upon standard-guaranteed
 *  behavior involving NULL pointers
 */
typedef struct tomo_multiplexer {
    ULONG size, cap;

    TOMO_ENDPOINT *endpts;

    WSAPOLLFD *fds;
} TOMO_MULTIPLEXER;


/** @brief Reserve at least @p newsize elements in @p muxer
 *  @param muxer
 *      Multiplexer
 *  @param newsize
 *      Minimum required size
 *  @returns Nonzero on error
 */
int tomo_multiplexer_reserve(TOMO_MULTIPLEXER *muxer, ULONG newsize);


/** @brief Add @p nfds sockets to the multiplexer
 *  @param muxer
 *      Multiplexer
 *  @param nfds
 *      Number of sockets to add
 *  @param endpts
 *      Pointer to array of endpoints to be copied in
 *  @param evts
 *      Pointer to array of requested return events
 *  @returns Nonzero on error
 */
int tomo_multiplexer_add(TOMO_MULTIPLEXER   *muxer,
                         ULONG               nfds,
                         const TOMO_ENDPOINT endpts[],
                         const SHORT         evts[]);


/** @brief Close the socket associated with @p idx
 *  @param muxer
 *      Multiplexer
 *  @param idx
 *      Index of the socket to be closed
 */
void tomo_multiplexer_close(TOMO_MULTIPLEXER *muxer, unsigned idx);


/** @brief Poll the multiplexer
 *  @param muxer
 *      Multiplexer
 *  @param timeout
 *      Timeout parameter passed directly to poll(2) (WSAPoll(2))
 *  @returns Nonzero on error. Errors from callbacks are not propagated, do your
 *      own error handling therein
 */
int tomo_multiplexer_poll(TOMO_MULTIPLEXER *muxer, int timeout);


/** @brief Frees all memory held by @p muxer and zeroes it
 *  @param muxer
 *      Multiplexer. This must have been zero-initialized or modified only
 *      through previous calls to this interface
 */
void tomo_multiplexer_clear(TOMO_MULTIPLEXER *muxer);


#endif /* TOMOSRV_MULTIPLEX_H */
