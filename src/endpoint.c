#include <stdio.h>

#include "endpoint.h"
#include "error.h"
#include "log.h"


void tomo_sockaddr_str(wchar_t *buf, size_t len, const TOMO_SOCKADDR46 *addr)
{
    const int fam = addr->gen.sa_family;
    const void *cp = NULL;

    switch (fam) {
    case AF_INET:
        cp = &addr->in4.sin_addr;
        break;
    case AF_INET6:
        cp = &addr->in6.sin6_addr;
        break;
    default:
        tomo_logf(TOMO_LOG_ERROR, __FUNCTIONW__ L": Invalid address family %d", fam);
        if (len) {
            *buf = '\0';
        }
        return;
    }
    InetNtopW(fam, cp, buf, len);
}


/** @brief Gets the size of the socket address buffer expected by system calls
 *  @param addr
 *      Socket address union
 *  @returns The length in bytes required by the address family currently
 *      contained by @p addr
 */
static socklen_t tomo_sockaddr_len(const TOMO_SOCKADDR46 *addr)
{
    const int af = addr->gen.sa_family;

    switch (af) {
    case AF_INET:
        return sizeof addr->in4;
    case AF_INET6:
        return sizeof addr->in6;
    default:
        tomo_logf(TOMO_LOG_ERROR, __FUNCTIONW__ L": Invalid address family %d", af);
        return 0;
    }
}


/** @brief Sets the socket address to listen to any incoming connection
 *  @param addr
 *      Socket address
 *  @param port
 *      Port to listen on
 */
static void tomo_sockaddr_any(TOMO_SOCKADDR46 *addr,
                              u_short          port)
{
    const int af = addr->gen.sa_family;

    switch (af) {
    case AF_INET:
        addr->in4.sin_addr = in4addr_any;
        addr->in4.sin_port = htons(port);
        break;
    case AF_INET6:
        addr->in6.sin6_addr = in6addr_any;
        addr->in6.sin6_port = htons(port);
        break;
    default:
        tomo_logf(TOMO_LOG_ERROR, __FUNCTIONW__ L": Invalid address family %d", af);
        break;
    }
}


static void tomo_sockaddr_set_port(TOMO_SOCKADDR46 *addr,
                                   u_short          port)
{
    const int af = addr->gen.sa_family;

    switch (af) {
    case AF_INET:
        addr->in4.sin_port = htons(port);
        break;
    case AF_INET6:
        addr->in6.sin6_port = htons(port);
        break;
    default:
        tomo_logf(TOMO_LOG_ERROR, __FUNCTIONW__ L": Invalid address family %d", af);
        break;
    }
}


int tomo_endpoint_open(TOMO_ENDPOINT *endp, int af, int type, int protocol)
{
    static const wchar_t *failmsg = L"Cannot open socket";

    endp->sock = socket(af, type, protocol);
    if (endp->sock != INVALID_SOCKET) {
        endp->addr.gen.sa_family = af;
    } else {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, failmsg);
    }
    return endp->sock == INVALID_SOCKET;
}


int tomo_endpoint_dual(TOMO_ENDPOINT *endp)
{
    const int val = 0;
    int res;

    res = setsockopt(endp->sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&val, sizeof val);
    if (res) {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, L"Failed to apply dual-stacking");
    }
    return res;
}


int tomo_endpoint_bind(TOMO_ENDPOINT *endp, u_short port)
{
    static const wchar_t *failmsg = L"Cannot bind socket";
    socklen_t len;
    int res;

    tomo_sockaddr_any(&endp->addr, port);
    len = tomo_sockaddr_len(&endp->addr);
    res = bind(endp->sock, &endp->addr.gen, len);
    if (res) {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, failmsg);
    }
    return res;
}


int tomo_endpoint_listen(TOMO_ENDPOINT *endp, int count)
{
    static const wchar_t *failmsg = L"Socket cannot listen";
    int res;

    res = listen(endp->sock, count);
    if (res) {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, failmsg);
    }
    return res;
}


int tomo_endpoint_accept(const TOMO_ENDPOINT *serv, TOMO_ENDPOINT *conn)
{
    static const wchar_t *failmsg = L"Cannot accept connection";
    TOMO_SOCKADDR46 addr = { 0 };
    socklen_t len = sizeof addr;
    SOCKET sock;

    sock = accept(serv->sock, &addr.gen, &len);
    if (sock == INVALID_SOCKET) {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, failmsg);
        return 1;
    }
    conn->sock = sock;
    conn->addr = addr;
    return 0;
}


int tomo_endpoint_connect(TOMO_ENDPOINT *endp,
                          const char    *host,
                          const char    *svc,
                          u_short        port)
/* This needs to be modified such that it opens the socket too, so that it can
match the address family returned by getaddrinfo(3) */
{
    ADDRINFO hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
        .ai_flags    = AI_ADDRCONFIG
    }, *ai, *node;
    wchar_t ip[65];
    int res;

    res = getaddrinfo(host, svc, &hints, &ai);
    if (res) {
        tomo_error_raise(TOMO_ERROR_SOCK, &res, L"Failed resolving host %S", host);
        return 1;
    }
    for (node = ai; node; node = node->ai_next) {
        tomo_sockaddr_str(ip, BUFLEN(ip), (TOMO_SOCKADDR46 *)node->ai_addr);
        tomo_logf(TOMO_LOG_DEBUG, L"Attempting to connect to %s", ip);
        if (!svc) {
            tomo_sockaddr_set_port((TOMO_SOCKADDR46 *)node->ai_addr, port);
        }
        res = connect(endp->sock, ai->ai_addr, (int)ai->ai_addrlen);
        if (!res) {
            memcpy(&endp->addr, ai->ai_addr, ai->ai_addrlen);
            break;
        }
    }
    if (res) {
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, L"Failed connecting to %S", host);
    }
    freeaddrinfo(ai);
    return res;
}


int tomo_endpoint_recv(TOMO_ENDPOINT *endp, char *buf, size_t *len)
{
    wchar_t ip[65];
    int nread;

    nread = recv(endp->sock, buf, (int)*len, 0);
    if (nread < 0) {
        tomo_sockaddr_str(ip, BUFLEN(ip), &endp->addr);
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, L"Failed receiving data from %s", ip);
        return 1;
    }
    *len = nread;
    return 0;
}


int tomo_endpoint_send(TOMO_ENDPOINT *endp, const char *buf, size_t len)
{
    wchar_t ip[65];
    int nread;

    nread = send(endp->sock, buf, (int)len, 0);
    if (nread < 0) {
        tomo_sockaddr_str(ip, BUFLEN(ip), &endp->addr);
        tomo_error_raise(TOMO_ERROR_SOCK, NULL, L"Failed sending data to %s", ip);
        return -1;
    }
    return nread;
}


int tomo_endpoint_close(TOMO_ENDPOINT *endp)
{
    int res;
    
    res = closesocket(endp->sock);
    endp->sock = INVALID_SOCKET;
    return res;
}
