#include "json_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Encode a Unicode codepoint as UTF-8 into buf.
 * Returns number of bytes written, 0 on error. */
static size_t encode_utf8(unsigned long cp, char *buf)
{
    if (cp <= 0x7F) {
        buf[0] = (char)(cp & 0x7F);
        return 1;
    } else if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | ((cp >> 6) & 0x1F));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
        buf[1] = (char)(0x80 | ((cp >> 6)  & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = (char)(0xF0 | ((cp >> 18) & 0x07));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6)  & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0; /* invalid codepoint */
}

static int parse_hex4(const char *s, unsigned int *out)
{
    unsigned int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if      (c >= '0' && c <= '9') v |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned int)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

/*
 * unescape_string - takes the raw bytes *between* the quotes and returns a
 *                   newly malloc'd, null-terminated, properly decoded string.
 *                   Returns NULL on error (sets p->err).
 */
char *unescape_string(parser_t *p, const char *raw, size_t len)
{
    /* Worst case: every \uXXXX (6 bytes) → 4 UTF-8 bytes — output ≤ input. */
    char  *out = (char *)malloc(len + 1);
    size_t wi  = 0;

    if (!out) {
        set_error(p, 0, 0, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)raw[i];

        if (c != '\\') {
            out[wi++] = (char)c;
            i++;
            continue;
        }

        /* escape sequence */
        i++;
        if (i >= len) {
            set_error(p, 0, 0, "truncated escape sequence in string");
            free(out);
            return NULL;
        }

        switch (raw[i]) {
            case '"':  out[wi++] = '"';  i++; break;
            case '\\': out[wi++] = '\\'; i++; break;
            case '/':  out[wi++] = '/';  i++; break;
            case 'b':  out[wi++] = '\b'; i++; break;
            case 'f':  out[wi++] = '\f'; i++; break;
            case 'n':  out[wi++] = '\n'; i++; break;
            case 'r':  out[wi++] = '\r'; i++; break;
            case 't':  out[wi++] = '\t'; i++; break;
            case 'u': {
                i++;
                if (i + 4 > len) {
                    set_error(p, 0, 0, "truncated \\uXXXX escape");
                    free(out);
                    return NULL;
                }
                unsigned int hi = 0;
                if (!parse_hex4(raw + i, &hi)) {
                    set_error(p, 0, 0,
                              "invalid hex in \\uXXXX escape: %.4s", raw + i);
                    free(out);
                    return NULL;
                }
                i += 4;
                unsigned long cp = hi;

                /* Surrogate pair handling (RFC 8259 §7) */
                if (hi >= 0xD800 && hi <= 0xDBFF) {
                    if (i + 2 > len || raw[i] != '\\' || raw[i+1] != 'u') {
                        set_error(p, 0, 0,
                                  "high surrogate U+%04X not followed by \\uXXXX", hi);
                        free(out);
                        return NULL;
                    }
                    i += 2;
                    if (i + 4 > len) {
                        set_error(p, 0, 0, "truncated low surrogate escape");
                        free(out);
                        return NULL;
                    }
                    unsigned int lo = 0;
                    if (!parse_hex4(raw + i, &lo)) {
                        set_error(p, 0, 0, "invalid low surrogate hex");
                        free(out);
                        return NULL;
                    }
                    i += 4;
                    if (lo < 0xDC00 || lo > 0xDFFF) {
                        set_error(p, 0, 0,
                                  "expected low surrogate, got U+%04X", lo);
                        free(out);
                        return NULL;
                    }
                    cp = 0x10000 + ((unsigned long)(hi - 0xD800) << 10)
                                 +  (unsigned long)(lo - 0xDC00);
                } else if (hi >= 0xDC00 && hi <= 0xDFFF) {
                    set_error(p, 0, 0,
                              "unexpected low surrogate U+%04X", hi);
                    free(out);
                    return NULL;
                }

                char utf8[4];
                size_t nb = encode_utf8(cp, utf8);
                if (!nb) {
                    set_error(p, 0, 0, "invalid codepoint U+%06lX", cp);
                    free(out);
                    return NULL;
                }
                memcpy(out + wi, utf8, nb);
                wi += nb;
                break;
            }
            default:
                set_error(p, 0, 0,
                          "invalid escape sequence '\\%c'", raw[i]);
                free(out);
                return NULL;
        }
    }

    out[wi] = '\0';
    return out;
}
