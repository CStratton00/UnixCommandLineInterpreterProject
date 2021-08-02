/*
Collin Stratton
CST-315
Topic 5 Project 2: Virtual Memory Manager
Dr. Ricardo Citro

For this project, the goal was to implement a virtual memory manager using paging in our shell script

References Used:
	https://www.geeksforgeeks.org/making-linux-shell-c/
	https://brennan.io/2015/01/16/write-a-shell-in-c/
	https://iq.opengenus.org/ls-command-in-c/
	https://www.geeksforgeeks.org/c-program-delete-file/
	https://github.com/azraelcrow/Virtual-Memory-Manager
*/

// directories included to be able to call specific commands
#include <alloca.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/wait.h>

// used to read the line from the terminal
#include <readline/readline.h>
#include <readline/history.h>

//Used for handling directory files
#include <dirent.h>

//For EXIT codes and error handling
#include <errno.h>

FILE *addresses;
FILE *backing_store;

typedef enum { // bool type enum for explicit bool type
    false,
    true
} bool;

struct proc {
    int id;
    int state;
    int calculationsRemaining;
    /*
    0. NEW
    1. READY
    2. WAITING
    3. RUNNING
    4. TERMINATED
    */
};

#define MAXCOM 1000 								// max number of letters supported for input
#define MAXLIST 100 								// max number of commands supported for input

#define BUFF_SIZE 10        						// buffer size for reading a line
#define ADDRESS_MASK 0xFFFF 						// mask all of logical_address except the address
#define OFFSET_MASK 0xFF    						// mask the offset
#define TLB_SIZE 16         						// 16 entries in the translation lookaside buffer
#define PAGE_TABLE_SIZE 128 						// page table of size 128
#define PAGE 256            						// upon page fault, read in 256-byte page from BACKING_STORE
#define FRAME_SIZE 256      						// size of each frame

#define clear() printf("\033[H\033[J") 				// clear the shell using escape sequences

int TLBEntries = 0;                              	// number of translation lookaside buffer entries
int hits = 0;                                    	// counter for translation lookaside buffer hits
int faults = 0;                                 	// counter for page faults
int currentPage = 0;                             	// number of pages
int logical_address;                             	// store logical address
int TLBpages[TLB_SIZE];                          	// page numbers in translation lookaside buffer
bool pagesRef[PAGE_TABLE_SIZE];                  	// reference bits for page numbers in translation lookaside buffer
int pageTableNumbers[PAGE_TABLE_SIZE];           	// page numbers in page table
char currentAddress[BUFF_SIZE];                  	// addresses
signed char fromBackingStore[PAGE];              	// reads from BACKING_STORE
signed char byte;                                	// value of physical memory at frame number/offset
int physicalMemory[PAGE_TABLE_SIZE][FRAME_SIZE]; 	// physical memory array of 32,678 bytes

void getPage(int logicaladdress);
int backingStore(int pageNum);
void TLBInsert(int pageNum, int frameNum);

void runInterrupt();
int runDummyProcess(struct proc *process);
void scheduler(int procNum, int timeoutNum, int printDetailed);

//Variables for Short term Scheduler
struct proc *processes;
int timeout;
int numProcesses;
int interrupt = 0; // 0 no interrupt, 1 interrupt


