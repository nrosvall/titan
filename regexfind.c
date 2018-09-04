/*
 * Copyright (C) 2018 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700
/* Make only POSIX.2 regexp functions available */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <regex.h>
#include "entry.h"
#include "utils.h"
#include "regexfind.h"

void regex_find(Entry_t *head, const char *search, int show_password)
{
    regex_t regex;
    int retval;

    if(regcomp(&regex, search, REG_NOSUB) != 0)
    {
        fprintf(stderr, "Invalid regular expression.\n");
        return;
    }

    /* Currently regex only searches for the title field */
    while(head != NULL)
    {
        retval = regexec(&regex, head->title, 0, NULL, 0);

        if(retval == 0)
        {
            /* We have a match */
            print_entry(head, show_password);
        }

        head = head->next;
    }

    regfree(&regex);
}
