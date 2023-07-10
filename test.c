#include <stdio.h>
#include "src/endpoint.h"
#include "src/log.h"


int wmain(int argc, wchar_t *argv[])
{
    TOMO_ENDPOINT endp = { 0 };
    TOMO_LOGFILE lf = {
        .threshold = TOMO_LOG_DEBUG,
        .proc = _putws,
        .data = stdout
    };
    char query[325];
    size_t len = BUFLEN(query);

    WSAStartup(MAKEWORD(2, 2), &(WSADATA){ 0 });
    tomo_log_add(&lf);
    if (tomo_endpoint_open(&endp, AF_INET6, SOCK_STREAM, 0)
     || tomo_endpoint_connect(&endp, "localhost", NULL, 6006)) {
        tomo_log_error(TOMO_LOG_ERROR);
    } else {
        wcstombs(query, argv[1], BUFLEN(query));
        if (tomo_endpoint_send(&endp, query, strlen(query)) < 0
         || tomo_endpoint_recv(&endp, query, &len)) {
            tomo_log_error(TOMO_LOG_ERROR);
        } else {
            query[len] = '\0';
            fwprintf(stdout, L"Reply from server: %S\n", query);
        }
    }
    tomo_endpoint_close(&endp);
    return 0;
}
