/*
 * jsonlint.c  –  validate and pretty-print JSON files
 *
 * Usage:
 *   jsonlint [file...]          pretty-print each file to stdout
 *   jsonlint -c [file...]       compact output
 *   jsonlint -q [file...]       quiet: exit code only (0=ok, 1=error)
 *   jsonlint -                  read from stdin
 *
 * Exit codes:
 *   0  all inputs valid
 *   1  at least one parse error
 *   2  usage / IO error
 */

#include "json_internal.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read entire file into heap buffer */

static char *read_file(FILE *f, size_t *out_len)
{
    size_t cap = 4096, len = 0;
    char  *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

/* Process one input */

static int process(const char *label, FILE *f, int compact, int quiet)
{
    size_t len;
    char  *src = read_file(f, &len);
    if (!src) {
        fprintf(stderr, "%s: read error\n", label);
        return 2;
    }

    json_error_t  err;
    json_value_t *root = json_parse(src, &err);
    free(src);

    if (!root) {
        if (!quiet)
            fprintf(stderr, "%s:%d:%d: %s\n",
                    label, err.line, err.col, err.message);
        return 1;
    }

    if (!quiet) {
        char *out = compact ? json_stringify(root) : json_prettify(root);
        if (!out) {
            fprintf(stderr, "%s: stringify failed (OOM)\n", label);
            json_free(root);
            return 2;
        }
        puts(out);
        free(out);
    }

    json_free(root);
    return 0;
}

/* main */

int main(int argc, char **argv)
{
    int compact = 0, quiet = 0;
    int i = 1;

    for (; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        if (strcmp(argv[i], "-c") == 0)      compact = 1;
        else if (strcmp(argv[i], "-q") == 0) quiet   = 1;
        else if (strcmp(argv[i], "--") == 0) { i++; break; }
        else {
            fprintf(stderr, "unknown flag: %s\n", argv[i]);
            fprintf(stderr, "usage: jsonlint [-c] [-q] [file... | -]\n");
            return 2;
        }
    }

    int rc = 0;

    if (i == argc) {
        /* no files: read stdin */
        int r = process("<stdin>", stdin, compact, quiet);
        if (r > rc) rc = r;
    } else {
        for (; i < argc; i++) {
            FILE *f;
            const char *label;
            if (strcmp(argv[i], "-") == 0) {
                f = stdin; label = "<stdin>";
            } else {
                f = fopen(argv[i], "r");
                label = argv[i];
                if (!f) {
                    fprintf(stderr, "%s: cannot open file\n", argv[i]);
                    rc = 2; continue;
                }
            }
            int r = process(label, f, compact, quiet);
            if (f != stdin) fclose(f);
            if (r > rc) rc = r;
        }
    }

    return rc;
}
