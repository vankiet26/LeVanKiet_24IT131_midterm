#include "myls.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int exit_status = 0;

void report_error(const char *path)
{
    fprintf(stderr, "myls: %s: %s\n", path, strerror(errno));
    exit_status = 1;
}

void *checked_realloc(void *ptr, size_t size)
{
    void *result = realloc(ptr, size);
    if (!result && size) {
        fprintf(stderr, "myls: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

char *copy_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *result = checked_realloc(NULL, length);
    memcpy(result, text, length);
    return result;
}

char *join_path(const char *directory, const char *name)
{
    size_t a = strlen(directory), b = strlen(name);
    bool slash = a && directory[a - 1] != '/';
    if (a > SIZE_MAX - b - 2) {
        fprintf(stderr, "myls: path too long\n");
        exit(EXIT_FAILURE);
    }
    char *result = checked_realloc(NULL, a + b + 2);
    memcpy(result, directory, a);
    if (slash) result[a++] = '/';
    memcpy(result + a, name, b + 1);
    return result;
}

void add_entry(Entries *entries, Entry entry)
{
    if (entries->size == entries->capacity) {
        size_t next = entries->capacity ? entries->capacity * 2 : 32;
        if (next < entries->capacity || next > SIZE_MAX / sizeof(Entry)) {
            fprintf(stderr, "myls: too many entries\n");
            exit(EXIT_FAILURE);
        }
        entries->items = checked_realloc(entries->items, next * sizeof(Entry));
        entries->capacity = next;
    }
    entries->items[entries->size++] = entry;
}

void free_entries(Entries *entries)
{
    for (size_t i = 0; i < entries->size; ++i) {
        free(entries->items[i].name);
        free(entries->items[i].path);
    }
    free(entries->items);
    *entries = (Entries){0};
}
