# COMP304 – Shell-ish: Project 1

**Repository:** [https://github.com/ege4ksoy/COMP304-Spring-Project1](https://github.com/ege4ksoy/COMP304-Spring-Project1)

A custom Unix shell written in C that supports built-in commands, piping, I/O redirection, background processes, and several custom utilities.

---

## Building

Compile all source files together with `gcc`:

```bash
gcc -o shellish shellish-skeleton.c my_cut.c chatroom.c process_tree.c -Wall
```

## Running

```bash
./shellish
```

You will see a prompt like:

```
user@hostname:/current/dir shellish$
```

---

## Features

### External Commands & PATH Resolution

Any command available on your system (`ls`, `cat`, `echo`, etc.) is resolved by searching the `PATH` environment variable and executed via `execv()`.

### Background Processes

Append `&` to any command to execute it in the background:

```
sleep 10 &
```

The shell will return the prompt immediately without waiting for the process to finish.

### I/O Redirection

| Syntax       | Description                |
| ------------ | -------------------------- |
| `<file`      | Redirect stdin from file   |
| `>file`      | Redirect stdout to file    |
| `>>file`     | Append stdout to file      |

**Examples:**

```
cat <input.txt >output.txt
echo hello >>log.txt
```

### Piping

Chain commands with `|`. The stdout of the left command becomes the stdin of the right command:

```
cat file.txt | cut -d: -f1
ls -la | grep .c
```

### Command History

Press the **Up Arrow** key to recall the previous command.

---

## Built-in Commands

### `cd <directory>`

Changes the current working directory.

```
cd /home/user/Documents
```

### `exit`

Exits the shell.

---

## Custom Commands

### `process_tree`

Visualizes the Linux process hierarchy as a tree with Unicode box-drawing characters and colored output.

**Usage:**

```
process_tree                # Full tree starting from PID 1 (init/systemd)
process_tree --pid <PID>    # Tree rooted at a specific PID
process_tree --me           # Tree rooted at the shell's own PID
```

**Flags:**

| Flag          | Description                                        |
| ------------- | -------------------------------------------------- |
| *(no flags)*  | Displays the entire system process tree from PID 1 |
| `--pid <PID>` | Displays the subtree rooted at the given PID       |
| `--me`        | Displays the subtree rooted at the shell process   |

**How it works:**

1. Scans `/proc` to read each process's PID, parent PID (PPID), and name from `/proc/<pid>/status`.
2. Builds a parent-child relationship map.
3. Recursively prints the tree using `├──`, `└──`, and `│` connectors.
4. Process names are displayed in **cyan** and PIDs in **yellow**.

**Example output:**

```
─── Process Tree ───

systemd (1)
├── systemd-journal (345)
├── sshd (1023)
│   └── sshd (4567)
│       └── bash (4570)
│           └── shellish (4580)
└── cron (1050)
```

---

### `cut`

A custom implementation of the Unix `cut` utility. Extracts fields from each line of standard input.

**Usage:**

```
cut -d<delimiter> -f<fields>
cut --delimiter <char> --fields <field_list>
```

**Options:**

| Option                      | Description                                   |
| --------------------------- | --------------------------------------------- |
| `-d <char>` or `-d<char>`   | Set the field delimiter (default: TAB)        |
| `-f <list>` or `-f<list>`   | Comma-separated list of field numbers (1-indexed) |
| `--delimiter <char>`        | Long form of `-d`                             |
| `--fields <list>`           | Long form of `-f`                             |

**Examples:**

```
echo "a:b:c:d" | cut -d: -f1,3        # Output: a:c
cat /etc/passwd | cut -d: -f1          # Prints all usernames
cut -d, -f2,4 <data.csv               # Extract fields 2 and 4 from a CSV
```

---

### `chatroom`

A multi-user chatroom built on named pipes (FIFOs). Multiple users on the same machine can chat in real time.

**Usage:**

```
chatroom <roomname> <username>
```

**How it works:**

1. Creates a shared directory `/tmp/chatroom-<roomname>`.
2. Each user gets a named pipe (FIFO) under that directory.
3. A **reader child process** continuously monitors the user's pipe for incoming messages.
4. The **parent process** reads input from stdin and broadcasts messages to all other users' pipes by forking short-lived writer child processes.
5. Pressing `Ctrl+C` cleanly removes the user's pipe and exits.

**Example – two terminal windows:**

```
# Terminal 1                       # Terminal 2
chatroom myroom alice              chatroom myroom bob
[myroom] alice > hello!            [myroom] bob > hi alice!
```

---

## Project Structure

| File                    | Description                                              |
| ----------------------- | -------------------------------------------------------- |
| `shellish-skeleton.c`   | Main shell: prompt, parsing, command dispatch, piping    |
| `process_tree.c`        | `process_tree` command — visualizes the process hierarchy|
| `my_cut.c`              | `cut` command — field extraction from stdin               |
| `chatroom.c`            | `chatroom` command — named-pipe multi-user chat          |
