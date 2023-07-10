#include <stdio.h>
#include "server.h"
#include "endpoint.h"
#include "error.h"
#include "log.h"


/** @brief Load the MRN table from disk */
static int tomo_server_load_table(TOMO_SERVER *serv, const wchar_t *path)
{
    if (tomo_csv_load(&serv->table, path)) {
        return 1;
    }
    return 0;
}


/** @brief Initialize Winsock2
 *  @param serv
 *      Server state buffer
 *  @returns Nonzero on error
 */
static int tomo_server_wsainit(TOMO_SERVER *serv)
{
    const WORD ver = MAKEWORD(2, 2);
    int res;

    res = WSAStartup(ver, &serv->wsadata);
    if (res) {
        tomo_error_raise(TOMO_ERROR_SOCK, &res, L"Cannot initialize Winsock2");
    }
    return res;
}


/** @brief Look up @p name and replace the string with the relevant MRN
 *  @param serv
 *      Server state
 *  @param name
 *      Name string, make sure this is nul-terminated
 *  @param len
 *      Length of the @p name buffer (NOT THE STRING!)
 */
static void tomo_server_name_lookup(TOMO_SERVER *serv, char *name, size_t len)
/** This function will have to be changed depending on the lookup methodology
 *  Access to the SQL server will obsolesce any other method
 */
{
    static const char *def = "NOT FOUND", *delim[2] = { "", "\n" };
    const TOMO_MRNPAIR *pair;
    unsigned i = 0;

    tomo_logf(TOMO_LOG_DEBUG, L"Looking up %S", name);
    pair = tomo_mrntable_lookup(&serv->table, name);
    if (!pair) {
        snprintf(name, len, def);
    } else if (tomo_mrnlist_sprint(name, len, pair->val)) {
        tomo_log_error(TOMO_LOG_WARN);
        tomo_error_reset();
    }
}


/** @brief Callback invoked on a client connection endpoint when data is ready
 *      to be read
 *  @param conn
 *      Connected endpoint
 *  @param arg
 *      Server state
 *  @returns A multiplexer status code
 */
static int tomo_server_connection_callback(TOMO_ENDPOINT *conn, void *arg)
{
    TOMO_SERVER *const serv = arg;
    char buf[512];
    size_t len = BUFLEN(buf) - 1;   /* Sub 1 to facilitate adding a nul term */

    if (tomo_endpoint_recv(conn, buf, &len)) {
        return TOMO_ENDPT_ERROR;
    } else if (!len) {
        return TOMO_ENDPT_CLOSED;
    }
    buf[len] = '\0';
    tomo_server_name_lookup(serv, buf, BUFLEN(buf));
    tomo_logf(TOMO_LOG_DEBUG, L"Replying with %S", buf);
    if (tomo_endpoint_send(conn, buf, strlen(buf)) < 0) {
        return TOMO_ENDPT_ERROR;
    }
    return 0;
}


/** @brief Callback invoked on a listening socket that is ready for IO
 *  @param lisnr
 *      The listening endpoint
 *  @param arg
 *      A pointer to the server state
 *  @returns One of the multiplexer status codes
 */
static int tomo_server_accept_callback(TOMO_ENDPOINT *lisnr, void *arg)
{
    TOMO_SERVER *const serv = arg;
    TOMO_ENDPOINT endp = {
        .proc = tomo_server_connection_callback,
        .data = arg
    };
    SHORT evt = POLLIN;
    wchar_t ip[65];
    int res = 0;

    if (tomo_endpoint_accept(lisnr, &endp)) {
        res = TOMO_ENDPT_ERROR;
    } else if (tomo_multiplexer_add(&serv->muxer, 1, &endp, &evt)) {
        tomo_endpoint_close(&endp);
        res = TOMO_ENDPT_ERROR;
    }
    if (!res) {
        tomo_sockaddr_str(ip, BUFLEN(ip), &endp.addr);
        tomo_logf(TOMO_LOG_INFO, L"Opened connection from %s", ip);
    }
    return res;
}


/** @brief Prepare the listener
 *  @param serv
 *      Server state buffer
 *  @param port
 *      Port to begin listening on
 *  @returns Nonzero on error
 */
