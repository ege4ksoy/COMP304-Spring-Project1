/**
 * chatroom.c — Named-pipe (FIFO) based multi-user chatroom.
 *
 * Architecture:
 *   - A shared directory  /tmp/chatroom-<roomname>  acts as the "room".
 *   - Each participant creates a personal named pipe (FIFO) inside it.
 *   - The process forks into two roles:
 *       CHILD  (reader) — blocks on its own pipe, prints incoming messages.
 *       PARENT (writer) — reads user input from stdin and broadcasts it
 *                         to every *other* user's pipe in the room.
 *
 * Signal handling:
 *   SIGINT / SIGTERM trigger cleanup(): the reader child is killed and
 *   the user's FIFO is removed so the room stays tidy.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Return codes used throughout the module */
int SUCCESS = 0;
int FAILURE = 1;

/*
 * Global state shared with the signal handler so it can clean up
 * regardless of where the signal is caught.
 */
static char user_pipe[512];   /* Absolute path to this user's FIFO */
static pid_t reader_pid = -1; /* PID of the reader child process   */

/**
 * cleanup — signal handler for SIGINT and SIGTERM.
 *
 * Ensures graceful shutdown:
 *   1. Terminates the reader child process (if still alive).
 *   2. Removes the user's named pipe from the room directory.
 *   3. Exits the program.
 *
 * @param sig  The signal number (unused, cast to void).
 */
void cleanup(int sig) {
  (void)sig; /* Suppress unused-parameter warning */

  /* Kill reader child if alive */
  if (reader_pid > 0)
    kill(reader_pid, SIGTERM);

  /* Remove the user's named pipe so the room directory stays clean */
  unlink(user_pipe);
  exit(0);
}

/**
 * chatroom — main entry point for the chatroom command.
 *
 * Expected arguments (passed through the shell):
 *   argv[1] = roomname   — logical name of the chat room
 *   argv[2] = username   — display name for this participant
 *
 * @param argc  Argument count (must be >= 3).
 * @param argv  Argument vector.
 * @return      SUCCESS (0) on normal exit, FAILURE (1) on error.
 */
