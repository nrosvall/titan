/*
 * Copyright (C) 2018 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "utils.h"

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
    new->next = NULL;

    return new;
}

/* Add new entry to the end of the list. Returns the currently added entry */
Entry_t *entry_add(Entry_t *head, const char *title, const char *user,
                   const char *url, const char *password, const char *notes)
{
    if(head == NULL)
    {
        return head = entry_new(title, user, url, password, notes);
    }

    Entry_t *cur = head;

    /* Walk until we're in the end of the list */
    while(cur->next != NULL)
        cur = cur->next;

    Entry_t *new = entry_new(title, user, url, password, notes);
    cur->next = new;

    return cur->next;
}

void entry_free(Entry_t *entry)
{
    Entry_t *tmp;

    while(entry != NULL)
    {
        tmp = entry->next;

        free(entry->title);
        free(entry->user);
        free(entry->url);
        free(entry->password);
        free(entry->notes);

        if(entry->stamp)
            free(entry->stamp);

        free(entry);
        entry = tmp;
    }
}
