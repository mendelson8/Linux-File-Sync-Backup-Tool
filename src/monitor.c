#include "common.h"

void remove_watches_under_path(int notify_fd, struct WatchMap *map, const char *base_path)
{
    size_t base_len = strlen(base_path);

    for (int i = map->watch_count - 1; i >= 0; i--)
    {
        if (strncmp(map->watch_map[i].path, base_path, base_len) == 0 &&
            (map->watch_map[i].path[base_len] == '/' || map->watch_map[i].path[base_len] == '\0'))
        {
            // printf("removing watch: '%s' @wd=%d\n", map->watch_map[i].path, map->watch_map[i].wd);
            inotify_rm_watch(notify_fd, map->watch_map[i].wd);
            free(map->watch_map[i].path);
            map->watch_map[i] = map->watch_map[map->watch_count - 1];
            map->watch_count--;
        }
    }
}

void monitor_process(const char *source_path, const char *target_path)
{
    sethandler(SIG_IGN, SIGINT);
    sethandler(sig_handler, SIGTERM);

    int my_notify_fd = inotify_init();
    if (my_notify_fd < 0)
    {
        perror("inotify_init in child");
        exit(1);
    }

    struct WatchMap my_watch_map;
    memset(&my_watch_map, 0, sizeof(struct WatchMap));

    add_watch_recursive(my_notify_fd, &my_watch_map, source_path);

    while (!should_exit)
    {
        char buffer[64 * (sizeof(struct inotify_event) + NAME_MAX + 1)];
        ssize_t len = read(my_notify_fd, buffer, sizeof(buffer));
        if (len < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        ssize_t i = 0;
        while (i < len)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            struct Watch *watch = find_watch(&my_watch_map, event->wd);

            char event_path[PATH_MAX] = "";
            if (watch && event->len > 0)
            {
                snprintf(event_path, sizeof(event_path), "%s/%s", watch->path, event->name);
            }
            else if (watch)
            {
                strncpy(event_path, watch->path, sizeof(event_path));
            }

            if (event->mask & IN_IGNORED)
            {
                if (watch)
                {
                    char path_copy[PATH_MAX];
                    strncpy(path_copy, watch->path, sizeof(path_copy) - 1);
                    path_copy[sizeof(path_copy) - 1] = '\0';
                    remove_watches_under_path(my_notify_fd, &my_watch_map, path_copy);
                }
                remove_from_map(&my_watch_map, event->wd);
            }
            else if (event->mask & IN_ISDIR)
            {
                char *rel = get_relative_path(source_path, event_path);
                char target_file_path[PATH_MAX];
                snprintf(target_file_path, sizeof(target_file_path), "%s/%s", target_path, rel);

                if (event->mask & IN_CREATE)
                {
                    struct stat st;
                    if (stat(event_path, &st) == 0)
                    {
                        copy_recursive(event_path, target_file_path, source_path, target_path);
                    }
                    add_watch_recursive(my_notify_fd, &my_watch_map, event_path);
                }
                else if (event->mask & IN_DELETE)
                {
                    remove_recursive(target_file_path);
                    remove_watches_under_path(my_notify_fd, &my_watch_map, event_path);
                }
                else if (event->mask & IN_MOVED_FROM)
                {
                    remove_recursive(target_file_path);
                    remove_watches_under_path(my_notify_fd, &my_watch_map, event_path);
                }
                else if (event->mask & IN_MOVED_TO)
                {
                    add_watch_recursive(my_notify_fd, &my_watch_map, event_path);
                    copy_recursive(event_path, target_file_path, source_path, target_path);
                }

                free(rel);
            }
            else
            {
                char *rel = get_relative_path(source_path, event_path);
                char target_file_path[PATH_MAX];
                snprintf(target_file_path, sizeof(target_file_path), "%s/%s", target_path, rel);

                if (event->mask & (IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO))
                {
                    copy_file(event_path, target_file_path, source_path, target_path);
                }
                else if (event->mask & (IN_DELETE | IN_MOVED_FROM))
                {
                    unlink(target_file_path);
                }

                free(rel);
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    close(my_notify_fd);
    for (int j = 0; j < my_watch_map.watch_count; j++)
    {
        free(my_watch_map.watch_map[j].path);
    }

    exit(0);
}
