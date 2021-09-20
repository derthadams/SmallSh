/*******************************************************************************
 * Author:      Derth Adams
 * Date:        July 21, 2020
 * Filename:    smallsh.c
 *
 * Description: Shell program that implements three built-in commands (cd,
 * status, and exit) and for all other commands forks a child process and
 * executes the installed Linux command. User can redirect stdin with
 * < [filename], redirect stdout with > [filename], and run a command in the
 * background by using & as the final argument. User can toggle
 * foreground-only mode on and off with Ctrl-Z. In foreground-only mode,
 * execution of background processes is disabled and a final & argument will
 * be ignored. Displays the PID of a background process when it begins execution
 * and displays the exit code or signal number when it terminates. If a signal
 * kills a foreground process, the signal number is displayed.
 ******************************************************************************/

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CMD_CHARS 2048      // Max number of characters in a command line
#define MAX_ARGS 512            // Max number of arguments in a command
#define PROMPT ": "             // Character used to prompt the user
#define COMMENT_PREFIX '#'      // Character that indicates a comment line
#define PID_EXPAND_CHAR '$'     // A set of two of these expands into the PID
#define BACKGROUND_STR "&"      // Character used to indicate background process
#define MAX_PID_CHARS 20        // Max number of characters in a PID
#define INPUT_REDIRECT "<"      // Character used for stdin redirection
#define OUTPUT_REDIRECT ">"     // Character used for stdout redirection

bool foreground_only = false;   // Indicates foreground-only mode in effect

/*******************************************************************************
 * Struct name:     Command
 * Description:     Represents a command input by the user
 *
 * Members:         char* args      Array of arguments input by the user
 *                  char* line      The line of command text input by the user
 *                  int numArgs     Number of arguments in the args array
 *                  bool background True if the command was run as a background
 *                                  process, false if not.
 ******************************************************************************/

typedef struct Command {
    char **args;
    char *line;
    int numArgs;
    bool background;
} Command;

void promptLoop(Command *command);
void parseCommandLine(Command *command);
void expandPID(char **argument);
int getPIDString(char **pidStr);
void printExitValOrSignal(int exitStatus);
int executeCommand(Command *command);
void redirect(Command* command);
void checkBackgroundChildren();
void catchSIGTSTP(int signo);

/*******************************************************************************
 * Function name:   void initCommand(Command *command)
 *
 * Description:     Initializes a Command struct.
 *
 * Preconditions:   Command struct is uninitialized
 *
 * Postconditions:  Memory has been allocated for the args char pointer array
 *                  and all pointers are set to NULL, numArgs = 0, and
 *                  background = false.
 *
 * Receives:        command     Command struct pointer
 ******************************************************************************/

void initCommand(Command *command) {
    // Allocate memory for args and set pointers to NULL
    command->args = malloc(sizeof(char*) * MAX_ARGS + 1);
    for(int i = 0; i < MAX_ARGS + 1; i++) {
        command->args[i] = NULL;
    }
    // Set all other struct members to defaults
    command->line = NULL;
    command->numArgs = 0;
    command->background = false;
}

/*******************************************************************************
 * Function name:   void freeCommand(Command *command)
 *
 * Description:     Frees the memory allocated for members of a Command struct
 *
 * Postconditions:  All memory allocated for args and line members has been
 *                  freed.
 *
 * Receives:        command     Command struct pointer
 ******************************************************************************/

void freeCommand(Command *command) {
    // Free and set to NULL all args pointers that aren't already NULL
    int i = 0;
    while(command->args[i]) {
        free(command->args[i]);
        command->args[i] = NULL;
        i++;
    }
    // Free the args array and command line
    free(command->args);
    if(command->line) {
        free(command->line);
        command->line = NULL;
    }
}

/*******************************************************************************
 * Function name:   void promptLoop(Command *command)
 *
 * Description:     Prompts user for input. Ignores input that is blank,
 *                  represents a comment, or has more characters than the limit.
 *                  Parses the input and executes the command, then prompts the
 *                  user again in a loop until the user runs the exit command.
 *
 * Receives:        command     Command struct pointer
 ******************************************************************************/

