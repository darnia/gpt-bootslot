#pragma once

#include <stdlib.h>
#include <string.h>

#define CMD_FLAG_DEFAULT 1

typedef struct {
    const char *command;
    unsigned min_args, max_args;
    int(*const dispatch)(int argc, char *argv[], void *userdata);
    int flag;
} command_t;

const command_t *find_command(const char *command, const command_t commands[]);
int dispatch_command(int argc, char *argv[], const command_t commands[],
        void *userdata);
