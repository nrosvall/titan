/*
 * Copyright (C) 2018 Niko Rosvall <niko@byteptr.com>
 */

#ifndef __DIRECTORY_WALKER_H
#define __DIRECTORY_WALKER_H

typedef bool (*actiononfile)(const char *, const char *);

void dir_walk(const char *path, actiononfile action, const char *pass);
void dir_walk_recursive(const char *path, actiononfile action, const char *pass);

#endif