int chatroom(int argc, char *argv[]) {

  /* --- Argument validation --- */
  if (argc < 3) {
    fprintf(stderr, "Usage: chatroom <roomname> <username>\n");
    return 1;
  }
  char *roomname = argv[1];
  char *username = argv[2];

  /* --- Room directory creation --- */
  /* /tmp/chatroom-<roomname> is the shared meeting point.
   * mkdir will fail with EEXIST if the room already exists — that's fine. */
  char room_path[256];
  snprintf(room_path, sizeof(room_path), "/tmp/chatroom-%s", roomname);
  if (mkdir(room_path, 0777) == -1 && errno != EEXIST) {
    perror("mkdir");
    return FAILURE;
  }

  /* --- Per-user named pipe (FIFO) creation --- */
  /* The pipe file is  <room_path>/<username>.
   * Other users will open this pipe for writing to send us messages. */
  snprintf(user_pipe, sizeof(user_pipe), "%s/%s", room_path, username);

  if (mkfifo(user_pipe, 0777) == -1 && errno != EEXIST) {
    perror("mkfifo");
    return FAILURE;
  }

  /* --- Signal handlers for graceful cleanup on Ctrl-C or kill --- */
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);

  printf("Welcome to %s!\n", roomname);

  /* ================================================================
   *  Fork into READER (child) and WRITER (parent) roles.
   * ================================================================ */
  reader_pid = fork();
  if (reader_pid < 0) {
    perror("fork");
    return FAILURE;
  }

  /* ────────────────────────────────────────────────────────────────
   *  CHILD PROCESS — READER
   *
   *  Continuously opens the user's own FIFO in read-only mode.
   *  When another user writes a message, it appears here.
   *  The pipe is re-opened in a loop so that multiple senders can
   *  connect over time (each open/close cycle handles one batch).
   * ──────────────────────────────────────────────────────────────── */
  if (reader_pid == 0) {
    while (1) {
      /* Blocking open — waits until a writer opens the other end */
      int fd = open(user_pipe, O_RDONLY);
      if (fd < 0) {
        /* Pipe was removed (cleanup ran) — time to exit */
        exit(0);
      }

      char buf[1024];
      ssize_t n;

      /* Read all available data from this writer session */
      while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';

        /* \r\033[K  = carriage return + clear-to-end-of-line
         * This overwrites the current prompt so the incoming
         * message doesn't collide with our typing. */
        printf("\r\033[K%s", buf);

        /* Reprint the prompt so the user can keep typing */
        printf("[%s] %s > ", roomname, username);
        fflush(stdout);
      }
      close(fd);
    }

  /* ────────────────────────────────────────────────────────────────
   *  PARENT PROCESS — WRITER
   *
   *  Reads lines from stdin and broadcasts each message to every
   *  other user in the room by iterating over FIFOs in the room
   *  directory.
   * ──────────────────────────────────────────────────────────────── */
  } else {
    char input[1024];

    while (1) {

      /* Print the chat prompt */
      printf("[%s] %s > ", roomname, username);
      fflush(stdout);

      /* Read a line of input from the user; NULL means EOF (Ctrl-D) */
      if (fgets(input, sizeof(input), stdin) == NULL)
        break;

      /* Strip trailing newline for cleaner message formatting */
      size_t len = strlen(input);
      if (len > 0 && input[len - 1] == '\n')
        input[len - 1] = '\0';

      /* Ignore empty lines (user just pressed Enter) */
      if (strlen(input) == 0)
        continue;

      /* Format the outgoing message:  [roomname] username: message\n */
      char msg[2048];
      snprintf(msg, sizeof(msg), "[%s] %s: %s\n", roomname, username, input);
      size_t msg_len = strlen(msg);

      /* --- Broadcast to all other users in the room --- */

      /* Open the room directory and enumerate all FIFOs */
      DIR *dir = opendir(room_path);
      if (dir == NULL) {
        perror("opendir");
        break;
      }

      struct dirent *entry;

      /*
       * We collect writer child PIDs so we can waitpid() on each one
       * later, without accidentally reaping the long-lived reader child.
       */
      pid_t writer_pids[256];
      int num_writers = 0;

      while ((entry = readdir(dir)) != NULL) {
        /* Skip the '.' and '..' pseudo-directories */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        /* Don't send the message to ourselves */
        if (strcmp(entry->d_name, username) == 0)
          continue;

        /*
         * Fork a short-lived child for each recipient.
         *
         * Why fork?  open() on a FIFO blocks until the other side
         * opens it for reading.  If a recipient is slow or has
         * disconnected, we don't want to block the whole sender loop.
         * Each writer child handles one recipient independently.
         */
        pid_t writer_pid = fork();
        if (writer_pid < 0) {
          perror("fork");
          continue;
        }

        if (writer_pid == 0) {
          /* ---- Writer child: deliver message to one recipient ---- */
          char target_pipe[1024];
          snprintf(target_pipe, sizeof(target_pipe), "%s/%s", room_path,
                   entry->d_name);

          /* O_WRONLY | O_NONBLOCK: fail immediately if no reader */
          int fd_target = open(target_pipe, O_WRONLY | O_NONBLOCK);
          if (fd_target < 0) {
            /* Recipient's pipe isn't being read — skip silently */
            exit(FAILURE);
          }

          write(fd_target, msg, msg_len);
          close(fd_target);
          exit(0); /* Writer child's job is done */
        }

        /* Parent records the child PID for later reaping */
        writer_pids[num_writers++] = writer_pid;
      }
      closedir(dir);

      /*
       * Wait ONLY for writer children by PID.
       * Using plain wait() here would risk reaping the reader child,
       * which must stay alive for the entire session.
       */
      for (int i = 0; i < num_writers; i++) {
        waitpid(writer_pids[i], NULL, 0);
      }
    }

    /* User typed EOF — clean up and exit */
    cleanup(0);
  }
  return SUCCESS;
}
