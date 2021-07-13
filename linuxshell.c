/*
Collin Stratton
CST-315
Topic 7 Project 7: Code Errors and the Butterfly Effect
Dr. Ricardo Citro

For this project, the goal was to our own shell script in the Linux terminal

References Used:
	https://www.geeksforgeeks.org/making-linux-shell-c/
	https://brennan.io/2015/01/16/write-a-shell-in-c/
	https://iq.opengenus.org/ls-command-in-c/
*/

// directories included to be able to call specific commands
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>

#define MAXCOM 1000 // max number of letters supported for input
#define MAXLIST 100 // max number of commands supported for input

#define clear() printf("\033[H\033[J") // clear the shell using escape sequences

// shell start up text
void init_shell() {
	/*
		clear the terminal
		print introductory text
		clear the terminal again
	*/

	clear();
	printf("\n\n\n\n******************************************");
	printf("\n\n\n\t****Collin Stratton Shell (CSS)****");
	printf("\n\n\n\n******************************************");

	printf("\n");
	sleep(1);
	clear();
}

// take input from user
int takeInput(char* str) {
	char* buf;					// character buffer pointer

	buf = readline("CSS > ");	// read the text after >>> in the terminal into the buffer
	if (strlen(buf) != 0) {		// if there are characters in the buffer
		add_history(buf);			// save the text int he buffer for later
		strcpy(str, buf);			// copy buf into str
		return 0;					// return 0
	} else {
		return 1;				// else return 1
	}
}

// print the curr directory
void printDir() {
	char cwd[1024];				// cwd var with array of 1024 characters
	getcwd(cwd, sizeof(cwd));	// get the current working directory and copy it to cwd
	printf("\n%s\n", cwd);		// print the current working directory
}

// execute the command entered by the user
void execArgs(char** parsed) {
	pid_t pid = fork();			// create a child thread pid

	/*
		if pid == -1, a child thread failed to be created
		if pid == 0 and no files can be executed from parsed, exit
		else wait for the child thread to terminate and exit
	*/
	if(pid == -1) {
		printf("\nFailed forking child thread..");
		return;
	}
	else if(pid == 0) {
		if (execvp(parsed[0], parsed) < 0) {
			printf("\nCould not execute command..");
		}
		exit(0);
	} 
	else {
		wait(NULL);
		return;
	}
}

// execute piped system commands
void execArgsPiped(char** parsed, char** parsedpipe) {
	/*
		create two pipes and make sure they are initialized
		frok the first pipe to create a child thread
	*/
	int pipefd[2];
	pid_t p1, p2;

	if (pipe(pipefd) < 0) {
		printf("\nPipe could not be initialized");
		return;
	}
	p1 = fork();
	if (p1 < 0) {
		printf("\nCould not fork");
		return;
	}

	/*
		if p1 == 1 then the child thread is executing
		close the pipe and exectue the command in the child thread if available
		
		else the parent thread is excectuing
		fork the parent thread and try to execute the steps from the first if statement
			else wait for the parent and children threads to terminate
	*/
	if (p1 == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);

		if (execvp(parsed[0], parsed) < 0) {
			printf("\nCould not execute command 1..");
			exit(0);
		}
	} else {
		p2 = fork();

		if (p2 < 0) {
			printf("\nCould not fork");
			return;
		}

		if (p2 == 0) {
			close(pipefd[1]);
			dup2(pipefd[0], STDIN_FILENO);
			close(pipefd[0]);
			if (execvp(parsedpipe[0], parsedpipe) < 0) {
				printf("\nCould not execute command 2..");
				exit(0);
			}
		} else {
			wait(NULL);
			wait(NULL);
		}
	}
}

// help command
void helpCmd() {
	puts("\n***Help Doc***"
		"\nList of Commands supported:"
		"\n>cd"
		"\n>ls"
		"\n>pwd"
		"\n>exit"
		"\n>hello");

	return;
}

