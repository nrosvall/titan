/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "cmd_ui.h"
#include "entry.h"
#include "db.h"
#include "utils.h"
#include "crypto.h"
#include "pwd-gen.h"
#include "regexfind.h"

/*Removes new line character from a string.*/
static void strip_newline_str(char *str)
{
    char *i = str;
    char *j = str;

    while (*j != '\0')
    {
        *i = *j++;

        if(*i != '\n')
            i++;
    }

    *i = '\0';
}

/* Function assumes that the in_buffer has enough space.
 */
static void generate_new_password(char *in_buffer)
{
    char *new_pass = NULL;
    new_pass = generate_password(16);

    if(!new_pass)
    {
        fprintf(stderr, "WARNING: Unable to generate new password.\n");
    }
    else
    {
        strcpy(in_buffer, new_pass);
        free(new_pass);
    }
}

/*Turns echo of from the terminal and asks for a passphrase.
 *Usually stream is stdin. Returns length of the passphrase,
 *passphrase is stored to lineptr. Lineptr must be allocated beforehand.
 */
static size_t my_getpass(char *prompt, char **lineptr, size_t *n, FILE *stream)
{
    struct termios old, new;
    int nread;

    /*Turn terminal echoing off.*/
    if(tcgetattr(fileno(stream), &old) != 0)
        return -1;

    new = old;
    new.c_lflag &= ~ECHO;

    if(tcsetattr(fileno(stream), TCSAFLUSH, &new) != 0)
        return -1;

    if(prompt)
        printf("%s", prompt);

    /*Read the password.*/
    nread = getline(lineptr, n, stream);

    if(nread >= 1 && (*lineptr)[nread - 1] == '\n')
    {
        (*lineptr)[nread - 1] = 0;
        nread--;
    }

    printf("\n");

    /*Restore terminal echo.*/
    tcsetattr(fileno(stream), TCSAFLUSH, &old);

    return nread;
}

static void auto_enc()
{
    fprintf(stdout, "Auto encrypt enabled, type password to encrypt.\n");
    encrypt_database();
}

void init_database(const char *path, int force, int auto_encrypt)
{
    if(!has_active_database() || force == 1)
    {
        //If forced, delete any existing file
        if(force == 1)
        {
            if(file_exists(path))
                unlink(path);
        }

        if(db_init_new(path))
        {
            write_active_database_path(path);

            if(auto_encrypt == 1)
                auto_enc();
        }
    }
    else
    {
        fprintf(stderr, "Existing database is already active. "
                "Encrypt it before creating a new one.\n");
    }

}

bool decrypt_database(const char *path)
{
    if(has_active_database())
    {
        fprintf(stderr, "Existing database is already active. "
                "Encrypt it before decrypting another one.\n");

        return false;
    }

    size_t pwdlen = 1024;
    char pass[pwdlen];
    char *ptr = pass;

    my_getpass("Password: ", &ptr, &pwdlen, stdin);

    if(!decrypt_file(pass, path))
    {
        fprintf(stderr, "Failed to decrypt %s.\n", path);
        return false;
    }

    write_active_database_path(path);

    return true;
}

bool encrypt_database()
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return false;
    }

    size_t pwdlen = 1024;
    char pass[pwdlen];
    char *ptr = pass;
    char pass2[pwdlen];
    char *ptr2 = pass2;
    char *path = NULL;
    char *lockfile_path = NULL;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Unable to read activate database path.\n");
        return false;
    }

    my_getpass("Password: ", &ptr, &pwdlen, stdin);
    my_getpass("Password again: ", &ptr2, &pwdlen, stdin);

    if(strcmp(pass, pass2) != 0)
    {
        fprintf(stderr, "Password mismatch.\n");
        free(path);
        return false;
    }

    if(!encrypt_file(pass, path))
    {
        fprintf(stderr, "Encryption of %s failed.\n", path);
        free(path);
        return false;
    }

    free(path);

    lockfile_path = get_lockfile_path();

    if(!lockfile_path)
    {
        fprintf(stderr, "Unable to retrieve the lock file path.\n");
        return false;
    }

    //Finally delete the file that holds the activate database path.
    //This way we allow Titan to create a new database or open another one.
    unlink(lockfile_path);
    free(lockfile_path);

    return true;
}


//TODO: This function should not look for database path (see encrypt_file?)
//Also we should probably use some other extension than the default one, for
//normal files. maybe foo.txt.titan for example?
bool encrypt_any_file(const char *path)
{
    size_t pwdlen = 1024;
    char pass[pwdlen];
    char *ptr = pass;
    char pass2[pwdlen];
    char *ptr2 = pass2;

    my_getpass("Password: ", &ptr, &pwdlen, stdin);
    my_getpass("Password again: ", &ptr2, &pwdlen, stdin);

    if(!encrypt_file(pass, path))
    {
        fprintf(stderr, "Encryption of %s failed.\n", path);
        return false;
    }

    return true;
}

