#include "myls.h"
#include <ctype.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

void print_name(const char *name, const Options *o)
{
    if (!o->quote) { fputs(name, stdout); return; }
    mbstate_t state = {0};
    size_t remaining = strlen(name);
    while (remaining) {
        wchar_t wc;
        size_t used = mbrtowc(&wc, name, remaining, &state);
        if (used == (size_t)-1 || used == (size_t)-2) {
            putchar('?'); ++name; --remaining;
            memset(&state, 0, sizeof(state));
        } else {
            if (iswprint(wc)) fwrite(name, 1, used, stdout);
            else putchar('?');
            name += used; remaining -= used;
        }
    }
}

static char file_type(mode_t mode)
{
    if (S_ISDIR(mode)) return 'd';
    if (S_ISLNK(mode)) return 'l';
    if (S_ISCHR(mode)) return 'c';
    if (S_ISBLK(mode)) return 'b';
    if (S_ISFIFO(mode)) return 'p';
    if (S_ISSOCK(mode)) return 's';
#ifdef S_ISWHT
    if (S_ISWHT(mode)) return 'w';
#endif
    return '-';
}

static void mode_string(mode_t mode, char text[11])
{
    text[0] = file_type(mode);
    const mode_t bits[9] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
                           S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    for (int i = 0; i < 9; ++i) text[i + 1] = mode & bits[i] ? "rwx"[i % 3] : '-';
    if (mode & S_ISUID) text[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) text[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) text[9] = mode & S_IXOTH ? 't' : 'T';
    text[10] = '\0';
}

static void human_size(uintmax_t bytes, char *buffer, size_t length)
{
    const char *units = "BKMGTPE";
    unsigned unit = 0;
    long double number = bytes;
    while (number >= 1024 && unit < 6) { number /= 1024; ++unit; }
    if (!unit || number >= 10) snprintf(buffer, length, "%.0Lf%c", number, units[unit]);
    else snprintf(buffer, length, "%.1Lf%c", number, units[unit]);
}

static uintmax_t allocated_bytes(const struct stat *info)
{
    uintmax_t blocks = info->st_blocks > 0 ? (uintmax_t)info->st_blocks : 0;
    return blocks > UINTMAX_MAX / 512 ? UINTMAX_MAX : blocks * 512;
}

static void block_text(uintmax_t bytes, const Options *o, char *buffer, size_t length)
{
    if (o->size_kind == SIZE_HUMAN) human_size(bytes, buffer, length);
    else {
        uintmax_t unit = o->size_kind == SIZE_KILO ? 1024 : o->block_unit;
        snprintf(buffer, length, "%ju", bytes / unit + (bytes % unit != 0));
    }
}

static void long_prefix(const Entry *e, const Options *o)
{
    const struct stat *s = &e->info;
    char mode[11], owner[256], group[256], size[64], date[64];
    mode_string(s->st_mode, mode);
    struct passwd *pw = o->numeric ? NULL : getpwuid(s->st_uid);
    if (pw) snprintf(owner, sizeof(owner), "%s", pw->pw_name);
    else snprintf(owner, sizeof(owner), "%ju", (uintmax_t)s->st_uid);
    struct group *gr = o->numeric ? NULL : getgrgid(s->st_gid);
    if (gr) snprintf(group, sizeof(group), "%s", gr->gr_name);
    else snprintf(group, sizeof(group), "%ju", (uintmax_t)s->st_gid);

    if (S_ISCHR(s->st_mode) || S_ISBLK(s->st_mode))
        snprintf(size, sizeof(size), "%u, %u", major(s->st_rdev), minor(s->st_rdev));
    else if (o->size_kind == SIZE_HUMAN && s->st_size >= 0)
        human_size((uintmax_t)s->st_size, size, sizeof(size));
    else snprintf(size, sizeof(size), "%jd", (intmax_t)s->st_size);

    time_t value = o->time_kind == TIME_CHANGED ? s->st_ctime :
                   o->time_kind == TIME_ACCESSED ? s->st_atime : s->st_mtime;
    struct tm local;
    time_t now = time(NULL);
    const char *format = difftime(now, value) > 15552000 || difftime(value, now) > 3600
                       ? "%b %e  %Y" : "%b %e %H:%M";
    if (!localtime_r(&value, &local) || !strftime(date, sizeof(date), format, &local))
        strcpy(date, "??? ?? ??:??");
    printf("%s %ju %s %s %s %s ", mode, (uintmax_t)s->st_nlink, owner, group, size, date);
}

static void suffix(mode_t mode)
{
    if (S_ISDIR(mode)) putchar('/');
    else if (S_ISLNK(mode)) putchar('@');
    else if (S_ISSOCK(mode)) putchar('=');
    else if (S_ISFIFO(mode)) putchar('|');
#ifdef S_ISWHT
    else if (S_ISWHT(mode)) putchar('%');
#endif
    else if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) putchar('*');
}

static void link_target(const Entry *e, const Options *o)
{
    /* readlink does not append NUL. Retry dynamically if the target grew. */
    size_t capacity = 256;
    for (;;) {
        char *buffer = checked_realloc(NULL, capacity + 1);
        ssize_t used = readlink(e->path, buffer, capacity);
        if (used < 0) { report_error(e->path); free(buffer); return; }
        if ((size_t)used < capacity) {
            buffer[used] = '\0';
            fputs(" -> ", stdout); print_name(buffer, o);
            free(buffer); return;
        }
        free(buffer);
        if (capacity > SIZE_MAX / 2 - 1) {
            fprintf(stderr, "myls: symlink target too long\n"); exit_status = 1; return;
        }
        capacity *= 2;
    }
}

void print_entries(const Entries *entries, const Options *o, bool directory)
{
    if (directory && (o->long_form || (o->blocks && isatty(STDOUT_FILENO)))) {
        uintmax_t total = 0;
        for (size_t i = 0; i < entries->size; ++i) {
            uintmax_t bytes = allocated_bytes(&entries->items[i].info);
            total = total > UINTMAX_MAX - bytes ? UINTMAX_MAX : total + bytes;
        }
        char buffer[64]; block_text(total, o, buffer, sizeof(buffer));
        printf("total %s\n", buffer);
    }
    for (size_t i = 0; i < entries->size; ++i) {
        const Entry *e = &entries->items[i];
        if (o->inode) printf("%ju ", (uintmax_t)e->info.st_ino);
        if (o->blocks) {
            char buffer[64]; block_text(allocated_bytes(&e->info), o, buffer, sizeof(buffer));
            printf("%s ", buffer);
        }
        if (o->long_form) long_prefix(e, o);
        print_name(e->name, o);
        if (o->classify) suffix(e->info.st_mode);
        if (o->long_form && S_ISLNK(e->info.st_mode)) link_target(e, o);
        putchar('\n');
    }
}
