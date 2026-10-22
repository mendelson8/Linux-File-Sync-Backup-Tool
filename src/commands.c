#include "common.h"

void setup_signals(void)
{
    for (int sig = 1; sig < NSIG; sig++)
    {
        if (sig != SIGINT && sig != SIGTERM && sig != SIGKILL && sig != SIGSTOP)
        {
            signal(sig, SIG_IGN);
        }
    }

    sethandler(sig_handler, SIGINT);
    sethandler(sig_handler, SIGTERM);
}

void cleanup_backups(struct Backup *backups, int backup_count)
{
    for (int i = 0; i < backup_count; i++)
    {
        for (int j = 0; j < backups[i].target_count; j++)
        {
            if (backups[i].targets[j].pid > 0)
            {
                kill(backups[i].targets[j].pid, SIGTERM);
                waitpid(backups[i].targets[j].pid, NULL, 0);
            }
            free(backups[i].targets[j].target_path);
        }
        free(backups[i].targets);

        close(backups[i].notify_fd);
        for (int j = 0; j < backups[i].watch_map.watch_count; j++)
        {
            free(backups[i].watch_map.watch_map[j].path);
        }
        free(backups[i].source_path);
    }
}

void handle_list_command(struct Backup *backups, int backup_count)
{
    if (backup_count == 0)
    {
        printf("No active backups\n");
    }
    else
    {
        printf("Active backups:\n");
        for (int i = 0; i < backup_count; i++)
        {
            printf("  Source: %s\n", backups[i].source_path);
            printf("  Targets (%d):\n", backups[i].target_count);
            for (int j = 0; j < backups[i].target_count; j++)
            {
                printf("    - %s (PID: %d)\n", backups[i].targets[j].target_path, backups[i].targets[j].pid);
            }
        }
    }
}

