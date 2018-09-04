/*
 * Copyright (C) 2018 Niko Rosvall <niko@byteptr.com>
 */

/* To get DT_REG */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include "directory_walker.h"
#include "utils.h"

static char *combine_path(const char *path, const char *filename)
{
    size_t size = (strlen(path) + strlen(filename)) + 2;
    char *fullpath = NULL;

    fullpath = tmalloc(size * sizeof(char));
    strcpy(fullpath, path);

    if(fullpath[strlen(fullpath) - 1] != '/')
        strcat(fullpath, "/");

    strcat(fullpath, filename);

    return fullpath;
}

/* In the future this should probably return a list of
 * files in the directory.
 */
void dir_walk(const char *path, actiononfile action, const char *pass)
{
    DIR *d;
    size_t count = 0;
    struct dirent *dir;
    d = opendir(path);

    if(d)
    {
        fprintf(stdout, "Progress: ");

        while((dir = readdir(d)) != NULL)
        {
            /* Make sure that the type is a regular file */
            if(dir->d_type == DT_REG)
            {
                char *fullpath = combine_path(path, dir->d_name);

                /* If the action is succesfull, print "progressbar" */
                if(action(pass, fullpath))
                {
                    fprintf(stdout, "#");
                    fflush(stdout);
                    count++;
                }

                free(fullpath);
            }
        }

        closedir(d);
        fprintf(stdout, "\n%zu files processed.\n", count);
    }
    else
    {
        fprintf(stderr, "Unable to open directory %s.\n", path);
    }
}

void dir_walk_recursive(const char *path, actiononfile action, const char *pass)
{


}