// shell start up text
void init_shell() {
	/*
		clear the terminal
		print introductory text
		clear the terminal again
	*/

	clear();
	printf("\n\n\n\n******************************************");
	printf("\n\n\n*******Collin Stratton Shell (CSS)********");
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
		add_history(buf);		// save the text int he buffer for later
		strcpy(str, buf);		// copy buf into str
		return 0;				// return 0
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

// help command
void helpCmd() {
	puts("\n***Help Doc***"
		"\nList of Commands supported:"
		"\n>cd"
		"\n>ls"
		"\n>rm"
		"\n>pwd"
		"\n>exit"
		"\n>page"
		"\n>hello"
		"\n>sproc");

	return;
}

// list all the files in the current working directory
void ls() {
	char cwd[1024];				// cwd var with array of 1024 characters
	getcwd(cwd, sizeof(cwd));	// get the current working directory and copy it to cwd

	// create variables that will store the text names of the files in the directory
	struct dirent *d;
	DIR *dh = opendir(cwd);

	/*
		if there is no directory
			if an error is recorded, print that the directory doesn't exist
			else print that the directory can't be read
		exit the command
	*/
	if (!dh) {
		if (errno == ENOENT) {
			perror("Directory doesn't exist");
		} else {
			perror("Unable to read directory");
		}
		exit(EXIT_FAILURE);
	}

	/*
		while the directory is readable
			if hidden files skip
			print all the file names in the directory
	*/
	while ((d = readdir(dh)) != NULL) {
		if (d->d_name[0] == '.') {
			continue;
		}
		printf("%s  ", d->d_name);
	}
	printf("\n");
}

void runInterrupt() { // Simluation of I/O interrupt
    printf("!!!I/O Interrupt!!!\n");
    sleep(1);
    interrupt = 0;
}

int runDummyProcess(struct proc *process) { // Simulation of process doing calculations
    if (process->calculationsRemaining > 0) {
        process->calculationsRemaining--; // subtract from total
    }
    if (process->calculationsRemaining <= 0) { // Process Completed
        return 1;
    }
    else { // Process not Completed
        return 0;
    }
}

void *printResults() { // Simplified  information display
    printf("ID: STATE:\n");
    while(processes[numProcesses-1].state != 4) {
        for (int i = 0; i < numProcesses; i++) {
            printf("%i %i\n",processes[i].id, processes[i].state);
                    
        }
        printf("---------\n");
        sleep(1);
    }
    printf("All processes Completed!\n");
    for (int i = 0; i < numProcesses; i++) {
        printf("%i %i\n",processes[i].id, processes[i].state);        
    }

	return NULL;
}

// Display with amount of time running and calculations remaining with extra details
void *printResultsDetailed() {
	/*
		print the ID, STATE, and the amount of calculation time remaining
		while there are still processes left that aren't in state 4
			get the time
			print the time
			print the state for all the processes
	*/

    time_t timeRunning;

    printf("ID: STATE: Calculations Remaining:\n");
    while(processes[numProcesses-1].state != 4) {
        time(&timeRunning);
        printf("Time Elapsed: %li\n", timeRunning);

        for (int i = 0; i < numProcesses; i++) {
            printf("%i %i %i\n",processes[i].id, processes[i].state, processes[i].calculationsRemaining);
            
        }

        printf("---------\n");
        sleep(1);
    }

    printf("All processes Completed!\n");

    for (int i = 0; i < numProcesses; i++) {
        printf("%i %i\n",processes[i].id, processes[i].state);        
    }

	return NULL;
}

// simple I/O simulation from user typing in console
void *interruptInput() { 
	/*
		while there are still processes running
			get the character typed
			set interrupt to 1
	*/
    while(processes[numProcesses-1].state != 4) {
        getchar();
        interrupt = 1;
    }

	return NULL;
}

// Round Robin code
void *RoundRobin() {
    // uses global proc struct array to simulate processes with states
    
    // declare variables
    int numRunning = 0;
    int numCompleted = 0;
    time_t timer, starttime;

    // set processes to default values
    for(int i = 0; i < numProcesses; i++) {
        processes[i].id = i; 							// new ID per proc
        processes[i].state = 0; 						// sets default state to NEW
        processes[i].calculationsRemaining = 1000000; 	// sets a default amount of calculations
    }

	// while all processes not completed
    while(numCompleted != numProcesses) { 
        for (int i = 0; i < numProcesses; i++) {
            if (processes[i].state != 4) { // only run procs that are not completed
                // if proc is NEW, set to READY
                if (processes[i].state == 0) {
                    processes[i].state = 1;
                }

                // if proc is READY and no processes are running, set to RUNNING
                if (processes[i].state == 1 && numRunning == 0) {
                    processes[i].state = 3;
                }

                // if there is an interrupt
                if (interrupt) {
                    processes[i].state = 2; // set proc to WAITING
                    runInterrupt();

                    if (numRunning == 0) { 	// if no other processes running, go directly to running state
                        processes[i].state = 3;
                    } else { 					// else go to ready state
                        processes[i].state = 1;
                    }
                }

                // if RUNNING state
                if (processes[i].state == 3) { 
                    numRunning++;
                    time(&starttime);

					// while runProcess is still returning not completed and time is not at timeout yet
                    do {
                        time(&timer);
                    } while (runDummyProcess(&processes[i]) == 0 && difftime(timer,starttime) <= timeout);
                    
                    // if process has finished processing, set it to terminated
                    if (processes[i].calculationsRemaining <= 0) {
                        processes[i].state = 4;
                        numCompleted++;
                    }
                    // otherwise set it to READY
                    else {
                        processes[i].state = 1;
                    }
                    numRunning--;
                }
            }
        }
    }

	return NULL;
}

// scheduler code that takes in the number of processes, the timeout value, and the print detailed number
void scheduler(int procNum, int timeoutNum, int printDetailed) { 
    // set initial variables
    numProcesses = procNum;
    timeout = timeoutNum;
    processes = malloc(numProcesses * sizeof(struct proc));

    // create threads
    pthread_t thread, printThread, interruptThread;
    pthread_create(&thread, NULL, &RoundRobin, NULL);
    
    // set which print method depending on user input (0 or 1)
    switch(printDetailed) {
        case 1:
            pthread_create(&printThread, NULL, &printResultsDetailed, NULL);
            break;     
        default:
            pthread_create(&printThread, NULL, &printResults, NULL);
            break;
    }
    // run interrupt simulation thread
    pthread_create(&interruptThread, NULL, &interruptInput, NULL);
    
    // join and cancel threads and free up memory
    pthread_join(thread, NULL);
    pthread_join(printThread, NULL);
    pthread_cancel(interruptThread);
    free(processes);
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
		printf("\nFailed forking child thread..\n");
		return;
	}
	else if(pid == 0) {
		if (execvp(parsed[0], parsed) < 0) {
			// printf("\nCould not execute command..\n");
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

// get the page from the logical address
void getPage(int logical_address) {
	/*
		initialize frameNum to -1
		mask leftmost 16 bits
		shift right 8 bits to extract page number
		offset is just the rightmost bits
		look through translation lookaside buffer
	*/
    int frameNum = -1;
    int pageNum = ((logical_address & ADDRESS_MASK) >> 8);
    int offset = (logical_address & OFFSET_MASK);
    
	/*
		if translation lookaside buffer hit
			extract frame number
			increase number of hits
	*/
    for (int i = 0; i < TLB_SIZE; i++) {
        if (TLBpages[i] == pageNum) {
            frameNum = i;
            hits++;
        }
    }

	/*
		if the frame number was not found in the translation lookaside buffer
			loop through all the pages
				if page number found in page table
					extract
					change reference bit
			if frame number is -1
				read from BACKING_STORE
				increase the number of page faults
				change frame number to first available frame number
	*/
    if (frameNum == -1) {
        for (int i = 0; i < currentPage; i++) {
            if (pageTableNumbers[i] == pageNum) {
                frameNum = i;
                pagesRef[i] = true;
            }
        }

        if (frameNum == -1) {
            int count = backingStore(pageNum);
            faults++;
            frameNum = count;
        }
    }

	/*
		insert page number and frame number into translation lookaside buffer
		assign the value of the signed char to byte
		output the virtual address, physical address and byte of the signed char to the console
	*/
    TLBInsert(pageNum, frameNum);
    byte = physicalMemory[frameNum][offset];
    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, (frameNum << 8) | offset, byte);
}

// read from backing store
int backingStore(int pageNum) {
    int counter = 0;

	/*
		position to read from pageNum
		read from beginning of file to find backing store
	*/
    if (fseek(backing_store, pageNum * PAGE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }
    if (fread(fromBackingStore, sizeof(signed char), PAGE, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");
    }

	/*
		search until specific page is found
			if reference bit is 0
				replace page
				set search to false to end loop
			else if reference bit is 1
				set reference bit to 0
	*/
    bool search = true;
    while (search) {
        if (currentPage == PAGE_TABLE_SIZE) {
            currentPage = 0;
        }
        if (pagesRef[currentPage] == false) {
            pageTableNumbers[currentPage] = pageNum;
            search = false;
        } else {
            pagesRef[currentPage] = false;
        }
        currentPage++;
    }
    // load contents into physical memory
    for (int i = 0; i < PAGE; i++) {
        physicalMemory[currentPage - 1][i] = fromBackingStore[i];
    }
    counter = currentPage - 1;
    return counter;
}

// insert page into translation lookaside buffer
void TLBInsert(int pageNum, int frameNum) {
	/*
		search for entry in translation lookaside buffer
	*/
    int i;
    for (i = 0; i < TLBEntries; i++) {
        if (TLBpages[i] == pageNum) {
            break;
        }
    }

	/*
		if the number of entries is equal to the index
			if TLB is not full
				insert page with FIFO replacement
			else
				shift everything over
				replace first in first out
		else the number of entries is not equal to the index
			loop over everything to move the number of entries back 1
			if still room in TLB
				insert page at the end
			else if TLB is full
				place page at number of entries - 1
	*/
    if (i == TLBEntries) {
        if (TLBEntries < TLB_SIZE) {
            TLBpages[TLBEntries] = pageNum;
        }
        else {
            for (i = 0; i < TLB_SIZE - 1; i++) {
                TLBpages[i] = TLBpages[i + 1];
            }
            TLBpages[TLBEntries - 1] = pageNum;
        }
    } else {
        for (i = i; i < TLBEntries - 1; i++) {
            TLBpages[i] = TLBpages[i + 1];
        }
        if (TLBEntries < TLB_SIZE) { 
            TLBpages[TLBEntries] = pageNum;
        } else {
            TLBpages[TLBEntries - 1] = pageNum;
        }
    }

    // if TLB is still not full, increment the number of entries
    if (TLBEntries < TLB_SIZE) {
        TLBEntries++;
    }
}

// runner for the thread
void *runner(void *arg) {
    system((char *)arg); // convert arg to string and runs it
	return NULL;
}

// grabs the file from the inputted value
void fromFile(char *fileName) {
	/*
		create variables store
			file name
			chars in the line
			length of the list of chars
			read length
	*/
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

	/*
		open the inputted file
		while the file is open
			split string by ;
			create threads for each token
				split the string by ; in each thread
			join all the threads together
	*/
    fp = fopen(fileName, "r");

    while ((read = getline(&line, &len, fp)) != -1) {
        char *token = strtok(line, ";");
        pthread_t tid[10];
        int i = 0;

        while (token != NULL) {
            pthread_create(&tid[i++], NULL, runner, token);
            token = strtok(NULL, ";");
        }

        for (int j = 0; j < i; j++) {
            pthread_join(tid[j], NULL);
        }
    }

	/*
		close the file 
		free the line on the console
	*/
    fclose(fp);
    if (line)
        free(line);
}

// exectue commands entered by the user created by me
int createdCmds(char** parsed) {
	/*
		create array with the number of custom commands
		assign the names of the commands to the spaces in the array
	*/

	int totalcmds = 9, i, switcharg = 0;
	char* cmdlist[totalcmds];
	char* username;
	int x, y, z;

	cmdlist[0] = "exit";
	cmdlist[1] = "cd";
	cmdlist[2] = "help";
	cmdlist[3] = "pwd";
	cmdlist[4] = "hello";
	cmdlist[5] = "ls";
	cmdlist[6] = "rm";
	cmdlist[7] = "page";
	cmdlist[8] = "sproc";

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
		case 6:
			ls();
			return 1;
		case 7:
			remove(parsed[1]);
			return 1;
		case 8:
			fromFile(parsed[1]);
			return 1;
		case 9:
			if (parsed[3]) {
				sscanf(parsed[1], "%d", &x);
				sscanf(parsed[2], "%d", &y);
				sscanf(parsed[3], "%d", &z);

				scheduler(x, y, z);
			} else if (parsed[2]) {
				sscanf(parsed[1], "%d", &x);
				sscanf(parsed[2], "%d", &y);

				scheduler(x, y, 0);
			} else if (parsed[1]) {
				sscanf(parsed[1], "%d", &x);

				scheduler(x, 2, 0);
			} else {
				printf("ERROR, Example Format: procs (num processes) (timeout) (detailed list 1/0)\n");
			}
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
int main() {
	sh_loop();

    return 0;
}
