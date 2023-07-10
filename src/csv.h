#pragma once

#ifndef TOMOSRV_CSV_H
#define TOMOSRV_CSV_H

#include "defines.h"
#include "structures/table.h"


/** @brief Load the table data from a CSV file at @p path
 *  @param tbl
 *      MRN table. The memory managed by this object is modified, but assumed to
 *      be externally managed. On failure, you should free this object yourself
 *  @param path
 *      Path to the CSV
 *  @returns Nonzero on error
 */
int tomo_csv_load(TOMO_MRNTABLE *tbl, const wchar_t *path);


#endif /* TOMOSRV_CSV_H */
