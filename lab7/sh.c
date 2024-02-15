/*
 * Copyright (C) 2014-2017 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2017-2018 Xavier Ruppen <xavier.ruppen@heig-vd.ch>
 * Copyright (C) 2017 Alexandre Malki <alexandre.malki@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

#define TOKEN_NR	10
#define ARGS_MAX	16
#define FILE_MAX	30
#define TOKEN_MAX	80

char tokens[TOKEN_NR][TOKEN_MAX];
char prompt[] = "so3% ";

void parse_token(char *str) {
	int i = 0;
	char *next_token;

	next_token = strtok(str, " ");
	if (!next_token)
		return;

	strcpy(tokens[i++], next_token);

	while ((next_token = strtok(NULL, " ")) != NULL)
		strcpy(tokens[i++], next_token);
}

/*
 * Process the command with the different tokens
 */
void process_cmd(void) {
	int i;
	char *argv[ARGS_MAX], *argv2[ARGS_MAX];
	char filename[FILE_MAX];
	int pid, sig;
	int is_pipe = 0;
	int pipe_fd[2];
	int arg1 = 0;
	int arg2 = 0;

	/* PRIVATE to SYE team */

	if (!strcmp(tokens[0], "dumpsched")) {
		sys_info(1, 0);
		return ;
	}

	if (!strcmp(tokens[0], "dumpproc")) {
		sys_info(4, 0);
		return ;
	}

	if (!strcmp(tokens[0], "exit")) {
		if (getpid() == 1) {
			printf("The shell root process can not be terminated...\n");
			return ;
		} else
			exit(0);

		/* If the shell is the root shell, there is a failure on exit() */
		return ;
	}

	/* end of PRIVATE */

	/* setenv */
	if (!strcmp(tokens[0], "setenv")) {
		/* second arg present ? */
		if (tokens[1][0] != 0) {
			/* third arg present ? */
			if (tokens[2][0] != 0) {
				/* Set the env. var. (always overwrite) */
				setenv(tokens[1], tokens[2], 1);
			} else
				unsetenv(tokens[1]);
		}
		return ;
	}

	/* env */
	if (!strcmp(tokens[0], "env")) {
		/* This function print the environment vars */
		for (i = 0; __environ[i] != NULL; i++)
			printf("%s\n", __environ[i]);

		return ;
	}

	/* kill */
	if (!strcmp(tokens[0], "kill")) {
		/* Send a signal to a process */
		sig = 0;

		if (tokens[2][0] == 0) {
			sig = SIGTERM;
			pid = atoi(tokens[1]);
		} else {
			if (!strcmp(tokens[1], "-USR1")) {
				sig = SIGUSR1;
				pid = atoi(tokens[2]);
			} else if (!strcmp(tokens[1], "-9")) {
				sig = SIGKILL;
				pid = atoi(tokens[2]);
			}
		}

		kill(pid, sig);

		return ;
	}

	/* General case - prepare to launch the application */

	/* Prepare the arguments to be passed to exec() syscall */
	while (tokens[arg1][0] != 0) {

		if (!strcmp(tokens[arg1], "|")) {
			is_pipe = 1;
			argv[arg1] = NULL;
		} else {
			if (is_pipe) {
				argv2[arg2] = tokens[arg1];
				arg2++;
			} else
				argv[arg1] = tokens[arg1];
		}
		arg1++;
	}

	/* Terminate the list of args properly */
	if (is_pipe)
		argv2[arg2] = NULL;
	else
		argv[arg1] = NULL;

	if(is_pipe){
		// Before forking, set up the pipe if char detected
		if (pipe(pipe_fd) == -1) {
			printf("Pipe child 1 failed.\n");
			exit(EXIT_FAILURE);
		}
	}

	// Always fork the first child (to handle the left-hand command)
	int pid_child1 = fork();
	if (pid_child1 == -1) {
		printf("Fork child 1 failed.\n");
		if(is_pipe) {
        	close(pipe_fd[0]);
        	close(pipe_fd[1]);
    	}

		exit(EXIT_FAILURE);
	}

	if (pid_child1 == 0) {

		// Child 1 code
		if(is_pipe){
			//Pipe management only if we detected the pipe char

			// Close unused read-end
			close(pipe_fd[0]);

			// Redirect standard output to the pipe's write-end
			dup2(pipe_fd[1], STDOUT_FILENO);
		}
		
		// Always execute the first command
		strncpy(filename, argv[0], FILE_MAX);
		strcat(filename, ".elf");

		if (execv(filename, argv) == -1) {

			if(!is_pipe){
				// Write in stdout only if no pipe redirection
				printf("%s: exec failed.\n", argv[0]);
			} else {
				close(pipe_fd[1]);
			}

			exit(EXIT_FAILURE);
		}
	}

	// Wait for the first child
	waitpid(pid_child1, NULL, 0);

	if(is_pipe){

		// Fork the second child (to handle the right-hand command) if pipe detected
		int pid_child2 = fork();
		if (pid_child2 == -1) {
			printf("Child2 fork failed.\n");
			exit(EXIT_FAILURE);
		}

		// Child2's code
		if (pid_child2 == 0) {
			
			// Close unused write-end
			close(pipe_fd[1]); 

			// Redirect standard input from the pipe's read-end
			dup2(pipe_fd[0], STDIN_FILENO);

			// Execute the right-hand command
			strncpy(filename, argv2[0], FILE_MAX);
			strcat(filename, ".elf");

			if (execv(filename, argv2) == -1) {
				printf("%s: exec failed.\n", argv2[0]);
				close(pipe_fd[0]);
				exit(EXIT_FAILURE);
			}

		} else {

			// Parent's code in case of pipe char detected

			// Parent closes both ends of the pipe, it no longer needs these
			close(pipe_fd[0]);
			close(pipe_fd[1]);

			// wait for the second child
			waitpid(pid_child2, NULL, 0);
		}
	}
}

/*
 * Ignore the SIGINT signal, but we re-display the prompt to be elegant ;-)
 */
void sigint_sh_handler(int sig) {

	printf("%s", prompt);
	fflush(stdout);
}
/*
 * Main entry point of the shell application.
 */
void main(int argc, char *argv[])
{
	char user_input[TOKEN_MAX];
	int i;
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));

	sa.sa_handler = sigint_sh_handler;
	sigaction(SIGINT, &sa, NULL);

	while (1) {
		/* Reset all tokens */
		for (i = 0; i < TOKEN_NR; i++)
			tokens[i][0] = 0;

		printf("%s", prompt);
		fflush(stdout);

		gets(user_input);

		if (strcmp(user_input, ""))
			parse_token(user_input);

		/* Check if there is at least one token to be processed */
		if (tokens[0][0] != 0)
			process_cmd();


	}
}
