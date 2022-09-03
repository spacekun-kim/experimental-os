#include "stdint.h"
#include "cmd_hash.h"
#include "builtin_cmd.h"
#include "hash.h"

#define CMD_COUNT 9
char* cmds[CMD_COUNT] = {"pwd", "cd", "ls", "ps", "clear", "mkdir", "rmdir", "rm", "test"};
void* funcs[CMD_COUNT] = {builtin_pwd, builtin_cd, builtin_ls, builtin_ps, builtin_clear, builtin_mkdir, builtin_rmdir, builtin_rm, builtin_test};

HT cmd_list;

void cmd_list_init(void) {
	hash_init(&cmd_list, 29, -1);
	for(int i = 0; i < CMD_COUNT; ++i) {
		hash_insert(&cmd_list, cmds[i], (uint32_t)(funcs[i]));
	}
}

void* cmd2func(char* cmd) {
	int32_t ret = hash_get(&cmd_list, cmd);
	if(ret == -1) 
		return NULL;
	return (void*)ret;
}
