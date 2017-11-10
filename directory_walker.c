/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#include <stdio.h>
#include <stdbool.h>
#include "directory_walker.h"

//In the future this should probably return a list of
//files in the directory.
void dir_walk(const char *path, actiononfile action, const char *pass)
{
    //TODO: Implement file looping for path
    action(pass, path);
}
