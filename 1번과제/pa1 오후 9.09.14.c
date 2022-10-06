/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"
#include <sys/wait.h>


/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */

struct entry {
   struct list_head list;
   char *string;
};

LIST_HEAD(history);

/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */

static void command_div(char* tokens[],int start, int end, char*new_tokens[])
{

    int idx = 0;
    
    for(int i =start; i<end;i++)
    {
        
        new_tokens[i] = malloc(sizeof(char) * MAX_COMMAND_LEN);
        new_tokens[idx] = tokens[i];
        idx++;
    }
    
    
}

static int run_command(int nr_tokens, char *tokens[])
{
    
    int pipe_check = 0; /*pipe 유무 확인*/
    int fd[2] = {0,0};
    int pipe_pt = 0;

    for(pipe_pt = 0; pipe_pt < nr_tokens; pipe_pt++)
    {
        if(strcmp(tokens[pipe_pt],"|")==0)
        {
            pipe_check = 1;
            break;
        }
    }
    
    
    if(pipe_check != 1) /*pipe 가 없는 그냥 command 일 때 */
    {
        if (strcmp(tokens[0], "exit") == 0) return 0; /*exit*/
        else if(strcmp(tokens[0],"cd") == 0){ /*cd command*/
            if(nr_tokens == 1 || (nr_tokens == 2 && strcmp(tokens[1],"~") == 0))
            {
                chdir(getenv("HOME"));
                return 1;
            }
            else if(nr_tokens == 2)
            {
                chdir(tokens[1]);
                return 1;
            }
            
            return -EINVAL;
        }
        
        else if(strcmp(tokens[0],"history") == 0){ /*history command*/
            struct entry*new_entry;
            int index = 0;
            list_for_each_entry(new_entry,&history,list)
            {
                
                fprintf(stderr,"%2d: %s",index,new_entry->string);
                index++;
                
            }
            
            return 1;
        }
        else if(strcmp(tokens[0], "!") == 0){  /* ! history command*/
            int command_num = atoi(tokens[1]);
            struct entry*new_entry_search;
            int index_entry = 0;
            
            list_for_each_entry(new_entry_search,&history, list)
            {
                if(index_entry == command_num)
                {
                    int new_nr_tokens_ = 0;
                    char *new_tokens[MAX_NR_TOKENS] = { NULL };
                    char* new_command = malloc(sizeof(char) * MAX_COMMAND_LEN);
                    strcpy(new_command, new_entry_search->string);
                    parse_command(new_command, &new_nr_tokens_, new_tokens);
                    run_command(new_nr_tokens_,new_tokens);
                    return 1;
                    
                }
                else{
                    index_entry+=1;
                }
                
            }
            return 1;
        }
        else /* run command */
        {
            
            int pid_exec = 0;
            pid_t pid_fork = fork();
            if(pid_fork < 0)
            {
                fprintf(stderr, "Unable to execute %s\n", tokens[0]);
                return -EINVAL;
            }
            else if(pid_fork == 0) /*fork 성공*/
            {
                pid_exec = execvp(tokens[0],tokens);
                if(pid_exec == -1)
                {
                    fprintf(stderr, "Unable to execute %s\n", tokens[0]);
                    return -EINVAL;
                }
                return 1; /*exec 성공*/
            }
            else{
                wait(NULL);
            }
        }
    }
    /*pipe line 있는 경우*/
    pid_t p1 = 0;
    pid_t p2 = 0;
    char*args[MAX_NR_TOKENS] = {NULL};
    char*argv[MAX_NR_TOKENS] =  {NULL};
    int start = 0;
    int end = pipe_pt;
    
    
    command_div(tokens, start, end , argv);
    
    
    command_div(tokens, pipe_pt+1, nr_tokens , args);
    
    
    
    
    if(pipe_check == 1) /*pipe exist*/
    {
        
        
        if(pipe(fd) == -1) /*pipe에 file descriptor 넣어주기*/
        {
            fprintf(stderr,"filedesciptor - ");
            fprintf(stderr, "Unable to execute %s\n", tokens[0]);
            return -EINVAL;
        }
        
        p1 = fork();
        if(p1== -1 )
            
        {
            fprintf(stderr, "Unable to execute %s\n", tokens[0]);
            return -EINVAL;
        }
        else if(p1 == 0) /*fork 성공 child process*/
        {
            

            dup2(fd[1],1);
            close(fd[0]);
            
            run_command(start+1 , argv);

            
        }
        else{
            wait(NULL);
        }
        
        p2 = fork();
        
        if(p2 < 0)
        {
            fprintf(stderr, "Unable to execute %s\n", tokens[0]);
            return -EINVAL;
        }
        else if(p2 == 0) /*fork 성공*/
        {
            
            
            dup2(fd[0],0);
            close(fd[1]);
            run_command(nr_tokens -pipe_pt + 1 , args);
            
        }
        else{
            wait(NULL);
        }
        
    }
	
    return 1;
    
}





/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char * const command)
{
    struct entry *return_value = (struct entry*)malloc(sizeof(struct entry));
    return_value->string = (char *)malloc(strlen(command)+2); /* \0 포함🤦🏻‍♀️ */
    strcpy(return_value->string,command);
    
    list_add_tail(&return_value->list,&history);
}


/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{
    
}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;
        
		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
