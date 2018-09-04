/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sqlite3.h>
#include "entry.h"
#include "db.h"
#include "utils.h"

/* sqlite callbacks */
static int cb_check_integrity(void *notused, int argc, char **argv, char **column_name);
static int cb_get_by_id(void *entry, int argc, char **argv, char **column_name);
static int cb_list_all(void *entry, int argc, char **argv, char **column_name);
static int cb_find(void *entry, int argc, char **argv, char **column_name);

/*Run integrity check for the database to detect
 *malformed and corrupted databases. Returns true
 *if everything is ok, false if something is wrong.
 */
static bool db_check_integrity(const char *path)
{
    sqlite3 *db;
    char *err = NULL;
    int retval;
    char *sql;

    retval = sqlite3_open(path, &db);

    if(retval)
    {
        fprintf(stderr, "Can't initialize: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sql = "pragma integrity_check;";

    retval = sqlite3_exec(db, sql, cb_check_integrity, 0, &err);

    if(retval != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);

    return true;
}

bool db_init_new(const char *path)
{
    sqlite3 *db;
    char *err = NULL;

    int rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to initialize database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);

        return false;
    }

    char *query = "create table entries"
        "(id integer primary key, title text, user text, url text,"
        "password text, notes text,"
        "timestamp date default (datetime('now','localtime')));";

    rc = sqlite3_exec(db, query, 0, 0, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);

        return false;
    }

    sqlite3_close(db);

    return true;
}

bool db_insert_entry(Entry_t *entry)
{
    sqlite3 *db;
    char *err = NULL;
    char *path = NULL;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Error getting database path\n");
        return false;
    }

    if(!db_check_integrity(path))
    {
        fprintf(stderr, "Corrupted database. Abort.\n");
        free(path);
        return 0;
    }

    int rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to initialize database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(path);

        return false;
    }

    char *query = sqlite3_mprintf("insert into entries(title, user, url, password, notes)"
                                  "values('%q','%q','%q','%q','%q')",
                                  entry->title, entry->user, entry->url, entry->password,
                                  entry->notes);

    rc = sqlite3_exec(db, query, NULL, 0, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);
        sqlite3_free(query);
        sqlite3_close(db);
        free(path);

        return false;
    }

    sqlite3_free(query);
    sqlite3_close(db);
    free(path);

    return true;
}

bool db_update_entry(int id, Entry_t *new_entry)
{
    sqlite3 *db;
    char *err = NULL;
    char *path = NULL;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Error getting database path\n");
        return false;
    }

    if(!db_check_integrity(path))
    {
        fprintf(stderr, "Corrupted database. Abort.\n");
        free(path);

        return 0;
    }

    int rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to initialize database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(path);

        return false;
    }

    char *query = sqlite3_mprintf("update entries set title='%q',"
                                  "user='%q',"
                                  "url='%q',"
                                  "password='%q',"
                                  "notes='%q',timestamp=datetime('now','localtime') where id=%d;",
                                  new_entry->title,
                                  new_entry->user,
                                  new_entry->url,
                                  new_entry->password,
                                  new_entry->notes,id);

    rc = sqlite3_exec(db, query, NULL, 0, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);
        sqlite3_free(query);
        sqlite3_close(db);
        free(path);

        return false;
    }

    sqlite3_free(query);
    sqlite3_close(db);
    free(path);

    return true;
}

/*Get entry which has the wanted id.
 * Caller must free the return value.
 */
Entry_t *db_get_entry_by_id(int id)
{
    char *path = NULL;
    sqlite3 *db;
    int rc;
    char *query;
    char *err = NULL;
    Entry_t *entry = NULL;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Error getting database path\n");
        return NULL;
    }

    if(!db_check_integrity(path))
    {
        fprintf(stderr, "Corrupted database. Abort.\n");
        free(path);

        return NULL;
    }

    rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error %s\n", sqlite3_errmsg(db));
        free(path);

        return NULL;
    }

    entry = tmalloc(sizeof(struct _entry));

    query = sqlite3_mprintf("select id,title,user,url,password,notes,"
                            "timestamp from entries where id=%d;", id);

    /* Set id to minus one by default. If query finds data
     * we set the id back to the original one in the callback.
     * We can uses this to easily check if we have valid data in the structure.
     */
    entry->id = -1;

    rc = sqlite3_exec(db, query, cb_get_by_id, entry, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);
        sqlite3_free(query);
        free(path);

        return NULL;
    }

    sqlite3_free(query);
    sqlite3_close(db);
    free(path);

    return entry;
}

/* Returns true on success, false on failure.
 * Parameter changes is set to true if entry with given
 * id was found and deleted.
 */
