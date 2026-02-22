#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * handle_cut — main entry point for the cut command.
 *
 * Expected arguments (passed through the shell):
 *   argv[1] = delimiter   — delimiter character
 *   argv[2] = fields      — fields to print
 *
 * @param argc  Argument count (must be >= 2).
 * @param argv  Argument vector.
 */
void handle_cut(int argc, char *argv[]) {
  char delimiter = '\t'; // default TAB
  int fields[100];       // field list
  int field_count = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i] == NULL)
      break;

    if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--delimiter") == 0)) {
      if (i + 1 < argc) {
        delimiter = argv[++i][0];
      }
    } else if (strncmp(argv[i], "-d", 2) == 0 && strlen(argv[i]) > 2) {
      // combined format: -d:
      delimiter = argv[i][2];
    } else if ((strcmp(argv[i], "-f") == 0) ||
               (strcmp(argv[i], "--fields") == 0)) {
      if (i + 1 < argc) {
        char *token = strtok(argv[++i], ",");
        while (token != NULL && field_count < 100) {
          fields[field_count++] = atoi(token);
          token = strtok(NULL, ",");
        }
      }
    } else if (strncmp(argv[i], "-f", 2) == 0 && strlen(argv[i]) > 2) {
      // combined format: -f1,3
      char *field_str = argv[i] + 2;
      char field_copy[1024];
      strncpy(field_copy, field_str, sizeof(field_copy) - 1);
      field_copy[sizeof(field_copy) - 1] = '\0';
      char *token = strtok(field_copy, ",");
      while (token != NULL && field_count < 100) {
        fields[field_count++] = atoi(token);
        token = strtok(NULL, ",");
      }
    }
  }

  char line[4096];
  while (fgets(line, sizeof(line), stdin)) {
    line[strcspn(line, "\n")] = '\0';

    char *tokens[100];
    int total_token = 0;

    char *start = line;
    char *ptr = line;
    while (*ptr != '\0') {
      if (*ptr == delimiter) {
        *ptr = '\0'; // change delimiter to null
        tokens[total_token++] = start;
        start = ptr + 1;
        if (total_token >= 100)
          break;
      }
      ptr++;
    }
    // add last token
    if (total_token < 100) {
      tokens[total_token++] = start;
    }

    // print fields
    for (int i = 0; i < field_count; i++) {
      int target = fields[i]; // 1-indexed
      if (target >= 1 && target <= total_token) {
        if (i > 0) {
          printf("%c", delimiter);
        }
        printf("%s", tokens[target - 1]);
      }
    }
    printf("\n");
  }
}