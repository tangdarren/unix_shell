---
title: CS 537 Project 3
layout: default
---

# CS537 Spring 2025, Project 3

## ⚠️Warnings⚠️

- This project will be checked for memory leaks! 
Here is an example how you can check for leaks:
```
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose

gcc --sanitize=address
```
- This project will have hidden test cases, which will not be released before due date, please add your own tests to test the different functionalities and edge cases.
- This project is time consuming, start now!


## Submitting your work
- Run `submission.sh` 
- Download generated p3.tar file
- Upload it to Canvas
  * Links to Canvas assignment: 
  * [Prof. Mike Swift's class](https://canvas.wisc.edu/courses/434155/assignments/2636146)
  * [Prof. Ali Abedi's class](https://canvas.wisc.edu/courses/434150/assignments/2636143)


## Learning Objectives

In this project, you’ll build a simple Unix shell called **wsh**. The shell is the heart of the command-line interface, and thus is central to the Unix/C programming environment. Knowing how the shell itself is built is the focus of this project.

There are three specific objectives to this assignment:

* To learn how processes are created, destroyed, and managed.
* To know how processes communicate among each other via pipes.
* To further familiarize yourself with the shell and gain understanding of it's internals

## Program Specifications

### Basic Shell: `wsh`

In this assignment, you will implement a *command line interpreter (CLI)* or, as it is more commonly known, a *shell*.

The shell you implement will be similar to, but simpler than, the one you run every day in Unix. When you are in doubt about some behavior in this assignment, try the behavior in `bash` before you ask. Maybe it makes things clear. Or not, and you will come to office hours (preferably) or ask on Piazza.

The maximum line size can be 1024 and the maximum arguments on a command line can be 128.

NOTE: You are provided a skeleton code to help with string parsing, take it as a jumping off point, and modify it according to your implementation.

The shell operates in two modes:

- **Interactive Mode:**  
  When run without arguments, your shell displays a prompt (`wsh> `) (note the space after the greater-than sign) and waits for user input.  
  ```sh
  prompt> ./wsh
  wsh> 
  ```

- **Batch Mode:**
When run with a single filename argument (e.g., ./wsh script.wsh), the shell reads commands from the file *without printing a prompt*.

The shell can be invoked with either no arguments or a single argument; anything else is an error.

### Functionality

Your shell must support the following key features:
- *Parse and execute external commands*
- *Execute specific set of built-in commands*
- *Support variable substitution*
- *Support pipelines*
- *Support command substitution*


### Parse and Execute External Commands
You should structure your shell such that it creates a process for each new command (the exception are `built-in` commands, discussed below). Your basic shell should be able to parse a command and run the program corresponding to the command. For example, if the user types `cp -r /tmp /tmp2`, your shell should run the program `/bin/cp` with the given arguments `-r`, `/tmp` and `/tmp2` (how does the shell know to run `/bin/cp`? It’s something called the shell **path**; more on this below).

For reading lines of input, you should use `strtok()` and we guarantee that each token is delimited by a single space.

To execute commands, look into `fork()`, `exec()`, and `wait()/waitpid()`. See the man pages for these functions, and also read the relevant book chapter for a brief overview.

You will note that there are a variety of commands in the `exec` family; for this project, you must use `execv`. You should **not** use the `system()` library function call to run a command. Remember that if `execv()` is successful, it will not return; if it does return, there was an error (e.g., the command does not exist).

### Execute Specific Set of Built-in Commands
Built-in commands (such as exit, export, local, and vars) are handled directly by the shell instead of being executed as external programs. Your shell should detect these commands and process them without forking a new process.

Here is the list of built-in commands for `wsh`:

* `exit`: When the user types exit, your shell should simply call the `exit` system call. It is an error to pass any arguments to `exit`.
* `export`: Used as `export VAR=<value>` to create or assign variable `VAR` as an environment variable. If a user types `export VAR` or `export VAR=` (with no `=<value>` or `<value>` respectively), you should unset the enviornment variable.
* `local`: Used as `local VAR=<value>` to create or assign variable `VAR` as a shell variable. If a user types `local VAR` or `local VAR=` (with no `=<value>` or `<value>` respectively), you should set that local variable to empty string. An assignment to an empty string will clear the variable. Cleared variable will be still visible in the list of variables (vars). Important note here is that though the value is optional, key is not, invocation like `local =<value>` or `local` are errors.
* `vars`: Described below in the "Displaying Variables" section.
* `ls`: Produces the same output as `LANG=C ls -1 --color=never`, however you cannot spawn `ls` program because this is a built-in. This built-in does not implement any parameters. Some implementations print slash `/` at the end of a directory name, it is up to you if you print it or not.
* `ps`: Produces a subset of `ps -eo pid:5,ppid:5,s:1,comm --sort=pid` command, We will print processes with only digit name directories in /proc/, you can get these parameters from the stat file in each process directory. Do not spawn the `ps` program because this is a built-in. This built-in does not implement any parameters. 

### Support variable substitution
Every Linux process has its set of environment variables. These variables are stored in the `environ` extern variable. Use `man environ` to learn more about this.

Some important things about environment are the following:
1. When `fork` is called, the child process gets a copy of the `environ` variable.
2. When a system call from the `exec` family of calls is used, the new process is either given the `environ` variable as its environment or a user specified environment depending on the exact system call used. See `man 3 exec`.
3. There are functions such as `getenv` and `setenv` that allow you to view and change the environment of the current process.

Environment variables may be added or modified by using the built-in `export` command like so:
```sh
export MYENVVARNAME=somevalue
```
After this command is executed, the `MYENVVARNAME` variable will be present in the environment. Check `setenv`. This means any future environment-variable lookups in the shell or spawned child processes should be able to use this new variable.

If a user types `export VAR` or `export VAR=` (with no `=<value>` or `<value>` respectively), you should unset the enviornment variable. Check `unsetenv`.

**Shell Variables**

Shell variables are different from environment variables. They can only be used within shell commands, are only active for the current session of the shell, and are not inherited by any child processes created by the shell.

We use the built-in `local` command to define and set a shell variable:
```sh
local MYSHELLVARNAME=somevalue
```

The variable never contains space (` `) hence there is no need nor special meaning of quotes (`""`).

This variable can then be used in a command like so:

```sh
cd $MYSHELLVARNAME
```

which will translate to 

```sh
cd somevalue
```

In our implementation of shell, a variable that does not exist should be replaced by an empty string. If a user types `local MYSHELLVARNAME` or `local MYSHELLVARNAME=` (with no `=<value>` or `<value>` respectively), you should assign an empty string to MYSHELLVARNAME. An assignment to an empty string will clear the variable. Cleared variable will be still visible in the list of variables (`vars`).

**Variable substitution**

Whenever the `$` sign in a command is NOT immediately followed by a `(`, you can assume it is always followed by a variable name. Variable values should be directly substituted for their names when the shell interprets the command. Tokens in our shell are always separated by white space, and variable names and values are guaranteed to each be a single token. For example, given the command `mv $ab $cd,`, you would need to replace variables `ab` and `cd`.


**If a variable does not exist (neither as local nor as environment),** substitute an empty string.  

+**Remember**: environment variables do **not** appear in `vars`. 
+

**NOTE** If a variable exists as both the environment variable and a shell variable, the environment variable takes precedence i.e. if the user runs the following sequence of commands
```sh
wsh> local var=my_local_value
wsh> export var=my_global_value
wsh> echo $var
```

The output will be `my_global_value`.

You can assume the following when handling variable assignment:
- The entire value of the variable will be present on the same line, following the `=` operator. There will not be multi-line values; you do not need to worry about quotation marks surrounding the value. 
- Variable names and values will not contain spaces or `=` characters.
- There is no limit on the number of variables you should be able to assign.

**Displaying Variables**

The `env` utility program (not a shell built-in) can be used to print the environment variables. For local variables, we use a built-in command in our shell called `vars`. Vars will print all of the local variables and their values in the format `<var>=<value>`, one variable per line. Variables should be printed in insertion order, with the most recently created variables printing last. Updates to existing variables will modify them in-place in the variable list, without moving them around in the list. Here's an example:

```sh
wsh> local a=b
wsh> local c=$a
wsh> vars
a=b
c=b
wsh> local a=
wsh> vars
a=
c=b
```

Having a dollar sign on the left sign is an error, e.g. `local $a=b` however `local a=$b` is correct and should be expanded.

**Remember**: environment variables do **not** appear in `vars`. 

### Paths

In our original example in the beginning, the user typed `cp` and the shell knew it has to execute the program `/bin/cp`. How does your shell know this?

It turns out that the user must specify a **path** variable to describe the set of directories to search for executables; the set of directories that comprise the path are sometimes called the search path of the shell. The path variable contains the list of all directories to search, in order, when the user types a command.

**Important:** Note that the shell itself does not implement `cp` or other commands (except built-ins). All it does is find those executables in one of the directories specified by path and create a new process to run them. Try `echo $PATH` to see where your shell looks for executables.

**Important:** If the user specifies a command that starts with a `.` (i.e. a relative path) or `/` (i.e. full path) (e.g. `./myprog` or `/usr/bin/ls`), you should treat it as a path to an executable file.

To check if a particular file exists in a directory and is executable, consider the `access()` system call. For example, when the user types `cp`, and path is set to `PATH=/usr/bin:/bin`, try `access("/usr/bin/cp", X_OK)`. If that fails, try `/bin/cp`. If that fails too, it is an error.

Your initial shell PATH environment variable should contain one directory: `/bin`. This means that you will overwrite `$PATH` inherited from your parent.

### What to Execute?
In general, the priority what to execute is following, where 1 is the highest priority:

1. Built-in.
2. Full or relative path (i.e. check if the command starts with a "." or "/", in case this match happens but the executable does not exist/cannot be accessed, that is a Command not found or not executable error)
3. Paths specified by `$PATH`


### Support Pipelines
A pipeline is a sequence of one or more commands separated by the control operators ‘|’.

The output of each command in the pipeline is connected via a pipe to the input of the next command. That is, each command reads the previous command’s output. 

For example:

```sh
wsh> echo hello world | tr ' ' '\n'
```
produces the output:
```sh
hello
world
```

Each command in a multi-command pipeline, where pipes are created, is executed in its own subshell, which is a separate process. Each command in the pipeline runs *concurrently*, with the standard output of one command being fed to the standard input of the next.

There can be a maximum of 128 pipes in the input.

Built-in commands will not be used with pipe together.

The shell waits for all commands in the pipeline to complete.
 
Look into `pipe()` and  `dup2()`.

Remember to close unnecessary file descriptors in each process.

### Support Command Substitution
Command substitution allows the output of a command to replace the command itself. In your shell, this is indicated by the syntax $(subcommand), which can be nested.

For example:

```sh
wsh> echo $(echo $(echo alpha beta | tr 'a-z' 'A-Z') $(echo gamma | sed 's/.*/DELTA/'))
```
produces the output:
```sh
ALPHA BETA DELTA
```

**Let us break it down:**
```sh
echo alpha beta | tr 'a-z' 'A-Z'
```
echo alpha beta prints alpha beta. That output is piped to tr 'a-z' 'A-Z', which converts lowercase letters to uppercase.

Result: `ALPHA BETA`
```sh
echo gamma | sed 's/.*/DELTA/'
```

echo gamma prints gamma.
That output is piped to `sed 's/.*/DELTA/'`, which replaces any entire line (i.e., .*) with DELTA.

Result: `DELTA`

Substituting the 2 inner commands, this command is transformed to:
```sh
wsh> echo $(echo ALPHA BETA DELTA)
```

Now doing a command substitution again:
```sh
echo ALPHA BETA DELTA
```

Thus, the outer echo $( ... ) prints ``ALPHA BETA DELTA``.

**Logic**

To perform a command substitution identify the subcommand by scanning for `$(` in the input. (Be careful not to mix it up with variable substitution).
Extract the subcommand (taking care to handle nested parentheses properly)
Then execute this command in a separate process, capturing its output (you can use pipes here too!).
Replace the command substitution with the standard output of the command, with any trailing newlines deleted.


Built-in commands will not be used with command substitution together.

### Example of command substitution with variable substitution

```sh
wsh> local MYVAR=isolated
wsh> echo $MYVAR
isolated
wsh> echo $(echo $MYVAR)
```

In the above, the command substitution runs in a subshell that has cleared its local variables, so $MYVAR expands to an empty string.

### Error conditions

Your shell should be resistant to various possible error conditions (failed command, bad builtin parameters etc.) and should not exit if you don't type `exit` or press `Ctrl-D`. The exit value of `wsh` is the exit value (for processes) or the return value (for builtins) of the last command. 

You should print out errors (from built-ins, when command not found, bogus built-in arguments…), to `stderr`.

You should exit with exit value -1 (255) for all error case.
The return value for erroneous built-in commands should also be -1 (255).

Here are the error conditions and statements that do **not** exit:
- ``"Command not found or not executable\n"`` : When a full path or relative path is not accessable or when you have checked all values from the **PATH** variable and none of them are valid/accessable.
- ``"PATH not set\n"``: When the given command is not a full path or relative path and the **PATH** variable is empty.
- ``"Unmatched parentheses in command substitution\n"`` 
- ``"Incorrect usage of export. Usage: export {VariableName}={VariableValue}\n"``: When export command does not have exactly two tokens (including export)
- ``"Incorrect usage of local. Usage: local {VariableName}={VariableValue}\n"``: When local command does not have exactly two tokens (including local)
- ``"Incorrect usage of exit. Usage: exit\n"``: When exit command has any parameters
- ``"Missing closing quote\n"``: Your skeleton code handles this, but you should go through it anyway.
- ``"fgets error\n"``: Your skeleton code handles this
- ``perror("execv")``: If a exec fails (NOTE only the child exists in this case, the shell still continues)

Here are the error conditions and statements that do exit:
- ``"getcwd"``: If at the beginning of an interactive session getcwd fails.
- ``"Error opening file"``: If fopen fails for the passed file in batch mode.
- ``perror("pipe"), perror("fork"), perror("dup2"), perror("strdup"), perror("malloc"), perror("realloc")`` : If the respective calls fail

### Tips and Hints

- Develop incrementally: Get basic command execution working before adding support for pipes, substitutions, and variables.
- It is recommended that you separate the process of parsing and execution parse first, look for syntax errors (if any), and then finally execute the commands.
- Check the return codes of all system calls from the very beginning of your work. This will often catch errors in how you are invoking these new system calls. It’s also just good programming sense.
- Changing global variables after fork in the child process does not affect anything in the parent, processes communicate through exit codes via wait or pipes (ofcourse this is only for this project, processes can communicate in other ways too).
- Flush your stdout at appropriate times or you may see newline errors in tests (see fflush)
- This project is longer than past projects, try to use functions instead of a giant main to make your life easier.



## Administrivia 
- **Due Date** by March 3, 2025 at 11:59 PM 
- Questions: We will be using Piazza for all questions.
- Collaboration: The assignment has to be done by yourself. Copying code (from others) is considered cheating. [Read this](http://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not. Please help us all have a good semester by not doing this.
- This project is to be done on the [lab machines](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/), so you can learn more about programming in C on a typical UNIX-based platform (Linux).
- You can create an additional .c and .h files to split your `wsh.c`. Just make sure `make` works properly with additional files. 
- A few sample tests are provided in the project repository. To run them, execute `run-tests.sh` in the `tests/` directory. Try `run-tests.sh -h` to learn more about the testing script. Note these test cases are not complete, and you are encouraged to create more on your own. 
- **Slip Days**: 
  - In case you need extra time on projects, you each will have 2 slip days for the first 3 projects and 2 more for the final three. After the due date we will make a copy of the handin directory for on time grading. 
  - To use a slip days or turn in your assignment late you will submit your files with an additional file that contains **only a single digit number**, which is the number of days late your assignment is (e.g. 1, 2, 3). Each consecutive day we will make a copy of any directories which contain one of these `slipdays.txt` files. 
  - `slipdays.txt` must be present at **the top-level directory** of your submission. 
  - Project directory structure. (some files are omitted)
  ```
  p3/
  ├─ solution/
  │  ├─ README.md
  │  ├─ wsh.c
  │  ├─ wsh.h
  |  ├─ Makefile
  ├─ tests/
  ├─ README.md
  ├─ slipdays.txt
  ```
  - We will track your slip days and late submissions from project to project and begin to deduct percentages after you have used up your slip days.
  - After using up your slip days you can get up to 80% if turned in 1 day late, 60% for 2 days late, and 40% for 3 days late, but for any single assignment we won't accept submissions after the third days without an exception. This means if you use both of your individual slip days on a single assignment you can only submit that assignment one additional day late for a total of 3 days late with a 20% deduction.
  - Any exception will need to be requested from the instructors.

  - Example of `slipdays.txt`:

```sh
$ cat slipdays.txt
1
```