void promptLoop(Command *command) {
    size_t len;             // Size of initial buffer
    ssize_t chars_read;     // Number of characters read by getline()
    int returnStatus = 0;   // Return value of executeCommand()
    // -1 means the user typed in the exit command
    // Prompt user
    while(true) {
        do {
            checkBackgroundChildren();  // Check for terminated bg children
            printf("%s", PROMPT);
            fflush(stdout);
            chars_read = getline(&command->line, &len, stdin);
            // Handle error if getline() is interrupted by a signal
            if(chars_read == -1) {
                clearerr(stdin);
                continue;
            }
            command->line[strcspn(command->line, "\n")] = 0;
        } while(command->line[0] == COMMENT_PREFIX || strlen(command->line) == 0
                || chars_read > MAX_CMD_CHARS + 1);

        // Parse and execute command
        parseCommandLine(command);
        returnStatus = executeCommand(command);
        freeCommand(command);

        // If user typed exit, quit the program by returning to main.
        if(returnStatus == -1) {
            free(command);
            return;
        }

        // Otherwise, initialize the Command struct to prepare for more input
        initCommand(command);
    }
}

/*******************************************************************************
 * Function name:   void parseCommandLine(Command *command)
 *
 * Description:     Parses the line entered by the user into an array of tokens
 *                  which is stored in command->args.
 *
 * Preconditions:   command->line contains user input
 *
 * Postconditions:  command->args contains parsed user input
 *
 * Receives:        command     Command struct pointer
 ******************************************************************************/

void parseCommandLine(Command *command) {
    int i = 0;                                      // Index for command->args
    char* token = strtok(command->line, " \n");     // Get first token

    // Get remaining tokens and copy them into command->args
    while(token && i < MAX_ARGS) {
        command->args[i] = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(command->args[i], token);
        expandPID(&command->args[i]);
        i++;
        token = strtok(NULL, " \n");
    }

    // Set argument count
    command->numArgs = i;
}

/*******************************************************************************
 * Function name:   void expandPID(char** argument)
 *
 * Description:     Takes in a pointer to an argument, searches for the
 *                  substring "$$", and replaces it with the
 *                  PID of the currently running process.
 *
 * Postconditions:  All instances of "$$" have been replaced with the PID
 *
 * Receives:        argument        char double-pointer
 ******************************************************************************/

void expandPID(char** argument) {
    // Iterate through the argument looking for "$$"
    for(int i = 0; i < strlen(*argument) - 1; i++) {
        if((*argument)[i] == PID_EXPAND_CHAR &&
           (*argument)[i + 1] == PID_EXPAND_CHAR) {

            // Get the PID as a string
            char* pidStr = NULL;
            int numPIDChars = getPIDString(&pidStr);

            // Create new string large enough to contain PID expansion
            char* newArg = malloc(sizeof(char) *
                                  (strlen(*argument) - 1) + numPIDChars);
            memset(newArg, '\0', strlen(*argument) -1 + numPIDChars);

            // Copy argument before "$$" to new string, then PID, then
            // argument after "$$"
            strncpy(newArg, *argument, (size_t)i);
            strcat(newArg, pidStr);
            strncat(newArg, (*argument) + (i + 2), strlen(*argument) - i);

            // Replace old argument with new expanded string
            free(pidStr);
            free(*argument);
            *argument = newArg;
            newArg = NULL;

            // Recursively look for more instances of "$$"
            expandPID(argument);

            return;
        }
    }
}

/*******************************************************************************
 * Function name:   int getPIDString(char **pidStr)
 *
 * Description:     Takes in a char pointer, gets the PID of the running
 *                  process, copies the PID into a char array attached to the
 *                  char pointer.
 *
 * Postconditions:  Char pointer points to PID string
 *
 * Receives:        pidStr      Char double-pointer
 *
 * Returns:         The number of characters in the string representation
 *                  of the PID
 ******************************************************************************/

int getPIDString(char **pidStr) {
    pid_t pid = getpid();
    *pidStr = malloc(sizeof(char) * MAX_PID_CHARS);
    return sprintf(*pidStr, "%ld", (long)pid);
}

/*******************************************************************************
 * Function name:   void printExitValOrSignal(int exitStatus)
 *
 * Description:     Takes in an integer representing the exit status of a
 *                  process. If the process exited normally, prints the exit
 *                  value. If the process was killed by a signal, prints
 *                  the signal number.
 *
 * Preconditions:   exitStatus is the exit status of a process
 *
 * Receives:        exitStatus      int     Exit status of a process
 ******************************************************************************/

void printExitValOrSignal(int exitStatus) {
    if(WIFEXITED(exitStatus)) {
        printf("exit value %d\n", WEXITSTATUS(exitStatus));
        fflush(stdout);
    } else if (WIFSIGNALED(exitStatus)) {
        printf("terminated by signal %d\n", WTERMSIG(exitStatus));
        fflush(stdout);
    }
}

