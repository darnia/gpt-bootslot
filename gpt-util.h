#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_BOOTSLOTS 5
#define GPT_PARTTYPE_XBOOTLDR "BC13C2FF-59E6-4262-A352-B275FD6F7172"
#define BOOTSLOT_DISABLE 0
#define BOOTSLOT_PRIMARY_PRIORITY 10

#pragma pack(1)
typedef union _gpt_entry_attributes {
	struct {
    uint64_t required_to_function:1;
    uint64_t efi_ignore:1;
    uint64_t legacy_bios_bootable:1;
    uint64_t reserved1:45;

    // GUID attributes: Chrome OS kernel partition attributes
		uint64_t boot_priority:4;
		uint64_t tries_remaining:4;
		uint64_t boot_success:1;
		uint64_t reserved2:7;
	} fields;
	uint64_t raw;
} gpt_entry_attr_with_guid_attr;
#pragma pack()

typedef struct {
  size_t partno;
  char *label;
  char *part_uuid;
  char *part_type_uuid;
  char *part_type_name;
  char *bootslot;

  // GPT attributes with guid attr (bootslot info)
  gpt_entry_attr_with_guid_attr gpt_part_attrs;
} gpt_xbootldr_partition_t;

const char *root_device();
void free_gpt_xbootldr_partition(gpt_xbootldr_partition_t part[MAX_BOOTSLOTS], int parts_len);
const char *get_bootslot_name(const char *label);
int gpt_update_attrs(const char *disk_device, gpt_xbootldr_partition_t *part);
int find_boot_partitions(const char *disk_device,
                         gpt_xbootldr_partition_t *parts, int *parts_len);