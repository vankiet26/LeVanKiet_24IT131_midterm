#include "myls.h"
#include <stdlib.h>
#include <string.h>

/* qsort has no portable context argument; this single-threaded program uses
 * a read-only context for the duration of each sorting operation. */
static const Options *sorting;

static struct timespec entry_time(const Entry *e)
{
    if (sorting->time_kind == TIME_ACCESSED) return e->info.st_atim;
    if (sorting->time_kind == TIME_CHANGED) return e->info.st_ctim;
    return e->info.st_mtim;
}

static int compare(const void *left, const void *right)
{
    const Entry *a = left, *b = right;
    int result = 0;
    if (sorting->sort_kind == SORT_SIZE) {
        result = (a->info.st_size < b->info.st_size) -
                 (a->info.st_size > b->info.st_size);
    } else if (sorting->sort_kind == SORT_TIME) {
        struct timespec ta = entry_time(a), tb = entry_time(b);
        result = (ta.tv_sec < tb.tv_sec) - (ta.tv_sec > tb.tv_sec);
        if (!result) result = (ta.tv_nsec < tb.tv_nsec) - (ta.tv_nsec > tb.tv_nsec);
    }
    if (!result) {
        int names = strcmp(a->name, b->name);
        result = (names > 0) - (names < 0);
    }
    return sorting->reverse ? -result : result;
}

void sort_entries(Entries *entries, const Options *options)
{
    if (options->sort_kind == SORT_NONE || entries->size < 2) return;
    sorting = options;
    qsort(entries->items, entries->size, sizeof(Entry), compare);
}
