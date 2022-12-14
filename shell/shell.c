#include "shell.h"
#include "assert.h"
#include "stdio.h"
#include "syscall.h"
#include "string.h"
#include "fs.h"
#include "syscall.h"
#include "cmd_hash.h"
#include "builtin_cmd.h"
#include "pipe.h"

#define cmd_len MAX_PATH_LEN
#define MAX_ARG_NR 16

static char cmd_line[cmd_len] = {0};
char final_path[MAX_PATH_LEN] = {0};

char cwd_cache[64] = {0};

void print_prompt(void) {
	printf("[rabbit@localhost %s]$ ", cwd_cache);
}

static void readline(char* buf, int32_t count) {
	assert(buf != NULL && count > 0);
	char* pos = buf;
	while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
		// printf("[%d]", pos - buf);
		switch (*pos) {
			case '\n':
			case '\r':
				*pos = 0;
				putchar('\n');
				return;

			case '\b':
				if(buf[0] != '\b') {
					--pos;
					putchar('\b');
				}
				break;
			/* ctrl + l */	
			case 'l' - 'a':
				*pos = 0;
				clear();
				print_prompt();
				printf("%s", buf);
				break;
			/* ctrl + u */
			case 'u' - 'a':
				while(buf != pos) {
					putchar('\b');
					*(pos) = 0;
					--pos;
				}
				break;

			default:
				putchar(*pos);
				++pos;
		}
	}
	printf("readline: can't find enter_key in the cmd_line, max num of char is 128!\n");
}

static int32_t cmd_parse(char* cmd_str, char** argv, char token) {
	assert(cmd_str != NULL);
	int32_t arg_idx = 0;
	while(arg_idx < MAX_ARG_NR) {
		argv[arg_idx] = NULL;
		++arg_idx;
	}
	char* next = cmd_str;
	int32_t argc = 0;
	while(*next) {
		while(*next == token) {
			++next;
		}
		if(*next == 0) {
			break;
		}
		argv[argc] = next;

		while(*next && *next != token) {
			++next;
		}

		if(*next) {
			*next++ = 0;
		}
		if(argc > MAX_ARG_NR) {
			return -1;
		}
		++argc;
	}
	return argc;
}

static void cmd_execute(uint32_t argc, char** argv) {
	void (*func)(uint32_t, char**) = cmd2func(argv[0]);
	if(func) {
		/* builtin cmd */
		(*func)(argc, argv);
	} else {
		/* other cmd */
		int32_t pid = fork();
		if(pid) {
			int32_t status;
			int32_t child_pid = wait(&status);
			if(child_pid == -1) {
				panic("my_shell: no child\n");
			}
			printf("child_pid %d, it's status: %d\n", child_pid, status);
		} else {
			make_clear_abs_path(argv[0], final_path);
			argv[0] = final_path;
			struct stat file_stat;
			memset(&file_stat, 0, sizeof(struct stat));
			if(stat(argv[0], &file_stat) == -1) {
				printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
				exit(-1);
			} else {
				execv(argv[0], (const char**)argv);
			}
		}
	}
}


char* argv[MAX_ARG_NR];
int32_t argc = -1;

void my_shell(void) {
	cwd_cache[0] = '/';
	while(1) {
		print_prompt();
		memset(final_path, 0, cmd_len);
		memset(cmd_line, 0, cmd_len);
		readline(cmd_line, cmd_len);
		if(cmd_line[0] == 0) {
			continue;
		}
		
		/* pipe */
		char* pipe_symbol = strchr(cmd_line, '|');
		if(pipe_symbol) {
			int32_t fd[2] = {-1};
			pipe(fd);

			/* redirect stdout to ioqueue */
			fd_redirect(1, fd[1]);
			
			/* first command */
			char* each_cmd = cmd_line;
			pipe_symbol = strchr(each_cmd, '|');
			*pipe_symbol = 0;

			argc = -1;
			argc = cmd_parse(each_cmd, argv, ' ');
			cmd_execute(argc, argv);

			each_cmd = pipe_symbol + 1;
			
			/* redirect stdin to ioqueue */
			fd_redirect(0, fd[0]);
			
			/* middle commands */
			while((pipe_symbol = strchr(each_cmd, '|'))) {
				*pipe_symbol = 0;
				argc = -1;
				argc = cmd_parse(each_cmd, argv, ' ');
				cmd_execute(argc, argv);
				each_cmd = pipe_symbol + 1;
			}
			
			/* recover stdout */
			fd_redirect(1, 1);
			
			/* last command */
			argc = -1;
			argc = cmd_parse(each_cmd, argv, ' ');
			cmd_execute(argc, argv);

			/* recover stdin */
			fd_redirect(0, 0);

			close(fd[0]);
			close(fd[1]);
		} else {
			argc = -1;
			argc = cmd_parse(cmd_line, argv, ' ');
			if(argc == -1) {
				printf("num of arguments exceed %d\n", MAX_ARG_NR);
				continue;
			}
			cmd_execute(argc, argv);
		}
		
		argc = -1;
		argc = cmd_parse(cmd_line, argv, ' ');
		if(argc == -1) {
			printf("num of arguments exceed %d.\n", MAX_ARG_NR);
			continue;
		}
	}
	panic("my_shell: should not be here!");
}
