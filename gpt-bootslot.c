#include "gpt-bootslot.h"
#include "command-util.h"
#include "gpt-util.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *arg_disk_device = NULL;

static int help(int argc, char *argv[], void *userdata)
{
    printf("Usage: %s [OPTIONS...] <command> [BOOTSLOT [STATE]]\n"
           "GPT boot partition mark good / bad and change primary.\n\n"
           "Options:\n"
           "  -h, --help                      Show this help\n"
           "      --version                   Show package version\n"
           "      --disk-device=dev           Disk device (ex. /dev/sda)\n"
           "Commands:\n"
           "  show                            Show boot partitions.\n"
           "  get-state <BOOTSLOT>            Get bootslot state (good / bad).\n"
           "  set-state <BOOTSLOT> <STATE>    Set bootslot state.\n"
           "  get-primary                     Get primary bootslot.\n"
           "  set-primary <BOOTSLOT>          Set primary bootslot.\n",
           PROGRAM_EXEC);
    return 0;
}

static int parse_options(int argc, char *argv[])
{

    enum {
        ARG_VERSION = 0x100,
        ARG_DISK_DEVICE,
    };

    static const struct option options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, ARG_VERSION},
        {"disk-device", required_argument, NULL, ARG_DISK_DEVICE},
        {}
    };

    int c;

    assert(argc >= 0);
    assert(argv);

    while ((c = getopt_long(argc, argv, "hv", options, NULL)) >= 0) {
        switch (c) {

        case 'h':
            help(argc, argv, NULL);
            return 0;

        case ARG_VERSION:
            puts(PROGRAM_STRING);
            return 0;

        case ARG_DISK_DEVICE:
            arg_disk_device = optarg;
            break;

        case '?':
            return -EINVAL;

        default:
            fprintf(stderr, "Unhandled option\n");
        }
    }

    return 1;
}

static int cmp_priority(const void *p1, const void *p2)
{
    if (((gpt_xbootldr_partition_t const *)p1)->gpt_part_attrs.fields.boot_priority <
        ((gpt_xbootldr_partition_t const *)p2)->gpt_part_attrs.fields.boot_priority) {
        return 1;
    } else {
        return -1;
    }
}

static int get_primary(int argc, char *argv[], void *userdata)
{
    gpt_xbootldr_partition_t parts[MAX_BOOTSLOTS];
    int parts_len;
    int rc;
    char *primary = NULL;
    uint8_t priority = 0;

    rc = find_boot_partitions(arg_disk_device, parts, &parts_len);
    if (rc < 0)
        return rc;

    for (int i = 0; i < parts_len; i++) {
        if (priority < parts[i].gpt_part_attrs.fields.boot_priority) {
            priority = parts[i].gpt_part_attrs.fields.boot_priority;
            primary = parts[i].bootslot;
        }
    }

    if (primary) {
        printf("%s\n", primary);
        rc = 0;
    } else
        rc = -EINVAL;

    free_gpt_xbootldr_partition(parts, parts_len);

    return rc;
}

static int set_primary(int argc, char *argv[], void *userdata)
{
    gpt_xbootldr_partition_t parts[MAX_BOOTSLOTS];
    int parts_len;
    int rc;
    char *primary = NULL;
    int new_prio = BOOTSLOT_PRIMARY_PRIORITY;

    primary = argv[0];

    rc = find_boot_partitions(arg_disk_device, parts, &parts_len);
    if (rc < 0)
        return rc;

    // sort partition by priority
    qsort(&parts, parts_len, sizeof(gpt_xbootldr_partition_t), cmp_priority);

    for (int i = 0; i < parts_len; i++) {
        int is_new_bootslot = !strcmp(primary, parts[i].bootslot);
        if (i == 0 && is_new_bootslot && parts[i].gpt_part_attrs.fields.tries_remaining > 0 &&
            parts[i].gpt_part_attrs.fields.boot_priority == BOOTSLOT_PRIMARY_PRIORITY)
            break; // Nothing to do
        if (is_new_bootslot) {
            parts[i].gpt_part_attrs.fields.boot_priority = BOOTSLOT_PRIMARY_PRIORITY;
            parts[i].gpt_part_attrs.fields.tries_remaining = 3;
        } else if (!parts[i].gpt_part_attrs.fields.boot_success && parts[i].gpt_part_attrs.fields.boot_priority == 0)
            continue; // bootslot is marked BAD. Skipping setting priority
        else
            parts[i].gpt_part_attrs.fields.boot_priority = --new_prio;

        rc = gpt_update_attrs(arg_disk_device, &parts[i]);
    }
    free_gpt_xbootldr_partition(parts, parts_len);

    return 0;
}

