/**
 * Component guest wrapper for MicroPython WASI.
 *
 * Converts WIT subcommand args to argc/argv and calls mpy_cli_main.
 * This allows MicroPython to be composed into a host runner (e.g. busybox-wasi)
 * via the agentskillmania:subcommand interface.
 */
#include "guest_subcommand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration — MicroPython CLI entry (defined in main.c) */
extern int mpy_cli_main(int argc, char **argv);

int32_t exports_agentskillmania_subcommand_subcommand_execute(
    guest_subcommand_list_string_t *args)
{
    int argc = (int)args->len;
    if (argc == 0) return 1;

    char **argv = malloc(sizeof(char *) * (size_t)(argc + 1));
    if (!argv) return 1;

    for (size_t i = 0; i < (size_t)argc; i++) {
        /* guest_subcommand strings are NOT null-terminated */
        size_t len = args->ptr[i].len;
        argv[i] = malloc(len + 1);
        if (!argv[i]) {
            for (size_t j = 0; j < i; j++) free(argv[j]);
            free(argv);
            return 1;
        }
        memcpy(argv[i], args->ptr[i].ptr, len);
        argv[i][len] = '\0';
    }
    argv[argc] = NULL;

    int rc = mpy_cli_main(argc, argv);

    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    return rc;
}
