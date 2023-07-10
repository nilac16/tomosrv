#pragma once

#ifndef TOMOSRV_SERVER_H
#define TOMOSRV_SERVER_H

#include "defines.h"
#include "multiplex.h"
#include "csv.h"


typedef struct tomo_server {
    WSADATA wsadata;

    TOMO_MULTIPLEXER muxer;
    TOMO_MRNTABLE table;

    HANDLE monitor;
    DWORD monid;

    HANDLE poller;
    DWORD pollid;

    HANDLE apc_evt;
    bool shouldquit;
} TOMO_SERVER;


/** @brief Prepares the initial state of the server
 *  @param serv
 *      Server state buffer
 *  @param port
 *      Port to open the initial listener on
 *  @returns Nonzero on error
 */
int tomo_server_open(TOMO_SERVER *serv, u_short port, const wchar_t *path);


/** @brief Runs the server
 *  @param serv
 *      Server state buffer
 *  @returns The application exit code, which will be nonzero on error. Error
 *      information can be obtained by tomo_error_strings
 */
int tomo_server_run(TOMO_SERVER *serv);


/** @brief Sets the shutdown flag and issues an event signal to the main thread
 *  @param serv
 *      Server state buffer. If this argument is NULL, this operation nops
 */
void tomo_server_shutdown(TOMO_SERVER *serv);


/** @brief Releases all resources held by @p serv
 *  @param serv
 *      Server state buffer
 */
void tomo_server_close(TOMO_SERVER *serv);


#endif /* TOMOSRV_SERVER_H */
