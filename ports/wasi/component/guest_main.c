/**
 * Component guest wrapper for MicroPython WASI.
 *
 * Converts WIT python interface args to argc/argv and calls mpy_cli_main.
 * Sets the guest's working directory from the host-provided cwd before execution.
 * Redirects stdout/stdin if the host provides file paths (for pipeline/redirect support).
 * This allows MicroPython to be composed into a host runner (e.g. busybox-wasi)
 * via the agentskillmania:subcommand/python interface.
 */
#include "guest_python.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Forward declaration -- MicroPython CLI entry (defined in main.c) */
extern int mpy_cli_main(int argc, char **argv);

int32_t exports_agentskillmania_subcommand_python_execute(
    guest_python_string_t *cwd,
    guest_python_list_string_t *args,
    guest_python_string_t *stdout_file,
    guest_python_string_t *stdin_file)
{
    /* Sync guest cwd with host */
    if (cwd->len > 0) {
        char *s = malloc(cwd->len + 1);
        if (s) {
            memcpy(s, cwd->ptr, cwd->len);
            s[cwd->len] = '\0';
            chdir(s);
            free(s);
        }
    }

    /* Redirect stdout if host requests it (for pipelines/redirects) */
    if (stdout_file->len > 0) {
        char *s = malloc(stdout_file->len + 1);
        if (s) {
            memcpy(s, stdout_file->ptr, stdout_file->len);
            s[stdout_file->len] = '\0';
            freopen(s, "w", stdout);
            free(s);
        }
    }

    /* Redirect stdin if host requests it (for pipeline input) */
    if (stdin_file->len > 0) {
        char *s = malloc(stdin_file->len + 1);
        if (s) {
            memcpy(s, stdin_file->ptr, stdin_file->len);
            s[stdin_file->len] = '\0';
            freopen(s, "r", stdin);
            free(s);
        }
    }

    int argc = (int)args->len;
    if (argc == 0) return 1;

    char **argv = malloc(sizeof(char *) * (size_t)(argc + 1));
    if (!argv) return 1;

    for (size_t i = 0; i < (size_t)argc; i++) {
        /* guest_python strings are NOT null-terminated */
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