/*******************************************************************************
 * Function name:   int executeCommand(Command *command)
 *
 * Description:     Takes in a Command struct and executes the command
 *                  represented by the arguments in the args array.
 *
 * Preconditions:   command has received user input via promptLoop and its
 *                  arguments have been parsed with parseCommandLine()
 *
 * Receives:        command     Command struct pointer
 *
 * Returns:         int     -1 if the user used the exit command
 *                           0 otherwise
 ******************************************************************************/

int executeCommand(Command *command) {
    struct sigaction default_action = {{0}};    // Sigaction for overriding
    default_action.sa_handler = SIG_DFL;        // SIG_IGN with SIG_DFL

    static int fgStatus = 0;        // Exit status of foreground processes

    // If final argument is "&", set command into background mode
    if(!strcmp(command->args[command->numArgs - 1], BACKGROUND_STR)) {
        if(!foreground_only) {
            command->background = true;
        }

        // Erase the final argument and decrement argument count
        free(command->args[command->numArgs - 1]);
        command->args[command->numArgs - 1] = NULL;
        command->numArgs--;
    }

    // Built-in exit command: return -1 so that promptLoop() will return
    // and quit the program
    if(!strcmp(command->args[0], "exit")) {
        return -1;
    }

    // Built-in cd command
    if(!strcmp(command->args[0], "cd")) {
        // User has given a directory argument: go to that directory
        if(command->args[1]) {
            chdir(command->args[1]);

            // User hasn't given any arguments: go home
        } else {
            chdir(getenv("HOME"));
        }
        return 0;
    }

    // Built-in status command
    if(!strcmp(command->args[0], "status")) {
        printExitValOrSignal(fgStatus);
        return 0;
    }

    // All other commands: fork off a child process and run the existing command
    pid_t spawnPid = fork();

    // Handle fork errors
    if(spawnPid == -1) {
        perror("fork()");
        exit(1);

        // Child process
    } else if(spawnPid == 0) {
        // If foreground process, receive SIGINT signals
        if(!command->background) {
            sigaction(SIGINT, &default_action, NULL);
        }
        // Process IO redirections and execute command
        redirect(command);
        execvp(command->args[0], command->args);

        // Handle command errors
        perror(command->args[0]);
        fflush(stdout);
        exit(1);

        // Parent process
    } else {
        // Wait for foreground processes
        if(!command->background) {
            spawnPid = waitpid(spawnPid, &fgStatus, 0);
            // Display signal number if terminated by signal
            if(WIFSIGNALED(fgStatus)) {
                printf("terminated by signal %d\n", WTERMSIG(fgStatus));
                fflush(stdout);
            }
        }
            // Print PID for background processes
        else {
            printf("background pid is %d\n", spawnPid);
            fflush(stdout);
        }
        return 0;
    }
}

/*******************************************************************************
 * Function name:   void redirect(Command* command)
 *
 * Description:     Takes in a Command struct and handles any IO redirections
 *                  given in the command. If the process is a background
 *                  process any redirection not specified by the user will
 *                  go to/from /dev/null.
 *
 * Preconditions:   command has received user input via promptLoop and its
 *                  arguments have been parsed with parseCommandLine()
 *
 * Postconditions:  IO has been redirected
 *
 * Receives:        command     Command struct pointer
 ******************************************************************************/