void handle_restore_command(struct ParsedLine *parsed)
{
    if (parsed->count < 3)
    {
        fprintf(stderr, "Usage: restore <source> <target>\n");
        return;
    }

    const char *source = parsed->chars[1];
    const char *target = parsed->chars[2];

    struct stat st;
    if (stat(source, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: Source '%s' must be a directory\n", source);
        return;
    }

    if (stat(target, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: Target '%s' must be a directory (backup)\n", target);
        return;
    }

    printf("Restoring from '%s' to '%s'...\n", target, source);
    printf("Step 1: Removing files not in backup...\n");
    remove_extra_files(source, target);

    printf("Step 2: Restoring files from backup...\n");
    if (restore_recursive(target, source, target, source) < 0)
    {
        fprintf(stderr, "Error: Restore failed\n");
    }
    else
    {
        printf("Restore completed successfully!\n");
    }
}

int handle_end_command(struct Backup *backups, int *backup_count, struct ParsedLine *parsed)
{
    if (parsed->count < 3)
    {
        fprintf(stderr, "Usage: end <source> <target1> [target2 ...]\n");
        return 0;
    }

    const char *source = parsed->chars[1];

    int backup_idx = -1;
    for (int i = 0; i < *backup_count; i++)
    {
        if (strcmp(backups[i].source_path, source) == 0)
        {
            backup_idx = i;
            break;
        }
    }

    if (backup_idx == -1)
    {
        fprintf(stderr, "Error: No backup found for source '%s'\n", source);
        return 0;
    }

    int removed_count = 0;
    for (int t = 2; t < parsed->count; t++)
    {
        const char *target = parsed->chars[t];

        for (int j = 0; j < backups[backup_idx].target_count; j++)
        {
            if (strcmp(backups[backup_idx].targets[j].target_path, target) == 0)
            {
                if (backups[backup_idx].targets[j].pid > 0)
                {
                    kill(backups[backup_idx].targets[j].pid, SIGTERM);
                    waitpid(backups[backup_idx].targets[j].pid, NULL, 0);
                    printf("Stopped backup to '%s'\n", target);
                }

                free(backups[backup_idx].targets[j].target_path);

                for (int k = j; k < backups[backup_idx].target_count - 1; k++)
                {
                    backups[backup_idx].targets[k] = backups[backup_idx].targets[k + 1];
                }

                backups[backup_idx].target_count--;
                removed_count++;
                break;
            }
        }
    }

    if (removed_count > 0)
    {
        printf("Ended %d backup(s)\n", removed_count);

        if (backups[backup_idx].target_count == 0)
        {
            printf("No more targets for '%s', removing backup\n", source);

            close(backups[backup_idx].notify_fd);
            for (int j = 0; j < backups[backup_idx].watch_map.watch_count; j++)
            {
                free(backups[backup_idx].watch_map.watch_map[j].path);
            }
            free(backups[backup_idx].source_path);
            free(backups[backup_idx].targets);

            for (int i = backup_idx; i < *backup_count - 1; i++)
            {
                backups[i] = backups[i + 1];
            }
            (*backup_count)--;
        }
    }
    else
    {
        fprintf(stderr, "Error: No matching targets found\n");
    }

    return 0;
}

int validate_and_create_targets(const char *source, char **targets, int *target_count, struct Backup *backups,
                                int backup_count, struct ParsedLine *parsed, int existing_idx)
{
    *target_count = 0;

    for (int t = 2; t < parsed->count && *target_count < MAX_TARGETS; t++)
    {
        const char *target = parsed->chars[t];

        if (is_path_inside(source, target))
        {
            fprintf(stderr, "Error: Cannot create backup of '%s' inside itself at '%s'\n", source, target);
            continue;
        }

        if (is_path_inside(target, source))
        {
            fprintf(stderr, "Error: Cannot create backup at '%s' - source '%s' is inside target\n", target, source);
            continue;
        }

        if (existing_idx >= 0)
        {
            int duplicate = 0;
            for (int j = 0; j < backups[existing_idx].target_count; j++)
            {
                if (strcmp(backups[existing_idx].targets[j].target_path, target) == 0)
                {
                    fprintf(stderr, "Error: Backup from '%s' to '%s' already exists\n", source, target);
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate)
                continue;
        }

        struct stat st;
        if (stat(target, &st) == 0)
        {
            if (!S_ISDIR(st.st_mode))
            {
                fprintf(stderr, "Error: Target '%s' exists but is not a directory\n", target);
                continue;
            }

            int empty = is_dir_empty(target);
            if (empty < 0)
            {
                fprintf(stderr, "Error: Cannot access target directory '%s'\n", target);
                continue;
            }
            if (!empty)
            {
                fprintf(stderr, "Error: Target directory '%s' must be empty\n", target);
                continue;
            }
        }
        else
        {
            if (mkpath(target, 0755) < 0)
            {
                fprintf(stderr, "Error: Cannot create target directory '%s'\n", target);
                continue;
            }
        }

        printf("Creating backup from '%s' to '%s'...\n", source, target);
        if (copy_recursive(source, target, source, target) < 0)
        {
            fprintf(stderr, "Error: Failed to copy to '%s'\n", target);
            remove_recursive(target);
            continue;
        }
        // printf("Backup to '%s' completed successfully!\n", target);

        char *target_copy = strdup(target);
        if (!target_copy)
        {
            perror("strdup");
            fprintf(stderr, "Error: Memory allocation failed for target '%s'\n", target);
            remove_recursive(target);
            continue;
        }
        targets[(*target_count)++] = target_copy;
    }

    return *target_count > 0 ? 0 : -1;
}

int initialize_new_backup(struct Backup *backup, const char *source, char **targets, int target_count)
{
    backup->source_path = strdup(source);
    if (!backup->source_path)
    {
        perror("strdup");
        return -1;
    }

    backup->targets = malloc(target_count * sizeof(struct TargetProcess));
    if (!backup->targets)
    {
        perror("malloc");
        free(backup->source_path);
        return -1;
    }

    backup->target_count = target_count;
    backup->notify_fd = inotify_init();

    if (backup->notify_fd < 0)
    {
        perror("inotify_init");
        free(backup->source_path);
        free(backup->targets);
        return -1;
    }

    for (int i = 0; i < target_count; i++)
    {
        backup->targets[i].target_path = targets[i];

        fflush(stdout);
        fflush(stderr);
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            backup->targets[i].pid = -1;
        }
        else if (pid == 0)
        {
            monitor_process(source, targets[i]);
        }
        else
        {
            backup->targets[i].pid = pid;
            // printf("Started process %d for target '%s'\n", pid, targets[i]);
        }
    }

    return 0;
}

int add_targets_to_existing_backup(struct Backup *backup, char **targets, int target_count)
{
    int old_count = backup->target_count;
    backup->targets = realloc(backup->targets, (old_count + target_count) * sizeof(struct TargetProcess));

    if (!backup->targets)
    {
        perror("realloc");
        for (int i = 0; i < target_count; i++)
        {
            free(targets[i]);
        }
        backup->target_count = 0;
        return -1;
    }

    for (int i = 0; i < target_count; i++)
    {
        int new_idx = old_count + i;
        backup->targets[new_idx].target_path = targets[i];

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            backup->targets[new_idx].pid = -1;
            free(targets[i]);
            backup->targets[new_idx].target_path = NULL;
        }
        else if (pid == 0)
        {
            monitor_process(backup->source_path, targets[i]);
        }
        else
        {
            backup->targets[new_idx].pid = pid;
            // printf("Started process %d for target '%s'\n", pid, targets[i]);
        }
    }

    backup->target_count += target_count;
    printf("Added %d new target(s) to existing backup\n", target_count);
    return 0;
}

int handle_add_command(struct Backup *backups, int *backup_count, struct ParsedLine *parsed)
{
    if (parsed->count < 3)
    {
        fprintf(stderr, "Usage: add <source> <target1> [target2 ...]\n");
        return 0;
    }

    const char *source = parsed->chars[1];

    struct stat st;
    if (stat(source, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: Source must be a directory\n");
        return 0;
    }

    int already_exists = 0;
    int existing_idx = -1;
    for (int i = 0; i < *backup_count; i++)
    {
        if (strcmp(backups[i].source_path, source) == 0)
        {
            already_exists = 1;
            existing_idx = i;
            break;
        }
    }

    char *target_paths[MAX_TARGETS];
    int target_count = 0;

    if (validate_and_create_targets(source, target_paths, &target_count, backups, *backup_count, parsed, existing_idx) <
        0)
    {
        fprintf(stderr, "Error: No backups were created\n");
        return 0;
    }

    if (!already_exists && *backup_count < MAX_BACKUPS)
    {
        if (initialize_new_backup(&backups[*backup_count], source, target_paths, target_count) < 0)
        {
            for (int i = 0; i < target_count; i++)
            {
                free(target_paths[i]);
            }
            return 0;
        }
        (*backup_count)++;
    }
    else if (already_exists)
    {
        if (add_targets_to_existing_backup(&backups[existing_idx], target_paths, target_count) < 0)
        {
            return 0;
        }
    }

    return 0;
}