static int tomo_server_open_listener(TOMO_SERVER *serv, u_short port)
{
    TOMO_ENDPOINT endp = {
        .proc = tomo_server_accept_callback,
        .data = serv
    };
    SHORT evt = POLLIN;
    int res;

    res = tomo_endpoint_open(&endp, AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (res) {
        return 1;
    }
    if (tomo_endpoint_dual(&endp)) {
        tomo_log_error(TOMO_LOG_WARN);
        tomo_error_reset();
    }
    res = tomo_endpoint_bind(&endp, port)
       || tomo_endpoint_listen(&endp, SOMAXCONN);
    if (!res) {
        res = tomo_multiplexer_add(&serv->muxer, 1, &endp, &evt);
    }
    return res;
}


/** @brief Initializes the handle for the current thread
 *  @param serv
 *      Server state
 *  @returns Nonzero on error
 */
static int tomo_server_init_current(TOMO_SERVER *serv)
{
    static const wchar_t *failmsg = L"Cannot get main thread handle";

    serv->monid = GetCurrentThreadId(); /* afaik this function can't fail */
    serv->monitor = OpenThread(THREAD_GET_CONTEXT, FALSE, serv->monid);
    if (!serv->monitor) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, failmsg);
    }
    return serv->monitor == NULL;
}


/** @brief Initialize the semaphore used to block the monitor thread
 *  @param serv
 *      Server state
 *  @returns Nonzero on error
 */
static int tomo_server_init_event(TOMO_SERVER *serv)
{
    static const wchar_t *failmsg = L"Cannot create APC event";

    serv->apc_evt = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!serv->apc_evt) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, failmsg);
    }
    return serv->apc_evt == NULL;
}


/** @brief Entry point for the polling thread
 *  @param arg
 *      Server state
 *  @returns Who cares
 */
static DWORD WINAPI tomo_server_poller(void *arg)
{
    TOMO_SERVER *serv = arg;
    int res;

    do {
        res = tomo_multiplexer_poll(&serv->muxer, -1);
    } while (!res);
    if (res) {
        tomo_log_error(TOMO_LOG_ERROR);
    }
    return res;
}


/** @brief Start the polling thread
 *  @param serv
 *      Server state
 *  @returns Nonzero on error
 */
static int tomo_server_start_poller(TOMO_SERVER *serv)
{
    static const wchar_t *failmsg = L"Cannot create socket poller thread";

    serv->poller = CreateThread(NULL,
                                0,
                                tomo_server_poller,
                                serv,
                                CREATE_SUSPENDED,
                                &serv->pollid);
    if (!serv->poller) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, failmsg);
    }
    return serv->poller == NULL;
}


/** @brief Create the poller thread and initialize thread handles/IDs
 *  @param serv
 *      Server state
 *  @returns Nonzero on error
 */
static int tomo_server_init_threads(TOMO_SERVER *serv)
{
    return tomo_server_init_current(serv)
        || tomo_server_init_event(serv)
        || tomo_server_start_poller(serv);
}


int tomo_server_open(TOMO_SERVER *serv, u_short port, const wchar_t *path)
{
    int res;

    res = tomo_server_load_table(serv, path)
       || tomo_server_wsainit(serv)
       || tomo_server_open_listener(serv, port);
    if (!res) {
        tomo_logf(TOMO_LOG_INFO, L"Opened listener on port %u", port);
        res = tomo_server_init_threads(serv);
    }
    return res;
}


int tomo_server_run(TOMO_SERVER *serv)
{
    DWORD res;

    res = ResumeThread(serv->poller);
    if (res == (DWORD)-1) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, L"Cannot resume poller");
        return 1;
    }
    do {
        res = WaitForSingleObjectEx(serv->apc_evt, INFINITE, TRUE);
        if (res == WAIT_FAILED) {
            tomo_error_raise(TOMO_ERROR_WIN32, NULL, L"Event blocking failed");
            serv->shouldquit = true;
        }
    } while (!serv->shouldquit);
    return res == WAIT_FAILED;
}


void tomo_server_shutdown(TOMO_SERVER *serv)
{
    if (serv) {
        serv->shouldquit = true;
        SetEvent(serv->apc_evt);
    }
}


void tomo_server_close(TOMO_SERVER *serv)
{
    CloseHandle(serv->apc_evt);
    TerminateThread(serv->poller, 0);
    CloseHandle(serv->poller);
    tomo_multiplexer_clear(&serv->muxer);
    tomo_mrntable_free(&serv->table);
    WSACleanup();
}
