#ifndef BM_PROCESS_CONSOLE_H
#define BM_PROCESS_CONSOLE_H
#include <stddef.h>
int bm_process_command(const char *);
int bm_process_parse_hex(const char *, unsigned char *, size_t);
#endif
