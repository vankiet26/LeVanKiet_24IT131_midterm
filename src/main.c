#include "myls.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    /* Decode multibyte filenames, while keeping bytewise lexical sorting. */
    setlocale(LC_CTYPE, "");
    Options options;
    int first = parse_options(argc, argv, &options);
    if (first < 0) return EXIT_FAILURE;

    Entries files = {0}, directories = {0};
    int count = first == argc ? 1 : argc - first;
    for (int i = 0; i < count; ++i) {
        const char *path = first == argc ? "." : argv[first + i];
        Entry entry = { .name = copy_string(path), .path = copy_string(path) };
        if (lstat(path, &entry.info) != 0) {
            report_error(path);
            free(entry.name); free(entry.path);
            continue;
        }
        /* BSD-style default: follow an operand symlink to a directory only
         * when neither -d, a long format, nor -F requests the link itself. */
        if (S_ISLNK(entry.info.st_mode) && !options.directory &&
            !options.long_form && !options.classify) {
            struct stat target;
            if (stat(path, &target) == 0 && S_ISDIR(target.st_mode)) entry.info = target;
        }
        if (S_ISDIR(entry.info.st_mode) && !options.directory)
            add_entry(&directories, entry);
        else add_entry(&files, entry);
    }
    sort_entries(&files, &options);
    sort_entries(&directories, &options);
    print_entries(&files, &options, false);
    bool printed = files.size > 0;
    for (size_t i = 0; i < directories.size; ++i)
        list_directory(directories.items[i].path, &options, NULL,
                       count > 1 || options.recursive, &printed);
    free_entries(&files);
    free_entries(&directories);
    if (fflush(stdout) == EOF || ferror(stdout)) {
        report_error("stdout");
    }
    return exit_status;
}
