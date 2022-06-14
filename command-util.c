#include "command-util.h"
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>

const command_t *find_command(const char *command, const command_t commands[]) {
  for (size_t i = 0; commands[i].dispatch; i++)
    if (strcmp(command, commands[i].command) == 0)
      return &commands[i];

  /* At the end of the list. Command not found. */
  return NULL;
}

int dispatch_command(int argc, char *argv[], const command_t commands[],
                     void *userdata) {
  const command_t *command = NULL;
  const char *command_str = NULL;
  int left;

  assert(commands);
  assert(commands[0].dispatch);
  assert(argc >= 0);
  assert(argv);
  assert(argc >= optind);

  left = argc - optind;
  argv += optind;
  optind = 0;
  command_str = argv[0];

  command = find_command(command_str, commands);
  if (!command) {
    if (command_str)
      fprintf(stderr, "Unknown command %s.\n", command_str);
    else
      fprintf(stderr, "Missing command.\n");

    return -EINVAL;
  }

  left--;
  argv++;

  if ((unsigned)left < command->min_args) {
    fprintf(stderr, "Too few arguments.\n");
    return -EINVAL;
  }

  if ((unsigned)left > command->max_args) {
    fprintf(stderr, "Too many arguments.\n");
    return -EINVAL;
  }

  return command->dispatch(left, argv, userdata);
}