bool db_delete_entry(int id, bool *changes)
{
    char *path = NULL;
    sqlite3 *db;
    int rc;
    char *query;
    char *err = NULL;
    int count;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Error getting database path\n");
        return false;
    }

    if(!db_check_integrity(path))
    {
        fprintf(stderr, "Corrupted database. Abort.\n");
        free(path);

        return false;
    }

    rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        free(path);
        return false;
    }

    query = sqlite3_mprintf("delete from entries where id=%d;", id);
    rc = sqlite3_exec(db, query, NULL, 0, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);
        sqlite3_free(query);
        sqlite3_close(db);
        free(path);

        return false;
    }

    count = sqlite3_changes(db);

    if(count > 0)
        *changes = true;

    sqlite3_free(query);
    sqlite3_close(db);

    free(path);

    return true;
}

/* Get latest count of entries pointed by count_latest.
 * -1 to get everything.
 */
Entry_t *db_get_list(int count_latest)
{
    char *path = NULL;
    char *err = NULL;
    sqlite3 *db;
    char *query = NULL;

    //TODO: Make a sanity check for the latest count. Should be >=0 or -1.
    if(count_latest < 0 && count_latest != -1)
    {
        fprintf(stderr, "Invalid parameter <count>\n");
        return NULL;
    }

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Error getting database path\n");
        return NULL;
    }

    if(!db_check_integrity(path))
    {
        fprintf(stderr, "Corrupted database. Abort.\n");
        free(path);

        return NULL;
    }

    int rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(path);

        return NULL;
    }

    /* Fill our list with dummy data */
    Entry_t *entry = entry_new("dummy", "dummy", "dummy", "dummy", "dummy");

    /* Get all data or a defined count */
    if(count_latest == -1)
        query = "select * from entries;";
    else
        query = sqlite3_mprintf("select * from entries order by date(column) desc limit %d", count_latest);

    rc = sqlite3_exec(db, query, cb_list_all, entry, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);

        if(count_latest != -1 && query != NULL)
            sqlite3_free(query);

        sqlite3_close(db);
        free(path);

        return NULL;
    }

    sqlite3_close(db);
    free(path);

    return entry;
}

Entry_t *db_find(const char *search)
{
    char *path = NULL;
    char *err = NULL;
    sqlite3 *db;

    path = read_active_database_path();

    if(!path)
    {
        fprintf(stderr, "Error getting database path\n");
        return NULL;
    }

    if(!db_check_integrity(path))
    {
        fprintf(stderr, "Corrupted database. Abort.\n");
        free(path);

        return NULL;
    }

    int rc = sqlite3_open(path, &db);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(path);

        return NULL;
    }

    /* Fill our list with dummy data */
    Entry_t *entry = entry_new("dummy", "dummy", "dummy", "dummy", "dummy");

    /* Search the same search term from each column we're might be interested in. */
    char *query = sqlite3_mprintf("select * from entries where title like '%%%q%%' "
                                  "or user like '%%%q%%' "
                                  "or url like '%%%q%%' "
                                  "or notes like '%%%q%%';", search, search, search, search);

    rc = sqlite3_exec(db, query, cb_find, entry, &err);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error: %s\n", err);
        sqlite3_free(err);
        sqlite3_free(query);
        sqlite3_close(db);
        free(path);

        return NULL;
    }

    sqlite3_free(query);
    sqlite3_close(db);
    free(path);

    return entry;
}

static int cb_check_integrity(void *notused, int argc, char **argv, char **column_name)
{
    for(int i = 0; i < argc; i++)
    {
        if(strcmp(column_name[i], "integrity_check") == 0)
        {
            char *result = argv[i];

            if(strcmp(result, "ok") != 0)
                return 1;
        }
    }

    return 0;
}

static int cb_list_all(void *entry, int argc, char **argv, char **column_name)
{
    Entry_t *one_entry = entry_add(entry, argv[1], argv[2], argv[3], argv[4], argv[5]);
    one_entry->id = atoi(argv[0]);
    one_entry->stamp = strdup(argv[6]);

    return 0;
}

static int cb_find(void *entry, int argc, char **argv, char **column_name)
{
    Entry_t *one_entry = entry_add(entry, argv[1], argv[2], argv[3], argv[4], argv[5]);
    one_entry->id = atoi(argv[0]);
    one_entry->stamp = strdup(argv[6]);

    return 0;
}

static int cb_get_by_id(void *entry, int argc, char **argv, char **column_name)
{
    /*Let's not allow NULLs*/
    if(argv[0] == NULL)
        return 1;
    if(argv[1] == NULL)
        return 1;
    if(argv[2] == NULL)
        return 1;
    if(argv[3] == NULL)
        return 1;
    if(argv[4] == NULL)
        return 1;
    if(argv[5] == NULL)
        return 1;
    if(argv[6] == NULL)
        return 1;

    ((Entry_t *)entry)->id = atoi(argv[0]);
    ((Entry_t *)entry)->title = strdup(argv[1]);
    ((Entry_t *)entry)->user = strdup(argv[2]);
    ((Entry_t *)entry)->url = strdup(argv[3]);
    ((Entry_t *)entry)->password = strdup(argv[4]);
    ((Entry_t *)entry)->notes = strdup(argv[5]);
    ((Entry_t *)entry)->stamp = strdup(argv[6]);
    ((Entry_t *)entry)->next = NULL;

    return 0;
}
