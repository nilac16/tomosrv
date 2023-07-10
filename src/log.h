#pragma once

#ifndef TOMOSRV_LOG_H
#define TOMOSRV_LOG_H

#include "defines.h"


typedef enum {
    TOMO_LOG_DEBUG,
    TOMO_LOG_INFO,
    TOMO_LOG_WARN,
    TOMO_LOG_ERROR
} TOMO_LOGLVL;


/** @brief Logging callback */
typedef int TOMO_LOGPROC(const wchar_t *msg, void *data, TOMO_LOGLVL lvl);


typedef struct tomo_logfile {
    TOMO_LOGLVL   threshold;
    TOMO_LOGPROC *proc;
    void         *data;
} TOMO_LOGFILE;


/** @brief Add a logging context to the list of logs
 *  @param lf
 *      Log file. This memory is externally managed, and must be valid until it
 *      is removed from the logging list
 *  @returns Nonzero on error
 */
int tomo_log_add(TOMO_LOGFILE *lf);


/** @brief Remove a logging callback from the list
 *  @param lf
 *      Log file to be removed. The correct file is found by direct pointer
 *      comparison, so it must be a pointer to the exact same object that was
 *      placed in the list with tomo_log_add
 */
void tomo_log_remove(const TOMO_LOGFILE *lf);


/** @brief Issue @p msg to all logs
 *  @param lvl
 *      Logging level
 *  @param msg
 *      Message to send
 */
void tomo_logs(TOMO_LOGLVL lvl, const wchar_t *msg);


/** @brief Issue formatted output to all logs
 *  @param lvl
 *      Logging level
 *  @param fmt
 *      Standard C format string
 */
void tomo_logf(TOMO_LOGLVL lvl, const wchar_t *fmt, ...);


/** @brief Fetches the thread's error state and issues a message to logs
 *  @param lvl
 *      Log level to issue the message to. This should be either ERROR or WARN,
 *      but nothing's stopping you from issuing this to debug or trace logs...
 */
void tomo_log_error(TOMO_LOGLVL lvl);


#endif /* TOMOSRV_LOG_H */
