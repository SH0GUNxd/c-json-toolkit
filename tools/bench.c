/*
 * bench.c  –  microbenchmark for json_parse + json_stringify
 *
 * Usage:
 *   ./bench [file] [iterations]
 *   ./bench data.json 10000
 *
 * Reports:
 *   - parse throughput (MB/s)
 *   - stringify throughput (MB/s)
 *   - mean / min / max latency per iteration
 */

#include "../src/json_internal.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Timer */

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Read file */

static char *slurp(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

/* Built-in test payload */

static const char *DEFAULT_JSON =
    "{"
    "\"users\":["
    "{\"id\":1,\"name\":\"Alice\",\"score\":98.5,\"active\":true,\"tags\":[\"admin\",\"user\"]},"
    "{\"id\":2,\"name\":\"Bob\",  \"score\":72.1,\"active\":false,\"tags\":[\"user\"]},"
    "{\"id\":3,\"name\":\"Carol\",\"score\":88.0,\"active\":true,\"tags\":[\"mod\",\"user\"]}"
    "],"
    "\"meta\":{\"version\":3,\"generated\":\"2024-01-01\",\"count\":3}"
    "}";

/* main */

int main(int argc, char **argv)
{
    const char *path  = NULL;
    int         iters = 100000;

    if (argc >= 2 && strcmp(argv[1], "-h") != 0) path  = argv[1];
    if (argc >= 3)                                iters = atoi(argv[2]);
    if (iters <= 0) iters = 100000;

    size_t src_len;
    char  *src;

    if (path) {
        src = slurp(path, &src_len);
        if (!src) return 1;
    } else {
        src     = (char *)DEFAULT_JSON;
        src_len = strlen(DEFAULT_JSON);
        printf("No file given - using built-in payload (%zu bytes)\n\n", src_len);
    }

    /* Warm-up parse */
    json_error_t  err;
    json_value_t *warm = json_parse(src, &err);
    if (!warm) {
        fprintf(stderr, "parse error: %s at %d:%d\n",
                err.message, err.line, err.col);
        if (path) free(src);
        return 1;
    }
    json_free(warm);

    /* Parse benchmark */
    double parse_min = 1e18, parse_max = 0, parse_total = 0;

    for (int i = 0; i < iters; i++) {
        double t0 = now_sec();
        json_value_t *v = json_parse(src, NULL);
        double t1 = now_sec();
        json_free(v);
        double dt = t1 - t0;
        parse_total += dt;
        if (dt < parse_min) parse_min = dt;
        if (dt > parse_max) parse_max = dt;
    }

    double parse_mean = parse_total / iters;
    double parse_mbs  = ((double)src_len / (1024.0 * 1024.0)) / parse_mean;

    /* Stringify benchmark */
    json_value_t *root = json_parse(src, NULL);
    double str_min = 1e18, str_max = 0, str_total = 0;

    for (int i = 0; i < iters; i++) {
        double t0 = now_sec();
        char  *out = json_stringify(root);
        double t1  = now_sec();
        free(out);
        double dt = t1 - t0;
        str_total += dt;
        if (dt < str_min) str_min = dt;
        if (dt > str_max) str_max = dt;
    }

    double str_mean = str_total / iters;
    char  *sample   = json_stringify(root);
    double str_mbs  = ((double)(sample ? strlen(sample) : 0) / (1024.0 * 1024.0))
                      / str_mean;
    free(sample);
    json_free(root);

    /* Report */
    printf("Input:       %s (%zu bytes)\n", path ? path : "<builtin>", src_len);
    printf("Iterations:  %d\n\n", iters);

    printf("%-14s %10s %10s %10s %12s\n",
           "", "mean (us)", "min (us)", "max (us)", "throughput");
    printf("%-14s %10.2f %10.2f %10.2f %9.1f MB/s\n",
           "json_parse",
           parse_mean * 1e6, parse_min * 1e6, parse_max * 1e6, parse_mbs);
    printf("%-14s %10.2f %10.2f %10.2f %9.1f MB/s\n",
           "json_stringify",
           str_mean * 1e6, str_min * 1e6, str_max * 1e6, str_mbs);

    if (path) free(src);
    return 0;
}
