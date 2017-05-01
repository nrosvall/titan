/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include "cmd_ui.h"
#include "entry.h"
#include "db.h"
#include "utils.h"
#include "pwd-gen.h"
#include "crypto.h"

static int show_password = 0;
static int force = 0;
static int auto_encrypt = 0;

static double v = 1.1;

static void version()
{
    printf("Titan version %.1f\n", v);
}

static void usage()
{
#define HELP "\
SYNOPSIS\n\
\n\
    titan [flags] [options]\n\
\n\
OPTIONS\n\
\n\
    -i --init         <path>         Initialize new database\n\
    -e --encrypt                     Encrypt current database\n\
    -d --decrypt      <path>         Decrypt database\n\
    -a --add                         Add new entry\n\
    -s --show-db-path                Show current database path\n\
    -u --use-db       <path>         Switch using another database\n\
    -r --remove       <id>           Remove entry pointed by id\n\
    -f --find         <search>       Search entries\n\
    -F --regex        <search>       Search entries with regular expressions\n\
    -c --edit         <id>           Edit entry pointed by id\n\
    -l --list-entry   <id>           List entry pointed by id\n\
    -A --list-all                    List all entries\n\
    -h --help                        Show short help and exit. This page\n\
    -g --gen-password <length>       Generate password\n\
    -q --quick        <search>       This is the same as running\n\
                                     --auto-encrypt --show-passwords -f\n\
\n\
    -V --version                     Show version number of program\n\
\n\
FLAGS\n\
\n\
    --auto-encrypt                   Automatically encrypt after exit\n\
    --show-passwords                 Show passwords in listings\n\
    --force                          Ignore everything and force operation\n\
                                     --force only works with --init option\n\
\n\
For more information and examples see man titan(1).\n\
\n\
AUTHORS\n\
    Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>\n\
"
    printf(HELP);
}

//TODO: Implement colors for listing, maybe bold font etc.
//Use environment variable TITAN_COLOR to set the color
//https://stackoverflow.com/questions/3585846/color-text-in-terminal-applications-in-unix
int main(int argc, char *argv[])
{
    int c;

    if(argc == 1)
    {
        usage();
        return 0;
    }

    while(true)
    {
        static struct option long_options[] =
        {
            {"init",                  required_argument, 0, 'i'},
            {"decrypt",               required_argument, 0, 'd'},
            {"encrypt",               no_argument,       0, 'e'},
            {"add",                   no_argument,       0, 'a'},
            {"remove",                required_argument, 0, 'r'},
            {"find",                  required_argument, 0, 'f'},
            {"regex",                 required_argument, 0, 'F'},
            {"edit",                  required_argument, 0, 'c'},
            {"list-entry",            required_argument, 0, 'l'},
            {"use-db",                required_argument, 0, 'u'},
            {"list-all",              no_argument,       0, 'A'},
            {"help",                  no_argument,       0, 'h'},
            {"version",               no_argument,       0, 'V'},
            {"show-db-path",          no_argument,       0, 's'},
            {"gen-password",          required_argument, 0, 'g'},
            {"quick",                 required_argument, 0, 'q'},
            {"auto-encrypt",          no_argument,       &auto_encrypt,  1},
            {"show-passwords",        no_argument,       &show_password, 1},
            {"force",                 no_argument,       &force, 1},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        c = getopt_long(argc, argv, "i:d:ear:f:F:c:l:Asu:hVg:q:", long_options, &option_index);

        if(c == -1)
            break;

        switch(c)
        {
        case 0:
            /* Handle flags here automatically */
            break;
        case 'i':
            init_database(optarg, force, auto_encrypt);
            break;
        case 'd': //decrypt
            decrypt_database(optarg);
            break;
        case 'e': //encrypt
            encrypt_database();
            break;
        case 'a':
            add_new_entry(auto_encrypt);
            break;
        case 's':
            show_current_db_path();
            break;
        case 'h':
            usage();
            break;
        case 'r':
            remove_entry(atoi(optarg), auto_encrypt);
            break;
        case 'u':
            set_use_db(optarg);
        case 'f':
            find(optarg, show_password, auto_encrypt);
            break;
        case 'F':
            find_regex(optarg, show_password);
            break;
        case 'c':
            edit_entry(atoi(optarg), auto_encrypt);
            break;
        case 'l':
            list_by_id(atoi(optarg), show_password, auto_encrypt);
            break;
        case 'A':
            list_all(show_password, auto_encrypt);
            break;
        case 'V':
            version();
            break;
        case 'g':
            generate_password(atoi(optarg));
            break;
        case 'q':
            auto_encrypt = 1;
            show_password = 1;
            find(optarg, show_password, auto_encrypt);
            break;
        case '?':
            usage();
            break;
        }
    }

    return 0;
}
