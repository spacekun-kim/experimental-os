#ifndef __BUILTIN_CMD_H
#define __BUILTIN_CMD_H
#include "stdint.h"
#include "global.h"
void make_clear_abs_path(char* path, char* final_path);
void builtin_pwd(uint32_t argc, char** argv UNUSED);
char* builtin_cd(uint32_t argc, char** argv);
void builtin_ls(uint32_t argc, char** argv);
void builtin_ps(uint32_t argc, char** argv UNUSED);
void builtin_clear(uint32_t argc, char** argv UNUSED);
int32_t builtin_mkdir(uint32_t argc, char** argv);
int32_t builtin_rmdir(uint32_t argc, char** argv);
int32_t builtin_rm(uint32_t argc, char** argv);
void builtin_test(uint32_t argc, char** argv UNUSED);
#endif
