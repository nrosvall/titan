/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "entry.h"

/* Allocate and return a new entry containing data.
   Called must free the return value.
*/
Entry_t *entry_new(const char *title, const char *user,
                   const char *url, const char *password,
                   const char *notes)
{

    Entry_t *new = NULL;

    new = tmalloc(sizeof(struct _entry));

    new->title = strdup(title);
    new->user = strdup(user);
    new->url = strdup(url);
    new->password = strdup(password);
    new->notes = strdup(notes);
    new->stamp = NULL;

    return new;
}

void entry_free(Entry_t *entry)
{
    if(!entry)
        return;

    free(entry->title);
    free(entry->user);
    free(entry->url);
    free(entry->password);
    free(entry->notes);

    if(entry->stamp)
        free(entry->stamp);

    free(entry);
}