void redirect(Command* command) {
    int oldOutputFD = 0;        // File descriptor for redirected output
    int oldInputFD = 0;         // File descriptor for redirected input
    int result = 0;             // Contains return value of dup2()
    int i = 0;                  // Index for command arguments
    int redirectIndex = MAX_ARGS - 1;   // Index of first redirect operator
    bool inputRedirected = false;       // Flag: user specified input redirect
    bool outputRedirected = false;      // Flag: user specified output redirect

    while(command->args[i]) {
        if(!strcmp(command->args[i], INPUT_REDIRECT) ||
           !strcmp(command->args[i], OUTPUT_REDIRECT)) {
            // Set redirectIndex to index of first redirect operator found
            if(i < redirectIndex) {
                redirectIndex = i;
            }
            // If redirect operator is for input, use the filename specified
            // in the next argument to create a new FD
            if(!strcmp(command->args[i], INPUT_REDIRECT)) {
                oldInputFD = open(command->args[i + 1], O_RDONLY);
                // Handle errors opening filename
                if(oldInputFD == -1) {
                    perror(command->args[i + 1]);
                    fflush(stdout);
                    exit(1);
                }
                inputRedirected = true;
            }
                // If redirect operator is for output, use the filename specified
                // in the next argument to create a new FD
            else if(!strcmp(command->args[i], OUTPUT_REDIRECT)) {
                oldOutputFD = open(command->args[i + 1],
                                   O_RDWR | O_CREAT | O_TRUNC, 0644);
                // Handle errors opening filename
                if(oldOutputFD == -1) {
                    perror(command->args[i + 1]);
                    fflush(stdout);
                    exit(1);
                }
                outputRedirected = true;
            }

            // Advance past redirect operator and filename argument
            i += 2;

        } else {
            // Continue to iterate through arguments
            i++;
        }
    }

    // User redirected input, or no input specified and process in background
    if(inputRedirected || (!inputRedirected && command->background)) {
        // No input specified and process is in background: create FD by
        // opening /dev/null
        if(!inputRedirected && command->background) {
            oldInputFD = open("/dev/null", O_RDONLY);
        }
        // Redirect input and handle errors
        result = dup2(oldInputFD, STDIN_FILENO);
        if(result == -1) {
            fprintf(stderr, "cannot redirect input\n");
            fflush(stdout);
            exit(2);
        }
        close(oldInputFD);
    }

    // User redirected output, or no output specified and process in background
    if(outputRedirected || (!outputRedirected && command->background)) {
        // No output specified and process is in background: create FD by
        // opening /dev/null
        if(!outputRedirected && command->background) {
            oldOutputFD = open("/dev/null", O_RDWR);
        }
        // Redirect output and handle errors
        result = dup2(oldOutputFD, STDOUT_FILENO);
        if(result == -1) {
            fprintf(stderr, "cannot redirect output\n");
            fflush(stdout);
            exit(2);
        }
        close(oldOutputFD);
    }

    // If the user specified any IO redirection, delete all arguments
    // pertaining to IO redirection so that they won't be sent to child
    // process.
    if(redirectIndex < MAX_ARGS - 1) {
        i = redirectIndex;
        while(command->args[i]) {
            free(command->args[i]);
            command->args[i] = NULL;
            command->numArgs--;
            i++;
        }
    }
}

/*******************************************************************************
 * Function name:   void checkBackgroundChildren()
 *
 * Description:     Checks for any child processes that have terminated. If
 *                  one is found, prints a notification including PID and
 *                  either exit status or signal number.
 ******************************************************************************/

void checkBackgroundChildren() {
    pid_t pid;      // PID of child process
    int bgStatus;   // Exit status of child

    // Consume any zombie processes and report on their exit status or
    // signal number
    while((pid = waitpid(-1, &bgStatus, WNOHANG)) > 0) {
        printf("Background pid %d is done: ", pid);
        printExitValOrSignal(bgStatus);
    }
}

/*******************************************************************************
 * Function name:   void catchSIGTSTP(int signo)
 *
 * Description:     Catches SIGTSTP and toggles between foreground-only mode
 *                  and regular mode.
 *
 * Postconditions:  Global variable foreground_only has been toggled to its
 *                  opposite value.
 ******************************************************************************/

void catchSIGTSTP(int signo) {
    if(foreground_only) {
        char *message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        fflush(stdout);
        foreground_only = false;
    } else {
        char *message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        fflush(stdout);
        foreground_only = true;
    }
}

/*******************************************************************************
 * Function name:   int main()
 *
 * Description:     Sets up signal handling to catch SIGTSTP (and send to
 *                  catchSIGTSTP()) and to ignore SIGINT. Declares and
 *                  initializes Command struct and passes it to promptLoop(),
 *                  starting the command prompt loop.
 ******************************************************************************/

int main() {
    // Set up signal handling
    struct sigaction SIGTSTP_action = {{0}};
    struct sigaction ignore_action = {{0}};

    SIGTSTP_action.sa_handler = catchSIGTSTP;   // Catch SIGTSTP
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;

    ignore_action.sa_handler = SIG_IGN;

    sigaction(SIGTSTP, &SIGTSTP_action, NULL);  // Catch SIGTSTP
    sigaction(SIGINT, &ignore_action, NULL);    // Ignore SIGINT

    // Declare and initialize Command struct, start command prompt loop
    Command* command = malloc(sizeof(Command));
    initCommand(command);
    promptLoop(command);
    return 0;
}