/* Interactively adds a new entry to the database */
bool add_new_entry(int auto_encrypt)
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return false;
    }

    char title[1024] = {0};
    char user[1024] = {0};
    char url[1024] = {0};
    char notes[1024] = {0};
    size_t pwdlen = 1024;
    char pass[pwdlen];
    char *ptr = pass;

    fprintf(stdout, "Title: ");
    fgets(title, 1024, stdin);
    fprintf(stdout, "Username: ");
    fgets(user, 1024, stdin);
    fprintf(stdout, "Url: ");
    fgets(url, 1024, stdin);
    fprintf(stdout, "Notes: ");
    fgets(notes, 1024, stdin);

    my_getpass("Password (empty to generate new): ", &ptr, &pwdlen, stdin);

    if(strcmp(pass, "") == 0)
        generate_new_password(pass);

    strip_newline_str(title);
    strip_newline_str(user);
    strip_newline_str(url);
    strip_newline_str(notes);

    Entry_t *entry = entry_new(title, user, url, pass,
                               notes);

    if(!entry)
        return false;

    if(!db_insert_entry(entry))
    {
        fprintf(stderr, "Failed to add a new entry.\n");
        return false;
    }

    entry_free(entry);

    if(auto_encrypt == 1)
        auto_enc();

    return true;
}

bool edit_entry(int id, int auto_encrypt)
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return false;
    }

    Entry_t *entry = db_get_entry_by_id(id);

    if(!entry)
        return false;

    if(entry->id == -1)
    {
        printf("Nothing found.\n");
        free(entry);
        return false;
    }

    char title[1024] = {0};
    char user[1024] = {0};
    char url[1024] = {0};
    char notes[1024] = {0};
    size_t pwdlen = 1024;
    char pass[pwdlen];
    char *ptr = pass;
    bool update = false;

    fprintf(stdout, "Current title %s\n", entry->title);
    fprintf(stdout, "New title: ");
    fgets(title, 1024, stdin);
    fprintf(stdout, "Current username %s\n", entry->user);
    fprintf(stdout, "New username: ");
    fgets(user, 1024, stdin);
    fprintf(stdout, "Current url %s\n", entry->url);
    fprintf(stdout, "New url: ");
    fgets(url, 1024, stdin);
    fprintf(stdout, "Current notes %s\n", entry->notes);
    fprintf(stdout, "New note: ");
    fgets(notes, 1024, stdin);
    fprintf(stdout, "Current password %s\n", entry->password);
    my_getpass("New password (empty to generate new): ", &ptr, &pwdlen,
            stdin);

    if(strcmp(pass, "") == 0)
        generate_new_password(pass);

    strip_newline_str(title);
    strip_newline_str(user);
    strip_newline_str(url);
    strip_newline_str(notes);

    if(title[0] != '\0')
    {
        entry->title = strdup(title);
        update = true;
    }
    if(user[0] != '\0')
    {
        entry->user = strdup(user);
        update = true;
    }
    if(url[0] != '\0')
    {
        entry->url = strdup(url);
        update = true;
    }
    if(notes[0] != '\0')
    {
        entry->notes = strdup(notes);
        update = true;
    }
    if(pass[0] != '\0')
    {
        entry->password = strdup(pass);
        update = true;
    }

    if(update)
        db_update_entry(entry->id, entry);

    entry_free(entry);

    if(auto_encrypt == 1)
        auto_enc();

    return true;
}

bool remove_entry(int id, int auto_encrypt)
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return false;
    }

    bool changes = false;

    if(db_delete_entry(id, &changes))
    {
        if(changes == true)
            fprintf(stdout, "Entry was deleted from the database.\n");
        else
            fprintf(stdout, "No entry with id %d was found.\n", id);

        return true;
    }

    if(auto_encrypt == 1)
        auto_enc();

    return false;
}

void list_by_id(int id, int show_password, int auto_encrypt)
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return;
    }

    Entry_t *entry = db_get_entry_by_id(id);

    if(!entry)
        return;

    if(entry->id == -1)
    {
        printf("Nothing found with id %d.\n", id);
        free(entry);
        return;
    }

    print_entry(entry, show_password);
    entry_free(entry);

    if(auto_encrypt == 1)
        auto_enc();
}

/* Loop through all entries in the database and output them to stdout.*/
void list_all(int show_password, int auto_encrypt)
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return;
    }

    Entry_t *entry = db_get_list();

    if(!entry)
        return;

    /* Because of how sqlite callbacks work, we need to initialize the list with dummy data.
     * Skip the dummy data to the next entry in the list
     */
    Entry_t *head = entry->next;

    while(head != NULL)
    {
        print_entry(head, show_password);
        head = head->next;
    }

    if(auto_encrypt == 1)
        auto_enc();

    entry_free(entry);
}

/* Uses sqlite "like" query and prints results to stdout.
 * This is ok for the command line version of Titan. However
 * better design is needed _if_ GUI version will be developed.
 */
void find(const char *search, int show_password, int auto_encrypt)
{
    if(!has_active_database())
    {
        fprintf(stderr, "No decrypted database found.\n");
        return;
    }

    Entry_t *list = db_find(search);

    if(!list)
        return;

    Entry_t *head = list->next;

    while(head != NULL)
    {
        print_entry(head, show_password);
        head = head->next;
    }

    if(auto_encrypt == 1)
        auto_enc();

    entry_free(list);
}

void find_regex(const char *regex, int show_password)
{
    Entry_t *list = db_get_list();
    Entry_t *head = list->next;

    regex_find(head, regex, show_password);

    entry_free(list);
}

void show_current_db_path()
{
    char *path = NULL;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "No decrypted database exist.\n");
    }
    else
    {
        fprintf(stdout, "%s\n", path);
        free(path);
    }
}

void set_use_db(const char *path)
{
    if(has_active_database())
    {
        fprintf(stdout,
            "Type password to encrypt existing active database.\n");

        if(!encrypt_database())
            return;
    }

    if(is_file_encrypted(path))
    {
        fprintf(stdout, "Decrypt %s.\n", path);

        if(!decrypt_database(path))
            return;
    }

    write_active_database_path(path);
}
