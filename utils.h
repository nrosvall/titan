/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */


#ifndef __UTILS_H
#define __UTILS_H

#include <stdbool.h>
#include "entry.h"

bool print_entry(Entry_t *entry, int show_password);
char *get_lockfile_path();
void write_active_database_path(const char *db_path);
char *read_active_database_path();
bool has_active_database();
void *tmalloc(size_t size);
bool file_exists(const char *path);

#endif
