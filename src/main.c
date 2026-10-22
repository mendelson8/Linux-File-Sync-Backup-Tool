#include "common.h"

int main()
{
    struct Backup *backups = calloc(MAX_BACKUPS, sizeof(struct Backup));
    if (!backups)
    {
        perror("calloc");
        return EXIT_FAILURE;
    }

    int backup_count = 0;
    char line[4096];

    setup_signals();

    printf("> ");
    fflush(stdout);

    while (!should_exit && fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\n")] = 0;

        struct ParsedLine parsed;
        parse_line(line, &parsed);

        if (parsed.count == 0)
        {
            printf("> ");
            fflush(stdout);
            continue;
        }

        const char *cmd = parsed.chars[0];

        if (strcmp(cmd, "add") == 0)
        {
            handle_add_command(backups, &backup_count, &parsed);
        }
        else if (strcmp(cmd, "list") == 0)
        {
            handle_list_command(backups, backup_count);
        }
        else if (strcmp(cmd, "end") == 0)
        {
            handle_end_command(backups, &backup_count, &parsed);
        }
        else if (strcmp(cmd, "restore") == 0)
        {
            handle_restore_command(&parsed);
        }
        else if (strcmp(cmd, "exit") == 0)
        {
            should_exit = 1;
            break;
        }
        else
        {
            fprintf(stderr, "Unknown command: %s\n", cmd);
        }

        printf("> ");
        fflush(stdout);
    }

    cleanup_backups(backups, backup_count);
    free(backups);

    return 0;
}
