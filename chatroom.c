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

int SUCCESS = 0;
int FAILURE = 1;

/* Global so signal handler can access them */
static char user_pipe[512];
static pid_t reader_pid = -1;

void cleanup(int sig) {
  (void)sig;
  /* Kill reader child if alive */
  if (reader_pid > 0)
    kill(reader_pid, SIGTERM);
  /* Remove the user's named pipe */
  unlink(user_pipe);
  exit(0);
}

int chatroom(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: chatroom <roomname> <username>\n");
    return 1;
  }
  char *roomname = argv[1];
  char *username = argv[2];

  char room_path[256];
  snprintf(room_path, sizeof(room_path), "/tmp/chatroom-%s", roomname);
  if (mkdir(room_path, 0777) == -1 && errno != EEXIST) {
    perror("mkdir");
    return FAILURE;
  }

  snprintf(user_pipe, sizeof(user_pipe), "%s/%s", room_path, username);

  if (mkfifo(user_pipe, 0777) == -1 && errno != EEXIST) {
    perror("mkfifo");
    return FAILURE;
  }

  /* Set up signal handler so Ctrl-C cleans up the pipe */
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);

  printf("Welcome to %s!\n", roomname);

  reader_pid = fork();
  if (reader_pid < 0) {
    perror("fork");
    return FAILURE;
  }

  if (reader_pid == 0) {
    while (1) {
      int fd = open(user_pipe, O_RDONLY);
      if (fd < 0) {
        exit(0);
      }
      char buf[1024];
      ssize_t n;
      while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("\r\033[K%s", buf);
        printf("[%s] %s > ", roomname, username);
        fflush(stdout);
      }
      close(fd);
    }
  } else {
    // WRITING LOOP (Parent)
    char input[1024];

    while (1) {

      /* Print prompt */
      printf("[%s] %s > ", roomname, username);
      fflush(stdout);
      if (fgets(input, sizeof(input), stdin) == NULL)
        break; /* EOF */

      /* Remove trailing newline */
      size_t len = strlen(input);
      if (len > 0 && input[len - 1] == '\n')
        input[len - 1] = '\0';

      if (strlen(input) == 0)
        continue; /* skip empty messages */

      /* Format: [roomname] username: message\n */
      char msg[2048];
      snprintf(msg, sizeof(msg), "[%s] %s: %s\n", roomname, username, input);
      size_t msg_len = strlen(msg);

      DIR *dir = opendir(room_path);
      if (dir == NULL) {
        perror("opendir");
        break;
      }

      struct dirent *entry;

      pid_t writer_pids[256];
      int num_writers = 0;

      while ((entry = readdir(dir)) != NULL) {
        /* Skip '.' and '..' */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        /* Skip our own pipe (we don't send to ourselves) */
        if (strcmp(entry->d_name, username) == 0)
          continue;

        pid_t writer_pid = fork();
        if (writer_pid < 0) {
          perror("fork");
          continue;
        }
        if (writer_pid == 0) {
          char target_pipe[1024];
          snprintf(target_pipe, sizeof(target_pipe), "%s/%s", room_path,
                   entry->d_name);
          int fd_target = open(target_pipe, O_WRONLY | O_NONBLOCK);
          if (fd_target < 0) {
            exit(FAILURE);
          }
          write(fd_target, msg, msg_len);
          close(fd_target);
          exit(0); // When writing is done, child dies
        }
        writer_pids[num_writers++] = writer_pid;
      }
      closedir(dir);

      /* Wait ONLY for writer children by PID — never blocks on reader */
      for (int i = 0; i < num_writers; i++) {
        waitpid(writer_pids[i], NULL, 0);
      }
    }
    cleanup(0);
  }
  return SUCCESS;
}
