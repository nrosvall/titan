/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700
/* Make only POSIX.2 regexp functions available */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <regex.h>
#include "regexfind.h"
#include "entry.h"

void regex_find(const char *search)
{
    regex_t regex;

    if(regcomp(&regex, search, REG_ICASE) != 0)
    {
        fprintf(stderr, "Invalid regular expression.\n");
    }


}
