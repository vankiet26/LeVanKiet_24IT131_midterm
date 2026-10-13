#include "myls.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void list_directory(const char *path, const Options *o,
                    const Ancestor *parent, bool heading, bool *printed)
{
    struct stat current;
    if (stat(path, &current) != 0) { report_error(path); return; }
    for (const Ancestor *a = parent; a; a = a->parent) {
        if (a->device == current.st_dev && a->inode == current.st_ino) {
            fprintf(stderr, "myls: %s: directory cycle detected\n", path);
            exit_status = 1;
            return;
        }
    }
    Ancestor ancestor = { current.st_dev, current.st_ino, parent };
    DIR *directory = opendir(path);
    if (!directory) { report_error(path); return; }
    Entries entries = {0};
    for (;;) {
        errno = 0;
        struct dirent *item = readdir(directory);
        if (!item) {
            if (errno) report_error(path);
            break;
        }
        const char *name = item->d_name;
        bool dot = !strcmp(name, ".") || !strcmp(name, "..");
        if (!o->all && (dot || (name[0] == '.' && !o->almost_all))) continue;
        Entry entry = { .name = copy_string(name), .path = join_path(path, name) };
        if (lstat(entry.path, &entry.info) != 0) {
            report_error(entry.path);
            free(entry.name); free(entry.path);
            continue;
        }
        add_entry(&entries, entry);
    }
    if (closedir(directory) != 0) report_error(path);
    sort_entries(&entries, o);
    if (heading) {
        if (*printed) putchar('\n');
        print_name(path, o);
        puts(":");
    }
    print_entries(&entries, o, true);
    *printed = true;
    if (o->recursive) {
        for (size_t i = 0; i < entries.size; ++i) {
            Entry *e = &entries.items[i];
            /* Never traverse . / .. or symlinks discovered inside a directory. */
            if (S_ISDIR(e->info.st_mode) && strcmp(e->name, ".") && strcmp(e->name, ".."))
                list_directory(e->path, o, &ancestor, true, printed);
        }
    }
    free_entries(&entries);
}
