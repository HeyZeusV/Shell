/****************************************************************
 * Name        : Jesus Valdes                                   *
 * Class       : CSC 415                                        *
 * Date        : 10/24/18                                       *
 * Description :  Writting a simple bash shell program          *
 *                that will execute simple commands. The main   *
 *                goal of the assignment is working with        *
 *                fork, pipes and exec system calls.            *
 ****************************************************************/

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAXCOMMAND 16
#define BUFFERSIZE 256
#define PROMPT "myShell"
#define PROMPTSIZE sizeof(PROMPT)
#define USERNAME getenv("USER")

// built in cd
int cd(char *newDir) {

	return chdir(newDir);
}

// built in pwd
void pwd () {

	char dir[512];
	getcwd(dir, sizeof(dir));
	printf("%s", dir);
}

// prints out directory 
void curDirectory() {

	printf("\n%s ", PROMPT);
	char homeDir[512];
	getcwd(homeDir, sizeof(homeDir));
	char *token;
	char *delimiter = "/";
	token = strtok(homeDir, delimiter);
	// since '/' is delimiter, should return NULL if in root directory
	// this avoids a segmentation fault 
	if (token == NULL) {

		printf("/");
	// in home directory at this step
	} else if (strcmp("home", token) == 0) {

		// prints /home if next token is NULL
		token = strtok(NULL, delimiter); 
		if (token == NULL) {

			printf("/home");
		// prints out ~/... if in USER home directory
		} else if (strcmp(USERNAME, token) == 0) {

			printf("~");  
			token = strtok(NULL, delimiter);
			if (token == NULL) {

				printf("/");
			}
			while (token != NULL) {

				printf("/%s", token);
				token = strtok(NULL, delimiter);
			}
		// this is when in a different user directory than the one signed in
		} else {

			printf("/home");
			token = strtok(NULL, delimiter);
			while (token != NULL) {

				printf("/%s", token);
				token = strtok(NULL, delimiter);
			}
		}
	// anything other than /home
	} else {

		while (token != NULL) {

			printf("/%s", token);
			token = strtok(NULL, delimiter);
		}
	}
	printf(" >> ");
}

// takes user input using readline
void user_input(char* input) {

	char* buffer;
	buffer = readline("");
	if (strlen(buffer) != 0) {

		add_history(buffer);
		strcpy(input, buffer);
		free(buffer);
	} else {

		printf("Please type out a command!\n");
	}
}

// this is to check if there will be any pipes in the command
// limited to one pipe per command 
int pipeTok(char* input, char** pipedInput) {

	for (int i = 0; i < 2; i++) {

		pipedInput[i] = strsep(&input, "|");
		if (pipedInput[i] == NULL) {

			break;
		}
	}
	if (pipedInput[1] == NULL) {

		return 0;
	} else {

		return 1;
	}
}

// chops up the user input using string tokenizer
void inputTok(char* input, char** parsedInput, bool *background, int *process, char** file) {

	char *token;
	char *delimiter = " ";
	int i = 0;
	bool ignore = false;

	token = strtok(input, delimiter);
	while (token != NULL) {

		// special character detection
		// for running background process 
		if (*token == '&') {

			*background = true;
			token = strtok(NULL, delimiter);
		// next 3 are for redirection
		} else if (*token == '<') {

			*process = 1;
			token = strtok(NULL, delimiter);
			ignore = true;
		} else if (strcmp(">>", token) == 0) {

			*process = 4;
			token = strtok(NULL, delimiter);
			ignore = true;
		} else if (*token == '>') {

			*process = 3;
			token = strtok(NULL, delimiter);
			ignore = true;
		// if there is redirection, then this ignores the file name to redirect to
		// instead it places it in a different char* to be used later
		} else if (ignore) {

			*file = token;
			token = strtok(NULL, delimiter);
			ignore = false;
		// everything else is added to be executed later
		} else {

			parsedInput[i] = token;
			i++;
			token = strtok(NULL, delimiter);
		}
	}
	// execvp expects last argument to be NULL
	parsedInput[i] = NULL;
}

// checks to see if there is pipes
// if there is then we need 2 char*, if not then just send input straight to inputTok
int inputProcessor(char* input, char** parsedInput, char** parsedPipedInput, bool *background, int *process, char** file) {

	char* pipedInput[2];
	int numPipes = 0;

	numPipes = pipeTok(input, pipedInput);

	if (numPipes) {

		inputTok(pipedInput[0], parsedInput, background, process, file);
		inputTok(pipedInput[1], parsedPipedInput, background, process, file);

	} else {

		inputTok(input, parsedInput, background, process, file);
	} 
	return 0 + numPipes;
}

