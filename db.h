/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#ifndef __DB_H
#define __DB_H

bool db_init_new(const char *path);
bool db_insert_entry(Entry_t *entry);
bool db_update_entry(int id, Entry_t *new_entry);
bool db_delete_entry(int id, bool *changes);
Entry_t *db_get_entry_by_id(int id);
Entry_t *db_get_list();
Entry_t *db_find(const char *search);

#endif
