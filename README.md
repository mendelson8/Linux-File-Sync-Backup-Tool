# Linux Backup

An academic systems programming project written in C. `Linux Backup` creates
and continuously synchronizes directory backups on Linux.

The application uses `inotify` to detect file-system events and starts an
independent worker process for each backup target.

## Features

- Create a backup for one or multiple target directories
- Synchronize file and directory creation, modification, deletion, and moves
- Restore the source directory from a backup
- Preserve and translate symbolic links where appropriate
- Interactive CLI with `add`, `list`, `end`, `restore`, and `exit` commands
- Graceful process termination and resource cleanup

## Technologies

- C17
- POSIX system calls
- Linux `inotify`
- Processes and signals (`fork`, `waitpid`, `SIGTERM`)
- GNU Make

## Architecture

The source code is organized into focused modules:

```text
src/
├── main.c        # Main CLI loop
├── commands.c    # Command handling and backup lifecycle
├── filesystem.c  # File operations and recursive copying
├── monitor.c     # inotify event processing
├── watch_map.c   # Watch descriptor to path mapping
├── parser.c      # Command-line parsing
├── signals.c     # Signal handling
└── common.h      # Shared types and declarations
```

## Build and Run

Requirements: Linux, a C17-compatible compiler, and GNU Make.

```bash
make
./sop-backup
```

Available commands:

```text
add <source> <target1> [target2 ...]
list
end <source> <target1> [target2 ...]
restore <source> <target>
exit
```