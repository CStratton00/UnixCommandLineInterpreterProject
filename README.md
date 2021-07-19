# UnixCommandLineInterpreterProject
CST-315 project to create a unix/linux command line interpreter

# Installation
1. Download zip file of code
2. Download gcc
3. Make sure the following libaries are installed
   - stdio.h
   - string.h
   - stdlib.h
   - unistd.h
   - sys/types.h
   - sys/wait.h
   - readline/readline.h
   - readline/history.h
   - dirent.h
   - errno.h
5. Open zip file and note the location of the .c file
6. Open terminal or commandline equivalent and go to the .c file
7. Run `gcc linuxshell.c -lreadline -o linuxshell`
8. Run `./linuxshell`

# Terminal Comands Available
1. exit
2. cd
3. ls
4. rm
6. pwd
5. help
7. hello
