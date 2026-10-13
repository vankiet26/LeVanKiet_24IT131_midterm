#ifndef MYLS_H
#define MYLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

typedef enum { TIME_MODIFIED, TIME_CHANGED, TIME_ACCESSED } TimeKind;
typedef enum { SIZE_BLOCKS, SIZE_KILO, SIZE_HUMAN } SizeKind;
typedef enum { SORT_NAME, SORT_TIME, SORT_SIZE, SORT_NONE } SortKind;

typedef struct {
    bool all, almost_all, directory, classify, inode, long_form;
    bool numeric, quote, recursive, reverse, blocks;
    TimeKind time_kind;
    SizeKind size_kind;
    SortKind sort_kind;
    uintmax_t block_unit;
} Options;

typedef struct {
    char *name;
    char *path;
    struct stat info;
} Entry;

typedef struct { Entry *items; size_t size, capacity; } Entries;
typedef struct Ancestor {
    dev_t device;
    ino_t inode;
    const struct Ancestor *parent;
} Ancestor;

extern int exit_status;
void report_error(const char *path);
void *checked_realloc(void *ptr, size_t size);
char *copy_string(const char *text);
char *join_path(const char *directory, const char *name);
void add_entry(Entries *entries, Entry entry);
void free_entries(Entries *entries);
int parse_options(int argc, char **argv, Options *options);
void sort_entries(Entries *entries, const Options *options);
void print_name(const char *name, const Options *options);
void print_entries(const Entries *entries, const Options *options, bool directory);
void list_directory(const char *path, const Options *options,
                    const Ancestor *parent, bool heading, bool *printed);

#endif
