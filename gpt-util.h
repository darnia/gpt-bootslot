#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_BOOTSLOTS 5
#define GPT_ATTR_BOOT_SUCC_BIT 56
#define GPT_ATTR_BOOT_TRIES_BIT 52
#define GPT_ATTR_BOOT_PRIORITY_BIT 48
#define GPT_PARTTYPE_XBOOTLDR "BC13C2FF-59E6-4262-A352-B275FD6F7172"
#define BOOTSLOT_DISABLE 0
#define BOOTSLOT_PRIMARY_PRIORITY 10

typedef struct {
  size_t partno;
  char *label;
  char *part_uuid;
  char *part_type_uuid;
  char *part_type_name;
  char *bootslot;

  // GPT attributes
  uint64_t gpt_part_attrs;
  // Parsed GPT attributes
  bool boot_successful;
  uint8_t tries_remaining;
  uint8_t priority;
} gpt_xbootldr_partition_t;

const char *root_device();
void free_gpt_xbootldr_partition(gpt_xbootldr_partition_t part[MAX_BOOTSLOTS], int parts_len);
int parse_gpt_attrs(gpt_xbootldr_partition_t *part);
int set_gpt_attrs(gpt_xbootldr_partition_t *part);
const char *get_bootslot_name(const char *label);
int gpt_update_attrs(const char *disk_device, gpt_xbootldr_partition_t *part);
int find_boot_partitions(const char *disk_device,
                         gpt_xbootldr_partition_t *parts, int *parts_len);