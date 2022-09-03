#ifndef __SHELL_SHELL_H
#define __SHELL_SHELL_H

#include "file.h"

extern char cwd_cache[64];
extern char final_path[MAX_PATH_LEN];

void print_prompt(void);
void my_shell(void);
#endif
