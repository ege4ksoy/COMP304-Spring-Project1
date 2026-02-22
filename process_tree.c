#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ─── Data Structure ─── */

typedef struct {
  int pid;
  int ppid;
  char name[256];
} proc_info_t;

#define MAX_PROCS 4096

/* ─── /proc Scanning ─── */

// Reads PID, PPID and process name from /proc/<pid>/status
static int read_proc_status(const char *pid_str, proc_info_t *info) {
  char path[512];
  snprintf(path, sizeof(path), "/proc/%s/status", pid_str);

  FILE *fp = fopen(path, "r");
  if (!fp)
    return -1;

  int got_name = 0, got_pid = 0, got_ppid = 0;
  char line[512];

  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "Name:", 5) == 0) {
      sscanf(line + 5, " %255s", info->name);
      got_name = 1;
    } else if (strncmp(line, "Pid:", 4) == 0) {
      sscanf(line + 4, " %d", &info->pid);
      got_pid = 1;
    } else if (strncmp(line, "PPid:", 5) == 0) {
      sscanf(line + 5, " %d", &info->ppid);
      got_ppid = 1;
    }
    if (got_name && got_pid && got_ppid)
      break;
  }

  fclose(fp);
  return (got_name && got_pid && got_ppid) ? 0 : -1;
}

// Scans all numeric directories under /proc to build the process list
static int read_all_procs(proc_info_t *procs) {
  DIR *dir = opendir("/proc");
  if (!dir) {
    perror("opendir /proc");
    return 0;
  }

  int count = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL && count < MAX_PROCS) {
    // Only numeric directories are processes
    if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
      continue;

    if (read_proc_status(entry->d_name, &procs[count]) == 0)
      count++;
  }

  closedir(dir);
  return count;
}

/* ─── Tree Rendering ─── */

// Draws the tree using Unicode box-drawing characters
static void print_tree(int root_pid, int depth, int *is_last,
                       proc_info_t *procs, int count) {
  // Find the root node
  proc_info_t *root = NULL;
  for (int i = 0; i < count; i++) {
    if (procs[i].pid == root_pid) {
      root = &procs[i];
      break;
    }
  }
  if (!root)
    return;

  // Indentation: │ or space for each level
  for (int i = 0; i < depth; i++) {
    if (is_last[i])
      printf("    ");
    else
      printf("│   ");
  }

  // Connection character
  if (depth > 0) {
    if (is_last[depth - 1])
      printf("└── ");
    else
      printf("├── ");
  }

  // Process info
  printf("\033[1;36m%s\033[0m (\033[33m%d\033[0m)\n", root->name, root->pid);

  // Find children
  int children[MAX_PROCS];
  int child_count = 0;
  for (int i = 0; i < count; i++) {
    if (procs[i].ppid == root_pid && procs[i].pid != root_pid)
      children[child_count++] = procs[i].pid;
  }

  // Print children recursively
  for (int i = 0; i < child_count; i++) {
    is_last[depth] = (i == child_count - 1);
    print_tree(children[i], depth + 1, is_last, procs, count);
  }
}

/* ─── Main Entry Point ─── */

void handle_process_tree(int argc, char *argv[]) {
  int root_pid = 1; // default: full tree from PID 1
  int show_me = 0;

  // Argument parsing
  for (int i = 1; i < argc; i++) {
    if (argv[i] == NULL)
      break;

    if (strcmp(argv[i], "--me") == 0) {
      show_me = 1;
    } else if (strcmp(argv[i], "--pid") == 0) {
      if (i + 1 < argc && argv[i + 1] != NULL) {
        char *endptr;
        long val = strtol(argv[++i], &endptr, 10);
        if (*endptr != '\0' || val <= 0) {
          fprintf(stderr, "process_tree: invalid PID: '%s'\n", argv[i]);
          return;
        }
        root_pid = (int)val;
      } else {
        fprintf(stderr, "process_tree: --pid requires a value\n");
        return;
      }
    }
  }

  // --me: use the current shell's PID as root
  // Note: since we run inside a fork, getppid() gives us the shell's PID
  if (show_me)
    root_pid = getppid();

  // Read all processes
  proc_info_t *procs = malloc(sizeof(proc_info_t) * MAX_PROCS);
  if (!procs) {
    perror("malloc");
    return;
  }

  int count = read_all_procs(procs);
  if (count == 0) {
    fprintf(stderr, "process_tree: failed to read processes\n");
    free(procs);
    return;
  }

  // Check if the given root PID exists
  int found = 0;
  for (int i = 0; i < count; i++) {
    if (procs[i].pid == root_pid) {
      found = 1;
      break;
    }
  }
  if (!found) {
    fprintf(stderr, "process_tree: PID %d not found\n", root_pid);
    free(procs);
    return;
  }

  // Draw the tree
  int is_last[256] = {0};
  printf("\n\033[1;35m─── Process Tree ───\033[0m\n\n");
  print_tree(root_pid, 0, is_last, procs, count);
  printf("\n");

  free(procs);
}
