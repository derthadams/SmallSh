# SmallSh

SmallSh is a lightweight shell program for Linux written 
in C.

## Installation

First, make sure you have `git`, `make`, and `gcc` installed on your system.

Navigate to the folder where you want to 
install the program and clone the repository with the command

    git clone https://github.com/derthadams/SmallSh.git

then navigate inside the repository directory with

    cd Smallsh
    
and compile the program with

    make
    
## Using SmallSh

Run the program with

    smallsh
    
You should now see the SmallSh prompt in your terminal (a single colon):
    
    :
    
### Running Installed Programs

From the SmallSh prompt you can launch any installed program or utility by
typing its name. 

For example, if you type

    : ls
    
then `ls` will run and you'll see a listing of all files in the current 
directory:

    README.md	makefile	smallsh		smallsh.c

### Running Programs in the Background

To run a program in the background, use an ampersand (`&`) after the command.
For example, 

    : sleep 5 &
    
will run the sleep utility for 5 seconds, but since it's running in the
background it won't block SmallSh and you can run other commands while
it's executing.

After you run a program in the background, SmallSh will display the pid for your
reference:

    background pid is 22418
    
If you want to stop the execution of a program running in the background, use
`kill` with the program's pid:

    : kill 22418
    
You'll then see the following message with the exit status code:

    Background pid 22418 is done: exit value 0
    
### Displaying Exit Status Code

If you want to check the exit status code of the most-recently terminated
program, type

    : status

and you'll get a message with the status code.

    exit value 0
    
If the program exited without issues the code will be 0, but if an
error occurred the code will be a different number. 

For example, if you run the `test` utility with a filename argument
that doesn't exist in the current directory

    : test -f badfile
    
and then run the status command

    : status
    
you'll get the message

    exit value 1
    
`test` uses the exit status code to return a boolean value: `0` for true 
and `1` for false. Since `badfile` doesn't exist, the boolean result is false
and the exit status code is `1`.

### Terminating Signal Display
If a program is terminated by a signal, SmallSh will display the signal number.
For example, if you kill a process using `CTRL-C`, you'll see the message

    terminated by signal 2

### Foreground-Only Mode

You can toggle foreground-only mode on and off using `CTRL-Z`. When SmallSh has
has switched to foreground-only mode you'll see the message

    Entering foreground-only mode (& is now ignored)
    
At this point, any ampersand (`&`) you include after a command will be ignored
and the program will run in the foreground.
When you want to switch back to normal mode, use `CTRL-Z` again and you'll see
the message

    Exiting foreground-only mode

### Input/Output Redirection

SmallSh implements redirection of `stdin` and `stdout` using the `<` and `>` 
operators.

To redirect the output of a program to the input of a different program or to a
file, use the `>` operator. For example, the command

    : ls > myfile

will take the list of files and directories output by `ls` and save it to a 
text file called "myfile". If you then type

    : cat myfile
    
you'll see the contents of `myfile`, which is just the output of `ls` with one 
row for
each file or directory:

    README.md
    makefile
    myfile
    smallsh
    smallsh.c

To redirect the input of a program, use the `<` operator. For example, the 
command

    : wc < myfile
    
takes the contents of `myfile` and sends it as input to the `wc` (word count) 
utility. You should then see the output of `wc`:

           5       5      44
           
The results represent the number of lines (5), words (5), and characters (44) in 
the file.

### Changing Directory

SmallSh implements its own version of `cd` by calling the Linux API function
`chdir()`. It works the same way as the built-in `cd` implementation.

Type `cd`, then provide a path for the directory you want to change to. For 
example, 

    cd ..
    
will change to the parent of the current directory.

### Quitting SmallSh
    
To quit SmallSh, type

    exit
    
### Uninstall Executable
To uninstall the SmallSh executable, use the command

    make clean
    
You'll still have to manually delete the SmallSh folder with the source code,
makefile, .gitignore and README.md.