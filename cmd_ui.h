/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#ifndef __CMD_UI_H
#define __CMD_UI_H

void init_database(const char *path, int force, int auto_encrypt);
bool add_new_entry(int auto_encrypt);
bool edit_entry(int id, int auto_encrypt);
bool remove_entry(int id, int auto_encrypt);
void list_by_id(int id, int show_password, int auto_encrypt);
void list_all(int show_password, int auto_encrypt);
void find(const char *search, int show_password, int auto_encrypt);
void show_current_db_path();
void set_use_db(const char *path);

bool decrypt_database(const char *path);
bool encrypt_database();

#endif
