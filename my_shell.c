#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

int executecommands(char* argv[], int argc, int fd);

//Initialising file descriptors.
int setfd(int* fd){
	//fd[0] = read
	//fd[1] = write
	//fd[2] = error
	while((*fd = open("console", O_RDWR)) >= 0) {
		if (*fd >= 3){
			close(*fd);
			break;
		}
	}
	return *fd;
}


//Iterates through cmd and puts commands into array.
int retrievecommands(char *cmd, char *argv[]){
	int argc = 0;
	while(1){
		//handling cmd
		//Null terminates white spaces, tabs and new lines.
		while(*cmd == ' ' || *cmd == '\t' || *cmd == '\n'){
			*cmd++ = 0;
		}
		if (*cmd == 0){
			break;
		}

		
		//Ensures that words within quotation marks are counted
		//as one argument.
		if (*cmd == '"') {
			argv[argc] = ++cmd;
			while (*cmd && *cmd != '"'){
				cmd++;
			}
			if (*cmd == '"'){
				*cmd++ = 0;
			}
		}
		
		else{
			argv[argc] = cmd;
			while (*cmd && *cmd != ' ' && *cmd != '\t' && *cmd != '\n'){
				cmd++;
			}
		}
		
		argc++;
	}
	
	argv[argc] = 0;
	return argc;
}

//Input/Output redirection.
int redirection(char* argv[], int argc, int fd){
	for (int i = 0; i < argc; i++){
		if (strcmp(argv[i], ">") == 0){
			if (argv[i+1] == 0){
				fprintf(2, "No file name provided.\n");
				exit(1);
			}
			fd = open(argv[i+1], O_WRONLY | O_CREATE);
			if (fd < 0){
				fprintf(2, "Cannot open file %s.\n");
				exit(1);
			}
			argv[i] = 0;
			argv[i + 1] = 0;
			close(1);
			dup(fd);
			close(fd);
			break;
		}
		else if (strcmp(argv[i], "<") == 0){
			if (argv[i+1] == 0){
				fprintf(2, "No file name provided.\n");
				exit(1);
			}
			fd = open(argv[i+1], O_RDONLY);
			if (fd < 0){
				fprintf(2, "Cannot open file %s.\n", argv[i+1]);
				exit(1);
			}
			argv[i] = 0;
			argv[i+1] = 0;
			close(0);
			dup(fd);
			close(fd);
			break;
		}
	}
	return 0;	
}

//Pipes
int pipes(char* argv[], int argc, int fd){

	char *leftpipe[50];
	char *rightpipe[50];
	int index = 0;
	int i;
	int p[2];
	int num_pipes = 0;


	if (pipe(p) < 0){
		printf("Pipe error.\n");
		exit(1);
	}

	for (i = 0; i < argc; i++ ){
		if (strcmp(argv[i], "|") == 0){
			index = i;
			break;
		}
	}
	for (i = 0; i < index; i++){
		leftpipe[i] = argv[i];
	}
	//Null terminating position where '|' used to be.
	leftpipe[i] = 0;

	int indexafterpipe = 0;
	for (i = index+1; i < argc; i++){
		rightpipe[indexafterpipe++] = argv[i];
	}
	rightpipe[indexafterpipe] = 0;
	

	//executes left side until pipe is reached.
	if (fork() == 0){
		close(1);
		dup(p[1]);
		close(p[0]);
		close(p[1]);
		exec(leftpipe[0], leftpipe);
	}
	if (fork() == 0){
		close(0);
		dup(p[0]);
		close(p[0]);
		close(p[1]);
		int rightpipelength = 0;
		while (rightpipe[rightpipelength] != 0){
			rightpipelength++;
		}

		//If more than 2 pipes available then use 
		//recursion for the other pipes
		if (num_pipes > 1){
			pipes(rightpipe, rightpipelength, fd);
		}
		else{
			executecommands(rightpipe, rightpipelength, fd);
		}
	}
	
	close(p[0]);
	close(p[1]);
	wait(0);
	wait(0);
	exit(0);


	return 0;
}

int listexec(char* argv[], int argc, int fd){
	char *leftpipe[40];
	char *rightpipe[40];
	int index = 0;
	int i;
	int p[2];

	if (pipe(p) < 0){
		printf("error.\n");
	}



	for (i = 0; i < argc; i++ ){
		if (strcmp(argv[i], ";") == 0){
			index = i;
			break;
		}
	}
	for (i = 0; i < index; i++){
		leftpipe[i] = argv[i];
	}
	//Null terminating position where '|' used to be.
	leftpipe[i] = 0;

	int indexafterpipe = 0;
	for (i = index+1; i < argc; i++){
		rightpipe[indexafterpipe++] = argv[i];
	}
	rightpipe[indexafterpipe] = 0;
	

	int  r_length = 0;
	int r_count = 0;
	while (rightpipe[r_length] != 0){
		r_length++;
		r_count++;
	}

	int l_length = 0;
	int l_count = 0;
	while (leftpipe[l_length] != 0){
		l_length++;
		l_count++;
	}

	if (fork() == 0){
		exec(leftpipe[0], leftpipe);
	}
	if (fork() > 0){
		wait(0);
		exec(rightpipe[0],rightpipe);
	}

	return 0;
}

//Execute commands
int executecommands(char* argv[], int argc, int fd){
	if (argc > 0){
		for (int i = 0; i < argc; i++){
			if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "<") == 0) {
				redirection(argv, argc, fd);
			}
			if (strcmp(argv[i], "|") == 0){
				//Pipes
				pipes(argv, argc, fd);
			}
			if (strcmp(argv[i], ";") == 0){
				//list
				listexec(argv, argc, fd);
			}

		}
		if (exec(argv[0], argv) < 0){
			printf("Cannot execute the command %s\n.", argv[0]);
			exit(1);
		}
	}
	return 0;
}

int main(void){
	char buf[512];
	int fd;
	//Opening file descriptor.
	setfd(&fd);

	while(1){
		// Reads user entry into buf.
		printf(">>> ");
		memset(buf, 0, sizeof(buf));
		gets(buf, sizeof(buf));
		if (strcmp(buf, "exit()\n") == 0){
			exit(0);
		}
		//handles cd.
		if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
			buf[strlen(buf)-1] = 0;
			if (chdir(buf+3)){
				fprintf(2, "cannot cd\n");
				exit(0);
			}
		}
		else{
		
			int pid = fork();
			if (pid < 0){
				printf("Fork error.\n");
			}
			else if(pid == 0) { //Child.
				char *argv[32];
				int argc = retrievecommands(buf, argv);
				executecommands(argv, argc, fd);
			}
			else { //Parent.
				wait(0);
			}
		}
	}
	exit(0);
}
