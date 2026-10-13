#include "myls.h"
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* BLOCKSIZE is a positive integer, optionally followed by K, M or G. */
static uintmax_t block_unit(void)
{
    const char *value = getenv("BLOCKSIZE");
    if (!value || !*value) return 512;
    if (*value == '-') return 512;
    char *end;
    errno = 0;
    unsigned long long number = strtoull(value, &end, 10);
    if (errno || end == value || number == 0) return 512;
    uintmax_t multiplier = 1;
    if (*end) {
        if (end[1]) return 512;
        switch (*end) {
        case 'k': case 'K': multiplier = 1024; break;
        case 'm': case 'M': multiplier = 1024 * 1024; break;
        case 'g': case 'G': multiplier = 1024ULL * 1024 * 1024; break;
        default: return 512;
        }
    }
    if (number > UINTMAX_MAX / multiplier) return 512;
    return (uintmax_t)number * multiplier;
}

int parse_options(int argc, char **argv, Options *o)
{
    *o = (Options){ .almost_all = geteuid() == 0,
        .quote = isatty(STDOUT_FILENO), .block_unit = block_unit() };
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "+AacdFfhiklnqRrSstuw")) != -1) {
        switch (c) {
        case 'A': o->almost_all = true; break;
        case 'a': o->all = true; break;
        case 'c': o->time_kind = TIME_CHANGED; break;
        case 'd': o->directory = true; o->recursive = false; break;
        case 'F': o->classify = true; break;
        case 'f': o->sort_kind = SORT_NONE; break;
        case 'h': o->size_kind = SIZE_HUMAN; break;
        case 'i': o->inode = true; break;
        case 'k': o->size_kind = SIZE_KILO; break;
        case 'l': o->long_form = true; o->numeric = false; break;
        case 'n': o->long_form = true; o->numeric = true; break;
        case 'q': o->quote = true; break;
        case 'R': o->recursive = true; o->directory = false; break;
        case 'r': o->reverse = true; break;
        case 'S': o->sort_kind = SORT_SIZE; break;
        case 's': o->blocks = true; break;
        case 't': o->sort_kind = SORT_TIME; break;
        case 'u': o->time_kind = TIME_ACCESSED; break;
        case 'w': o->quote = false; break;
        default:
            fprintf(stderr, "myls: invalid option -- '%c'\n"
                    "usage: myls [-AacdFfhiklnqRrSstuw] [file ...]\n", optopt);
            return -1;
        }
    }
    return optind;
}
