#pragma once

#ifndef ENDPOINT_H
#define ENDPOINT_H

#include "defines.h"
#include <ws2tcpip.h>


typedef union tomo_sockaddr46 {
    struct sockaddr     gen;
    struct sockaddr_in  in4;
    struct sockaddr_in6 in6;
} TOMO_SOCKADDR46;


/** @brief Prints the presentation form of the contained socket address @p addr
 *      to the buffer @p buf
 *  @param buf
 *      Destination buffer
 *  @param len
 *      Length of @p buf
 *  @param addr
 *      Socket address
 */
void tomo_sockaddr_str(wchar_t *buf, size_t len, const TOMO_SOCKADDR46 *addr);


enum {
    TOMO_ENDPT_ERROR  = -2,
    TOMO_ENDPT_CLOSED = -1
};


typedef struct tomo_endpoint {
    TOMO_SOCKADDR46 addr;
    SOCKET sock;

    /** STRONGly consider making the poll callba% part of the endpoint */
    int (*proc)(struct tomo_endpoint *endp, void *data);
    void *data;
} TOMO_ENDPOINT;


/** @brief Opens the socket and sets the address family
 *  @param endp
 *      Endpoint
 *  @param af
 *      Address family
 *  @param type
 *      Socket type
 *  @param protocol
 *      Protocol
 *  @returns Nonzero on error
 */
int tomo_endpoint_open(TOMO_ENDPOINT *endp, int af, int type, int protocol);


/** @brief Set IPv6 endpoint @p endp to dual-stacking mode
 *  @param endp
 *      Endpoint
 *  @returns Nonzero on error
 */
int tomo_endpoint_dual(TOMO_ENDPOINT *endp);


/** @brief Bind @p endp to @p port
 *  @param endp
 *      Endpoint
 *  @param port
 *      Port to bind
 *  @returns Nonzero on error
 */
int tomo_endpoint_bind(TOMO_ENDPOINT *endp, u_short port);


/** @brief Begin listening on the socket contained by @p endp
 *  @param endp
 *      Endpoint
 *  @param count
 *      Connection buffer size
 *  @returns Nonzero on error
 */
int tomo_endpoint_listen(TOMO_ENDPOINT *endp, int count);


/** @brief Accept a pending connection on @p serv and bind its details to
 *      @p conn
 *  @param serv
 *      Listening endpoint
 *  @param conn
 *      The connection details will be written here
 *  @returns Nonzero on error, including EWOULDBLOCK!
 */
int tomo_endpoint_accept(const TOMO_ENDPOINT *serv, TOMO_ENDPOINT *conn);


/** @brief Connect to @p host
 *  @param endp
 *      Endpoint
 *  @param host
 *      Hostname. This parameter cannot be NULL
 *  @param svc
 *      Service name. If this is NULL, then @p port is used instead
 *  @param port
 *      Port to connect to. This is ignored if @p svc is not NULL
 *  @returns Nonzero on error
 */
int tomo_endpoint_connect(TOMO_ENDPOINT *endp,
                          const char    *host,
                          const char    *svc,
                          u_short        port);


/** @brief Receive data buffered by the system for @p endp
 *  @param endp
 *      Endpoint
 *  @param buf
 *      Destination buffer
 *  @param[in,out] len
 *      On input, the maximum size of @p buf. On output the number of characters
 *      written to the buffer if there was no error
 *  @returns Nonzero on error
 */
int tomo_endpoint_recv(TOMO_ENDPOINT *endp, char *buf, size_t *len);


/** @brief Send data to @p endp
 *  @param endp
 *      Endpoint
 *  @param buf
 *      Buffer to send
 *  @param len
 *      Number of bytes in @p buf to send
 *  @returns Negative on error, the number of bytes sent on success
 */
int tomo_endpoint_send(TOMO_ENDPOINT *endp, const char *buf, size_t len);


/** @brief Executes this endpoint's associated function
 *  @param endp
 *      Endpoint
 *  @returns The result of the function
 */
static inline int tomo_endpoint_exec(TOMO_ENDPOINT *endp)
{
    return endp->proc(endp, endp->data);
}


/** @brief Close the socket. This operation invalidates the contained SOCKET
 *      handle
 *  @param endp
 *      Endpoint to close
 *  @warning This function propagates the return value from the syscall, but
 *      does NOT raise an error state
 *  @note This function does not touch the socket address contained by @p endp.
 *      You may zero this information if you wish, but as long as you adhere to
 *      this API, it will not matter
 */
int tomo_endpoint_close(TOMO_ENDPOINT *endp);


#endif /* ENDPOINT_H */
