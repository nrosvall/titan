/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#ifndef __ENTRY_H
#define __ENTRY_H

typedef struct _entry
{
    int id;
    char *title;
    char *user;
    char *url;
    char *password;
    char *notes;
    /* Currently stamp is only used by db_get_entry_by_id */
    char *stamp;

} Entry_t;


Entry_t *entry_new(const char *title, const char *user, const char *url,
                   const char *password, const char *notes);

void entry_free(Entry_t *entry);

#endif
