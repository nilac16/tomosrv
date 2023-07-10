#pragma once

#ifndef TOMOSRV_DEFINES_H
#define TOMOSRV_DEFINES_H

#define UNICODE 1
#define _UNICODE 1

#include <stdbool.h>

#define BUFLEN(buf) (sizeof (buf) / sizeof *(buf))

#define thread_local __declspec(thread)


#endif /* TOMOSRV_DEFINES_H */