static int get_state(int argc, char *argv[], void *userdata)
{
    gpt_xbootldr_partition_t parts[MAX_BOOTSLOTS];
    int parts_len;
    char *bootslot = NULL;
    int rc;

    bootslot = argv[0];

    rc = find_boot_partitions(arg_disk_device, parts, &parts_len);
    if (rc < 0)
        return rc;

    rc = -EINVAL;
    for (int i = 0; i < parts_len; i++) {
        if (!strcmp(bootslot, parts[i].bootslot)) {
            if (parts[i].gpt_part_attrs.fields.boot_priority == 0) {
                printf("bad\n");
                rc = 0;
                break;
            } else if (parts[i].gpt_part_attrs.fields.boot_success ||
                       (!parts[i].gpt_part_attrs.fields.boot_success && parts[i].gpt_part_attrs.fields.boot_priority > 0 &&
                        parts[i].gpt_part_attrs.fields.tries_remaining > 0)) {
                printf("good\n");
                rc = 0;
                break;
            }
        }
    }
    free_gpt_xbootldr_partition(parts, parts_len);
    return rc;
}

static int set_state(int argc, char *argv[], void *userdata)
{
    gpt_xbootldr_partition_t parts[MAX_BOOTSLOTS];
    int parts_len;
    char *bootslot = NULL;
    char *new_state_str = NULL;
    bootstate_e new_state;
    int rc;

    bootslot = argv[0];
    new_state_str = argv[1];

    if (!strcmp(new_state_str, "good"))
        new_state = BOOTSLOT_GOOD;
    else if (!strcmp(new_state_str, "bad"))
        new_state = BOOTSLOT_BAD;
    else {
        fprintf(stderr, "Invalid state %s\n", new_state_str);
        return -EINVAL;
    }

    rc = find_boot_partitions(arg_disk_device, parts, &parts_len);
    if (rc < 0)
        return rc;

    for (int i = 0; i < parts_len; i++) {
        if (!strcmp(bootslot, parts[i].bootslot)) {
            parts[i].gpt_part_attrs.fields.boot_success = (new_state == BOOTSLOT_GOOD);

            if (new_state == BOOTSLOT_BAD) {
                parts[i].gpt_part_attrs.fields.boot_priority = BOOTSLOT_DISABLE;
                parts[i].gpt_part_attrs.fields.tries_remaining = 0;
            }

            rc = gpt_update_attrs(arg_disk_device, &parts[i]);
            break;
        }
    }
    free_gpt_xbootldr_partition(parts, parts_len);
    return rc;
}

static int show(int argc, char *argv[], void *userdata)
{
    gpt_xbootldr_partition_t parts[MAX_BOOTSLOTS];
    int parts_len;
    int rc;

    rc = find_boot_partitions(arg_disk_device, parts, &parts_len);
    if (rc < 0)
        return rc;

    for (int i = 0; i < parts_len; i++) {
        printf("\nPartno:               %ld\n", parts[i].partno);
        printf("Part UUID:            %s\n", parts[i].part_uuid);
        printf("Part label:           %s\n", parts[i].label);
        printf("Part type:            %s (%s)\n", parts[i].part_type_name,
               parts[i].part_type_uuid);

        printf("Bootslot:             %s\n", parts[i].bootslot);
        printf("Boot successful:      %s\n",
               parts[i].gpt_part_attrs.fields.boot_success ? "true" : "false");
        printf("Boot tries remaining: %d\n", parts[i].gpt_part_attrs.fields.tries_remaining);
        printf("Boot priority:        %d\n", parts[i].gpt_part_attrs.fields.boot_priority);
    }
    free_gpt_xbootldr_partition(parts, parts_len);

    return 0;
}

static int run_command(int argc, char *argv[])
{
    static const command_t commands[] = {{"show", 0, 0, show, CMD_FLAG_DEFAULT},
                                         {"get-state", 1, 1, get_state},
                                         {"set-state", 2, 2, set_state},
                                         {"get-primary", 0, 0, get_primary},
                                         {"set-primary", 1, 1, set_primary},
                                         {"help", 0, 0, help},
                                         {}};

    return dispatch_command(argc, argv, commands, NULL);
}

int main(int argc, char *argv[])
{
    int rc;
    rc = parse_options(argc, argv);
    if (rc <= 0)
        return rc;

    if (!arg_disk_device)
        arg_disk_device = root_device();

    return run_command(argc, argv);
}