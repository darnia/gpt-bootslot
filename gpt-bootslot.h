#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define FILE_KERNEL_TRIES "/etc/kernel/tries"
#define PROGRAM_STRING "GPT Bootslot v1.0"
#define PROGRAM_EXEC "gpt-bootslot"

typedef enum { BOOTSLOT_GOOD, BOOTSLOT_BAD } bootstate_e;