// exectue commands entered by the user created by me
int createdCmds(char** parsed) {
	/*
		create array with the number of custom commands
		assign the names of the commands to the spaces in the array
	*/

	int totalcmds = 5, i, switcharg = 0;
	char* cmdlist[totalcmds];
	char* username;

	cmdlist[0] = "exit";
	cmdlist[1] = "cd";
	cmdlist[2] = "help";
	cmdlist[3] = "pwd";
	cmdlist[4] = "hello";

	/*
		loop through the cmdlist and parsed list to compare when the commands match
		the switch arg is then loaded with the value for the command
		the case for the switch is called
	*/

	for (i = 0; i < totalcmds; i++) {
		if (strcmp(parsed[0], cmdlist[i]) == 0) {
			switcharg = i + 1;
			break;
		}
	}

	switch (switcharg) {
		case 1:
			printf("\nGoodbye\n");
			exit(0);
		case 2:
			chdir(parsed[1]);
			return 1;
		case 3:
			helpCmd();
			return 1;
		case 4:
			printDir();
			return 1;
		case 5:
			username = getenv("USER");
			printf("\nHello %s!\n", username);
			return 1;
		default:
			break;
	}

	return 0;
}

// find the values in the pipe
int parsePipe(char* str, char** strpiped) {
	/*
		loop through strpiped and separates the string by |
		break if strpiped is null meaning a pipe was found
	*/

	for (int i = 0; i < 2; i++) {
		strpiped[i] = strsep(&str, "|");
		if (strpiped[i] == NULL)
			break;
	}

	/*
		if strpiped is null then a pipe was found
			return 0
		else return 1
	*/

	if (strpiped[1] == NULL)
		return 0;
	else {
		return 1;
	}
}

// parse command words
void parseCmd(char* str, char** parsed) {
	/*
		separate all the words in the parsed list by " "
		if the value is null then break
		else if its 0 go back to the previous string in the parsed list
	*/

	for (int i = 0; i < MAXLIST; i++) {
		parsed[i] = strsep(&str, " ");

		if (parsed[i] == NULL)
			break;
		if (strlen(parsed[i]) == 0)
			i--;
	}
}

// process the string entered by the user
int processString(char* str, char** parsed, char** parsedpipe) {
	/*
		if strpiped is piped (indicated by parsePipe returning 1)
			parse the command in strpiped position 0 and 1
		else parse the whole string from str

		if created cmds returns 1 (meaning there is a user cmd), return 0 (so execFlag doesn't call a basic command)
		else return 1 + piped to call the basic linux shell command through execFlag
	*/

	char* strpiped[2];
	int piped = 0;

	piped = parsePipe(str, strpiped);

	if (piped) {
		parseCmd(strpiped[0], parsed);
		parseCmd(strpiped[1], parsedpipe);

	} else {
		parseCmd(str, parsed);
	}

	if (createdCmds(parsed))
		return 0;
	else
		return 1 + piped;
}

// shell loop to constantly call cmds
void sh_loop(void) {
	/*
		create char pointers to house the values entered by the user
		initialize the shell
		enter an infinite loop 
	*/

    char inputString[MAXCOM], *parsedArgs[MAXLIST];
	char* parsedArgsPiped[MAXLIST];
	int execFlag = 0;

	init_shell();

	while (1) {
		/*
			take an input from the user and process the value
		*/

		if (takeInput(inputString))
			continue;
		
		execFlag = processString(inputString, parsedArgs, parsedArgsPiped);

		/*
			execflag returns: 
				0 if there is a created command (not built in)
				1 if it is a simple command (built in)
				2 if it is included a pipe (built in)
		*/

		if (execFlag == 1)
			execArgs(parsedArgs);

		if (execFlag == 2)
			execArgsPiped(parsedArgs, parsedArgsPiped);
	}
}

// main function that calls sh_loop
int main()
{
	sh_loop();

    return 0;
}

