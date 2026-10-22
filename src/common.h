#ifndef WATCHER_COMMON_H
#define WATCHER_COMMON_H

#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_WATCHES 8192
#define MAX_TOKENS 100
#define MAX_LINE 4096
#define MAX_BACKUPS 100
#define MAX_TARGETS 100

#define ERR(source) \\
    (fprintf(stderr, "%s:%d\\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

extern volatile sig_atomic_t should_exit;
extern volatile sig_atomic_t last_signal;

struct Watch
{
    int wd;
    char *path;
};

struct WatchMap
{
    struct Watch watch_map[MAX_WATCHES];
    int watch_count;
};

struct TargetProcess
{
    char *target_path;
    pid_t pid;
};

struct Backup
{
    char *source_path;
    struct TargetProcess *targets;
    int target_count;
    int notify_fd;
    struct WatchMap watch_map;
};

struct ParsedLine
{
    char chars[MAX_TOKENS][PATH_MAX];
    int count;
};

void sethandler(void (*f)(int), int sigNo);
void sig_handler(int sig);
void setup_signals(void);

void parse_line(const char *line, struct ParsedLine *result);

void add_to_map(struct WatchMap *map, int wd, const char *path);
struct Watch *find_watch(struct WatchMap *map, int wd);
void remove_from_map(struct WatchMap *map, int wd);
void add_watch_recursive(int notify_fd, struct WatchMap *map, const char *base_path);
void update_watch_paths(struct WatchMap *map, const char *old_path, const char *new_path);
void remove_watches_under_path(int notify_fd, struct WatchMap *map, const char *base_path);

int is_dir_empty(const char *path);
int mkpath(const char *path, mode_t mode);
int files_differ(const char *file1, const char *file2);
int remove_recursive(const char *path);
void remove_extra_files(const char *source_dir, const char *backup_dir);
int copy_symlink(const char *src, const char *dest, const char *source_base, const char *target_base);
int copy_regular_file(const char *src, const char *dest, mode_t mode);
int copy_file(const char *src, const char *dest, const char *source_base, const char *target_base);
int restore_recursive(const char *backup_path, const char *source_path, const char *backup_base,
                      const char *source_base);
int copy_recursive(const char *src, const char *dest, const char *source_base, const char *target_base);
char *get_relative_path(const char *base, const char *path);
int is_path_inside(const char *parent, const char *child);

void monitor_process(const char *source_path, const char *target_path);

void cleanup_backups(struct Backup *backups, int backup_count);
void handle_list_command(struct Backup *backups, int backup_count);
void handle_restore_command(struct ParsedLine *parsed);
int handle_end_command(struct Backup *backups, int *backup_count, struct ParsedLine *parsed);
int handle_add_command(struct Backup *backups, int *backup_count, struct ParsedLine *parsed);

#endif
