#define _GNU_SOURCE
#include "gpt-util.h"
#include <assert.h>
#include <errno.h>
#include <libfdisk/libfdisk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

const char *root_device() {
  char *dev_block_path = NULL;
  struct stat st;
  dev_t dev;

  if (stat("/usr", &st) != 0) {
    fprintf(stderr, "failed to get root device\n");
    return NULL;
  }

  dev = S_ISBLK(st.st_mode) ? st.st_rdev : st.st_dev;
  asprintf(&dev_block_path, "/dev/block/%d:%d", major(dev), minor(dev));
  return dev_block_path;
}

void free_gpt_xbootldr_partition(gpt_xbootldr_partition_t part[MAX_BOOTSLOTS], int parts_len) {
  for (int i = 0; i < parts_len; i++) {
    if (part[i].label)
      free(part[i].label);
    if (part[i].part_type_name)
      free(part[i].part_type_name);
    if (part[i].part_type_uuid)
      free(part[i].part_type_uuid);
    if (part[i].part_uuid)
      free(part[i].part_uuid);
    if (part[i].bootslot)
      free(part[i].bootslot);
  }
}

int parse_gpt_attrs(gpt_xbootldr_partition_t *part) {
  part->boot_successful =
      ((part->gpt_part_attrs >> GPT_ATTR_BOOT_SUCC_BIT) & 1) == 1;
  part->tries_remaining =
      (part->gpt_part_attrs >> GPT_ATTR_BOOT_TRIES_BIT) & 15;
  part->priority = (part->gpt_part_attrs >> GPT_ATTR_BOOT_PRIORITY_BIT) & 15;
  return 0;
}

int set_gpt_attrs(gpt_xbootldr_partition_t *part) {
  if (part->boot_successful)
    part->gpt_part_attrs |= ((uint64_t)1) << GPT_ATTR_BOOT_SUCC_BIT;
  else
    part->gpt_part_attrs &= ~(((uint64_t)1) << GPT_ATTR_BOOT_SUCC_BIT);

  part->gpt_part_attrs &= ~(((uint64_t)0xF) << GPT_ATTR_BOOT_TRIES_BIT);
  part->gpt_part_attrs |= ((uint64_t)part->tries_remaining)
                          << GPT_ATTR_BOOT_TRIES_BIT;

  part->gpt_part_attrs &= ~(((uint64_t)0xF) << GPT_ATTR_BOOT_PRIORITY_BIT);
  part->gpt_part_attrs |= ((uint64_t)part->priority)
                          << GPT_ATTR_BOOT_PRIORITY_BIT;
  return 0;
}

const char *get_bootslot_name(const char *label) {
  char *bootslot;

  if (!label)
    return "";

  bootslot = strrchr(label, '-');
  if (!bootslot)
    return label;

  bootslot++;
  return bootslot;
}

int gpt_update_attrs(const char *disk_device, gpt_xbootldr_partition_t *part) {
  struct fdisk_context *ctx = NULL;
  int rc;

  ctx = fdisk_new_context();

  rc = fdisk_assign_device(ctx, disk_device, 0);
  if (rc < 0) {
    fprintf(stderr, "Failed to get access to device %s\n", disk_device);
    goto device_error;
  }

  rc = fdisk_gpt_set_partition_attrs(ctx, part->partno, part->gpt_part_attrs);
  if (rc < 0) {
    fprintf(stderr, "Failed to update attributes on partno %lu\n",
            part->partno);
    goto set_error;
  }

  rc = fdisk_write_disklabel(ctx);
  if (rc < 0)
    fprintf(stderr, "Failed to write partition table\n");

set_error:
  fdisk_deassign_device(ctx, 0);

device_error:
  fdisk_unref_context(ctx);
  return rc;
}

int find_boot_partitions(const char *disk_device,
                         gpt_xbootldr_partition_t *parts, int *parts_len) {
  struct fdisk_context *ctx = NULL;
  struct fdisk_label *lb = NULL;
  struct fdisk_table *tb = NULL;
  struct fdisk_iter *it = NULL;
  struct fdisk_partition *pa = NULL;
  struct fdisk_parttype *t = NULL;
  const char *parttype = NULL;
  uint64_t gpt_part_attrs;
  int partno = 0;
  int index = 0;
  int rc;

  ctx = fdisk_new_context();

  rc = fdisk_assign_device(ctx, disk_device, 1);
  if (rc < 0) {
    fprintf(stderr, "Failed to get access to device %s\n", disk_device);
    goto device_error;
  }

  // Look for GPT table
  lb = fdisk_get_label(ctx, "gpt");
  if (lb == NULL) {
    fprintf(stderr, "Failed to get GPT table from device.");
    goto partition_error;
  }

  rc = fdisk_get_partitions(ctx, &tb);
  if (rc < 0) {
    fprintf(stderr, "Failed to get partitions\n");
    goto error;
  }

  it = fdisk_new_iter(FDISK_ITER_FORWARD);
  while (fdisk_table_next_partition(tb, it, &pa) == 0) {
    partno = fdisk_partition_get_partno(pa);

    t = fdisk_partition_get_type(pa);
    if (!t) {
      fprintf(stderr, "Failed to get partition type for partno: %d\n", partno);
      continue;
    }

    // Get GPT Partition type UUID
    parttype = fdisk_parttype_get_string(t);
    if (strcmp(parttype, GPT_PARTTYPE_XBOOTLDR))
      continue;

    // Found an extend boot loader partition (xbootldr)

    // Get GPT partition attributes
    rc = fdisk_gpt_get_partition_attrs(ctx, partno, &gpt_part_attrs);
    if (rc < 0) {
      fprintf(stderr, "Failed to get attributes for GPT partition\n");
      continue;
    }

    parts[index].partno = partno;
    parts[index].label = strdup(fdisk_partition_get_name(pa));
    parts[index].part_uuid = strdup(fdisk_partition_get_uuid(pa));
    parts[index].part_type_uuid = strdup(parttype);
    parts[index].part_type_name = strdup(fdisk_parttype_get_name(t));
    parts[index].bootslot = strdup(get_bootslot_name(parts[index].label));
    parts[index].gpt_part_attrs = gpt_part_attrs;

    // Parse GUID type specific GPT boot attributes (bit 56 - 48)
    parse_gpt_attrs(&parts[index]);

    index++;
    if (index == MAX_BOOTSLOTS)
      break;
  }

  *parts_len = index;

  fdisk_free_iter(it);
  fdisk_unref_table(tb);

error:
  fdisk_unref_partition(pa);
partition_error:
  fdisk_deassign_device(ctx, 0);
device_error:
  fdisk_unref_context(ctx);

  return rc;
}