// used when redirection is detected
void ioRedirection(char** parsedInput, int process, char* file) { 

	// opens the file, then copies the file's file descriptor into In/Out so that the command read/writes the file
	// <
	if (process == 1) {

		int fd1 = open(file, O_RDONLY);
		dup2(fd1, STDIN_FILENO);
		close(fd1);
	// >
	} else if (process == 3) {
		
		int fd3 = open(file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		dup2(fd3, STDOUT_FILENO);
		close(fd3);
	// >> 
	} else if (process == 4) {

		int fd4 = open(file, O_WRONLY | O_APPEND | O_CREAT, 0644);
		dup2(fd4, STDOUT_FILENO);
		close(fd4);
	}

	execvp(parsedInput[0], parsedInput);
	perror("Something went wrong...");
	exit(1);
}

// used when there are no pipes
void commands(char** parsedInput, bool background, int process, char* file) {
 
	int stat_loc;
	pid_t child_pid = fork();
	if (child_pid < 0) {

		perror("Fork failed!");
		exit(1);
	}
	if (child_pid == 0) {

		if (process > 0) {

			ioRedirection(parsedInput, process, file);
		} else {

			execvp(parsedInput[0], parsedInput);
			perror("You messed up somewhere...");
			exit(1);
		}
	} else {

		if (!background) {

			waitpid(child_pid, &stat_loc, WUNTRACED);
		}
	}
}

// pipe commands here
void pipedCommands(char** parsedInput, char** parsedPipedInput, bool background, int process, char* file) { 

    // 0 = read
    // 1 = write
    int stat_loc;
    int pipefd[2];  
    pid_t p1, p2; 
  
  	// pipe creation
    if (pipe(pipefd) < 0) { 

        printf("\nPipe could not be initialized"); 
        return; 
    } 

    // 1st process
    p1 = fork(); 
    if (p1 < 0) { 

        perror("Fork failed!");
		exit(1);
    } 
  
    if (p1 == 0) { 

    	// connects stdout to write 
    	// closes read end of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); 
  
  		if (process > 0) {

  			ioRedirection(parsedInput, process, file);
  		} else {

        	execvp(parsedInput[0], parsedInput);
            perror("1st process failed...");
            exit(0); 
        } 
    } 

    // 2nd process
    p2 = fork(); 
  
    if (p2 < 0) { 

        perror("Fork failed!");
		exit(1);
    } 

    if (p2 == 0) { 

    	// connects stdin to read
    	// closes write end of pipe
		dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); 

        if (process > 0) {

        	ioRedirection(parsedInput, process, file);
        } else {

       		execvp(parsedPipedInput[0], parsedPipedInput);
            perror("2nd process failed...");
            exit(0); 
        } 
    }
    // close both ends of the pipe
    close( pipefd[0] );
	close( pipefd[1] );
    // used to wait for children
    if (!background) {

    	waitpid(p1, &stat_loc, WUNTRACED);
   		waitpid(p2, &stat_loc, WUNTRACED);
	}
} 

int main() {

	char input[BUFFERSIZE], *file;
	char *parsedInput[MAXCOMMAND], *parsedPipedInput[MAXCOMMAND];
	int numPipes;
	int process;
	bool background; 
	using_history();


	printf("\nEnter 'quit' to exit...\nUse arrow keys for history...");

	while (1) {
 	
 		process = 0;
 		background = false;
		curDirectory();
		user_input(input);

		// allows user to quit whenever
		if (strcmp("quit", input) == 0) {

			exit(0);
		}

		numPipes = inputProcessor(input, parsedInput, parsedPipedInput, &background, &process, &file); 
		
		// built in cd 
		if (strcmp(parsedInput[0], "cd") == 0) {

            if (cd(parsedInput[1]) < 0) {

                perror(parsedInput[1]);
            }

            /* Skip the fork */
            continue;
        }

        // built in pwd
        if (strcmp(parsedInput[0], "pwd") == 0) {
            
        	pwd();
            /* Skip the fork */
            continue;
        }

     
        if (numPipes >= 1) {

			pipedCommands(parsedInput, parsedPipedInput, background, process, file);
		} 

        if (numPipes == 0) {

        	commands(parsedInput, background, process, file);
        }        
	}
	return 0; 	
}
