#include "common.h"

int is_dir_empty(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
        return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return 1;
}

int mkpath(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    if (!path || strlen(path) >= PATH_MAX)
        return -1;

    strcpy(tmp, path);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

int files_differ(const char *file1, const char *file2)
{
    struct stat st1, st2;

    if (lstat(file1, &st1) < 0)
        return 1;
    if (lstat(file2, &st2) < 0)
        return 1;

    if (st1.st_mtime != st2.st_mtime)
        return 1;

    return 0;
}

int remove_recursive(const char *path)
{
    struct stat st;
    if (lstat(path, &st) < 0)
        return -1;

    if (S_ISDIR(st.st_mode))
    {
        DIR *dir = opendir(path);
        if (!dir)
            return -1;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            remove_recursive(full_path);
        }

        closedir(dir);
        rmdir(path);
    }
    else
    {
        unlink(path);
    }

    return 0;
}

void remove_extra_files(const char *source_dir, const char *backup_dir)
{
    DIR *dir = opendir(source_dir);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char source_path[PATH_MAX];
        char backup_path[PATH_MAX];
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name);
        snprintf(backup_path, sizeof(backup_path), "%s/%s", backup_dir, entry->d_name);

        struct stat st;
        if (lstat(source_path, &st) < 0)
            continue;

        struct stat backup_st;
        if (lstat(backup_path, &backup_st) < 0)
        {
            printf("Removing '%s' (not in backup)\n", source_path);
            if (S_ISDIR(st.st_mode))
            {
                remove_recursive(source_path);
            }
            else
            {
                unlink(source_path);
            }
        }
        else if (S_ISDIR(st.st_mode))
        {
            remove_extra_files(source_path, backup_path);
        }
    }

    closedir(dir);
}

int copy_symlink(const char *src, const char *dest, const char *source_base, const char *target_base)
{
    char link_target[PATH_MAX];
    ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
    if (len < 0)
        return -1;
    link_target[len] = '\0';

    if (link_target[0] != '/')
        return symlink(link_target, dest);

    char real_source_base[PATH_MAX];
    if (realpath(source_base, real_source_base) == NULL)
        return -1;

    size_t base_len = strlen(real_source_base);
    if (strncmp(link_target, real_source_base, base_len) != 0)
        return symlink(link_target, dest);

    const char *relative_part = link_target + base_len;
    while (*relative_part == '/')
        relative_part++;

    char real_target_base[PATH_MAX];
    if (realpath(target_base, real_target_base) == NULL)
        return -1;

    char new_target[PATH_MAX * 2];
    snprintf(new_target, sizeof(new_target), "%s/%s", real_target_base, relative_part);
    return symlink(new_target, dest);
}

int copy_regular_file(const char *src, const char *dest, mode_t mode)
{
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
        return -1;

    int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dest_fd < 0)
    {
        close(src_fd);
        return -1;
    }

    char buffer[8192];
    ssize_t bytes;
    int result = 0;

    while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0)
    {
        if (write(dest_fd, buffer, bytes) != bytes)
        {
            result = -1;
            break;
        }
    }

    if (bytes < 0)
        result = -1;

    close(src_fd);
    close(dest_fd);
    return result;
}

int copy_file(const char *src, const char *dest, const char *source_base, const char *target_base)
{
    struct stat st;
    if (lstat(src, &st) < 0)
        return -1;

    if (S_ISLNK(st.st_mode))
        return copy_symlink(src, dest, source_base, target_base);

    return copy_regular_file(src, dest, st.st_mode);
}

int restore_recursive(const char *backup_path, const char *source_path, const char *backup_base,
                      const char *source_base)
{
    struct stat st;
    if (lstat(backup_path, &st) < 0)
        return -1;

    if (S_ISDIR(st.st_mode))
    {
        mkdir(source_path, st.st_mode);

        DIR *dir = opendir(backup_path);
        if (!dir)
            return -1;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char backup_sub[PATH_MAX];
            char source_sub[PATH_MAX];
            snprintf(backup_sub, sizeof(backup_sub), "%s/%s", backup_path, entry->d_name);
            snprintf(source_sub, sizeof(source_sub), "%s/%s", source_path, entry->d_name);

            restore_recursive(backup_sub, source_sub, backup_base, source_base);
        }

        closedir(dir);
    }
    else
    {
        if (files_differ(backup_path, source_path))
        {
            printf("Restoring '%s'\n", source_path);
            unlink(source_path);
            return copy_file(backup_path, source_path, backup_base, source_base);
        }
    }

    return 0;
}

int copy_recursive(const char *src, const char *dest, const char *source_base, const char *target_base)
{
    struct stat st;
    if (lstat(src, &st) < 0)
        return -1;

    if (S_ISDIR(st.st_mode))
    {
        if (mkdir(dest, st.st_mode) < 0 && errno != EEXIST)
            return -1;

        DIR *dir = opendir(src);
        if (!dir)
            return -1;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

            if (copy_recursive(src_path, dest_path, source_base, target_base) < 0)
            {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);
    }
    else
    {
        return copy_file(src, dest, source_base, target_base);
    }

    return 0;
}

char *get_relative_path(const char *base, const char *path)
{
    size_t base_len = strlen(base);
    if (strncmp(base, path, base_len) == 0)
    {
        const char *rel = path + base_len;
        while (*rel == '/')
            rel++;
        return strdup(rel[0] ? rel : ".");
    }
    return strdup(path);
}

int is_path_inside(const char *parent, const char *child)
{
    char real_parent[PATH_MAX];
    char real_child[PATH_MAX];

    if (realpath(parent, real_parent) == NULL)
        return 0;

    if (realpath(child, real_child) == NULL)
    {
        strncpy(real_child, child, PATH_MAX - 1);
        real_child[PATH_MAX - 1] = '\0';
    }

    size_t parent_len = strlen(real_parent);

    if (strncmp(real_parent, real_child, parent_len) == 0 && real_child[parent_len] == '/')
        return 1;

    return 0;
}
