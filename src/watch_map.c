#include "common.h"

void add_to_map(struct WatchMap *map, int wd, const char *path)
{
    if (map->watch_count >= MAX_WATCHES)
    {
        fprintf(stderr, "Exceeded max watches!\n");
        return;
    }
    map->watch_map[map->watch_count].wd = wd;
    map->watch_map[map->watch_count].path = strdup(path);
    map->watch_count++;
    // printf("new watch: '%s' @wd=%d\n", path, wd);
}

struct Watch *find_watch(struct WatchMap *map, int wd)
{
    for (int i = 0; i < map->watch_count; i++)
    {
        if (map->watch_map[i].wd == wd)
        {
            return &map->watch_map[i];
        }
    }
    return NULL;
}

void remove_from_map(struct WatchMap *map, int wd)
{
    for (int i = 0; i < map->watch_count; i++)
    {
        if (map->watch_map[i].wd == wd)
        {
            // printf("removing watch: '%s' @wd=%d\n", map->watch_map[i].path, wd);
            free(map->watch_map[i].path);
            map->watch_map[i] = map->watch_map[map->watch_count - 1];
            map->watch_count--;
            return;
        }
    }
}

void add_watch_recursive(int notify_fd, struct WatchMap *map, const char *base_path)
{
    uint32_t mask = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF;

    int wd = inotify_add_watch(notify_fd, base_path, mask);
    if (wd < 0)
    {
        perror("inotify_add_watch");
        return;
    }
    add_to_map(map, wd, base_path);

    DIR *dir = opendir(base_path);
    if (!dir)
    {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode))
        {
            add_watch_recursive(notify_fd, map, full_path);
        }
    }

    closedir(dir);
}

void update_watch_paths(struct WatchMap *map, const char *old_path, const char *new_path)
{
    size_t old_len = strlen(old_path);

    for (int i = 0; i < map->watch_count; i++)
    {
        if (strncmp(map->watch_map[i].path, old_path, old_len) == 0 &&
            (map->watch_map[i].path[old_len] == '/' || map->watch_map[i].path[old_len] == '\0'))
        {
            char new_full_path[PATH_MAX];
            snprintf(new_full_path, sizeof(new_full_path), "%s%s", new_path, map->watch_map[i].path + old_len);

            free(map->watch_map[i].path);
            map->watch_map[i].path = strdup(new_full_path);
        }
    }
}
