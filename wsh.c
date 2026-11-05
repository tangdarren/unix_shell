// Author:  Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2023
// Revised: John Shawger <shawgerj@cs.wisc.edu>, Spring 2024
// Revised: Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2024
// Revised: Leshna Balara <lbalara@cs.wisc.edu>, Spring 2025

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "wsh.h"

// Dynamic memory allocation for variable storage
char **local_var_names = NULL;  // Pointer to store variable names
char **local_var_values = NULL; // Pointer to store variable values
int var_count = 0;  // Track the number of local variables

//store exit status
int exit_value = 0;

/* Print an application error message (does not exit) */
void app_error(char *msg)
{
  fprintf(stderr, "%s\n", msg);
}

void non_recoverable_error(char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(-1);
}

// This is just basic string parsing, some of the things not handled are mentioned as TODOs*/
// Exits currently do not return the correct value according to the spec,
// so you will need to change the parameter when appropriate*/
// Some helper functions available to you but currently not reachable:
// char *do_variable_substitution(const char *command)
// char *replaceCommandSubstitution(const char *command)
// print_ps_header (also provides how to print ps entries as HINT)
int main(int argc, char **argv)
{
  check_params(argv, argc);
  setenv("PATH", "/bin", 1);
  if (argc == 1)
    interactive_main();
  else
    batch_main(argv[1]);

  return 0; /* Not reached */
}

/* TODO: Print prompt where appropriate */
void interactive_main(void)
{
  if (!getcwd(cwd, MAXLINE))
  {
    perror("getcwd");
    exit(-1);
  }
  while (1)
  {
    printf("%s", prompt);
    fflush(stdout);
    char cmdline[MAXLINE];
    if (fgets(cmdline, MAXLINE, stdin) == NULL)
    {
      if (ferror(stdin))
        app_error("fgets error\n"); //added \n for the test case
      if (feof(stdin))
      {
        /* TODO: Handle end of file */
        exit(exit_value);
      }
    }

    eval(cmdline);
    fflush(stdout);
  }
}

/* Batch mode: read each line from the given script file */
int batch_main(char *scriptFile)
{
  FILE *file = fopen(scriptFile, "r");
  if (file == NULL)
  {
    perror("fopen");
    exit(-1);
  }

  char line[MAXLINE + 1];
  while (fgets(line, MAXLINE, file) != NULL)
  {
    int length = strlen(line);
    if (length > 0 && line[length - 1] != '\n')
    {
      line[length] = '\n';
      line[length + 1] = '\0';
    }
    eval(line);
    fflush(stdout);
  }

  fclose(file);
  exit(exit_value);
  return 0;
}

/* Evaluate a command line */
/* TODO: Actual Evaluation */
void eval(char *cmdline)
{
  //have to check this before substitution because $a gets replaced below
  if (strncmp(cmdline, "local $", 7) == 0) {
    fprintf(stderr, "Incorrect usage of local. Usage: local {VariableName}={VariableValue}\n");
    exit_value = -1;
    return;
  }

  cmdline = do_variable_substitution(cmdline);

  cmdline = replaceCommandSubstitution(cmdline);

  //returns NULL on errors like unmatched parentheses, the message was already printed
  if (cmdline == NULL) {
    exit_value = -1;
    return;
  }

  /* If the command contains a pipe, handle it separately */
  if (strchr(cmdline, '|') != NULL)
  {
    eval_pipe(cmdline);
    return;
  }

  /* you can invoke parseline_no_subst(const char *cmdline, char **argv, int *argc)
    for basic parsing with no substitutions*/
  char *argv[MAXARGS + 1] = {NULL};
  int argc = 0;

  parseline_no_subst(cmdline, argv, &argc);
  
  if(argv[0] == NULL) {
    return;
  }

  if(strcmp(argv[0], "exit") == 0 ) {
    exit_built_in(argc);
    return;
  } 
  else if(strcmp(argv[0], "export")== 0) {
    export_built_in(argv);
    return;
  }
  else if(strcmp(argv[0], "local") == 0) {
    local_built_in(argv);
    return;
  }
  else if(strcmp(argv[0], "vars")==0) {
    vars_built_in();
    return;
  }
  else if(strcmp(argv[0], "ls")==0) {
    ls_built_in();
    return;
  }
  else if(strcmp(argv[0], "ps")==0) {
    ps_built_in();
    return;
  }
  
  pid_t pid = fork();

  //child process
  if (pid == 0) {
    //find path and handle error
    char *path = find_path(argv[0]);
    
    //can exit here because we dont want to call execv which will also return an error 
    if(path == NULL) {
      _exit(-1);
    }

    execv(path, argv);
    perror("execv");
    _exit(-1);
  }
  
  //parent process
  else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    //dont really know how to check waitpid here. not really mentioned that we have to. i hope we dont have to.
    exit_value = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  } 
  //fork failed
  else {
    perror("fork"); //exit right away
    exit(-1);
  }
}

/* basic check to verify we have at most 2 arguments */
void check_params(char **argv, int argc)
{
  (void)argv;
  if (argc > 2)
  {
    printf("Usage: wsh or wsh script.wsh \n");
    printf("No arguments allowed.\n");
    exit(-1);
  }
}

/* Evaluate a command line that contains a pipe */
/* TODO: The basic parsing in eval_pipe does not handle substitutions of any kind */
void eval_pipe(char *cmdline)
{
  /* Split the processed command line on '|' */
  char *commands[MAXARGS + 1];
  int num_commands = 0;
  char *token = strtok(cmdline, "|");
  while (token != NULL && num_commands < MAXARGS + 1)
  {
    while (*token == ' ')
      token++; /* trim leading whitespace */
    commands[num_commands++] = strdup(token);
    token = strtok(NULL, "|");
  }

  /* For each pipeline segment, tokenize without re‑substitution */
  char **argv_list[num_commands];
  int argc_list[num_commands];
  for (int i = 0; i < num_commands; i++)
  {
    argv_list[i] = malloc(sizeof(char *) * (MAXARGS + 1));
    parseline_no_subst(commands[i], argv_list[i], &argc_list[i]);
    free(commands[i]);
    if (!argc_list[i])
    {
      return;
    }
  }

  /* Create (num_commands-1) pipes */
  int pipefds[256];
  for (int i = 0; i < num_commands - 1; i++)
  {
    if (pipe(pipefds + i * 2) < 0)
    {
      perror("pipe");
      exit(-1);
    }
  }

  /* Now you have commands in argv_list[i][0] and their arg list in argv_list[i] */
  /* TODO: Execute and handle the command when needed */

  //store process id
  pid_t pid[num_commands];

  //loop through commands in pipe
  for (int i = 0; i < num_commands; i++)
  {
    pid[i] = fork();
    //child process
    if (pid[i] == 0) {
      //checks if that we are on the first command and if we are not references the previous pipe (RD)
      if (i > 0) {
        if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) {
          perror("dup2");
          exit(-1);
        }
      }

      //checks that we are not in the last command and sends the output to the next pipe (WR)
      if (i < num_commands - 1) {
        if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) {
          perror("dup2");
          exit(-1);
        }
      }

      // Close all pipe file descriptors in the child process
      for (int j = 0; j < 2 * (num_commands - 1); j++) {
        close(pipefds[j]);
      }

      // Execute the current command
      // FIXED THIS SESSION FOR test #9


      char *path = find_path(argv_list[i][0]);
      if (path) {
        execv(path, argv_list[i]);
        perror("execv");
        _exit(-1);
      } else {
        _exit(-1);
      }
    }
  }

  /* close all pipe file descriptors */
  for (int i = 0; i < 2 * (num_commands - 1); i++) {
    close(pipefds[i]);
  }

  //let child processes finish
  for (int i = 0; i < num_commands; i++) {
    int status;
    if (waitpid(pid[i], &status, 0) == -1) {
        perror("waitpid");
        exit(-1);
    }
    //exit value of a pipeline is the exit value of the last command
    if (i == num_commands - 1) {
      exit_value = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
  }

  for (int i = 0; i < num_commands; i++)
  {
    free(argv_list[i]);
  }
}

/* Replace variables of the form $VAR (skipping those starting with $(') */
char *do_variable_substitution(const char *command)
{

  //so basically the first thing i am checking is the direct command line. in this, i am basically making sure that 
  // we do not have the case where we have to account for the example of command substitution with variable substitution
  // i do not know how to do this properly but i am going to use it this way which basically goes through the command line 
  // and checks essentially if there is a $(---) in a $(---)

  //this was the part i had to code in order to complete test 3: $(echo $(echo $MYVAR))
  //this allows us to skip over the normally variable substiution (the edge case mentioned in the starter code)
  int count_of_commands = 0;
  int variable_found_edgeCase = 0; 

  for (size_t i = 0; i < strlen(command); i++)
  {
    //this line ONLY works because I am assuming that there will be no a$ variable because we were not told about this
    // we check for the $( which means we are in the variable
    if (command[i] == '$' && i + 1 < strlen(command) && command[i + 1] == '(')
    {
      count_of_commands++;
      continue;
    }
    //here we might just have the other end of the original $( part which means that now we kind of have to restart our search
    else if (command[i] == ')' && count_of_commands > 0)
    {
      count_of_commands--;
    }

    //this is our last check if we have seen a $( and another $( with no ) in between. This means we have meet the condition
    // and the edge case is activated where we no longer go through the variable substitution.
    //originally i had this checker for the character to make sure the one after is an actual value but i think we can assume this
    // because our original errors that are listed tell us nothing about accounting for this error
    if (count_of_commands > 0 && command[i] == '$')
    {
      variable_found_edgeCase = 1;
      //exit cause we found it! dont really have to honestly but its good practice
      break; 
    }
  }

  const char *delimiters = " \t\n=";
  char *commandCopy = strdup(command);
  if (!commandCopy)
  {
    perror("strdup"); //NOTE FOR FUTURE: need to exit
    exit(-1);
  }
  /* We work on a copy that is replaced gradually */
  char *current = strdup(command);
  if (!current)
  {
    perror("strdup"); //NOTE FOR FUTURE: need to exit
    exit(-1);
  }

  char *saveptr;
  char *token = strtok_r(commandCopy, delimiters, &saveptr);
  while (token != NULL)
  {
    size_t token_len = strlen(token);
    //added check to see if its our edge case
    if (variable_found_edgeCase != 1 && token[0] == '$' && token_len > 1 && token[1] != '(') {
      //get variable name without the $
      char *var_name = token + 1;
      char *var_value = NULL;

      //WE HAVE TO FOLLOW THE ORDER SPECIFIED IN THE INSTRUCTIONS:
      // NOTE If a variable exists as both the environment variable and a shell variable, the 
      //environment variable takes precedence i.e. if the user runs the following sequence of commands
      //Check 1
      var_value = getenv(var_name);
      //Check 2
      if (var_value == NULL)
      {
        for (int i = 0; i < var_count; i++)
        {
          if (strcmp(local_var_names[i], var_name) == 0)
          {
            var_value = local_var_values[i];
            break;
          }
        }
      }

      //replace because there ws a variable found
      char *pos = strstr(current, token);
      if (pos != NULL)
      {
        //length of old variable, 0 if NULL
        size_t var_value_len;
        if (var_value != NULL) {
          var_value_len = strlen(var_value); 
        } else {
          var_value_len = 0;
        }

        //length of new variable
        size_t old_len = strlen(current) + 1;
        size_t new_len = old_len - token_len + var_value_len;

        //realloc can move current so remember where pos was
        size_t pos_offset = pos - current;

        //reallocate memory to make space. never shrink because the text is not shifted yet
        char *new_current = realloc(current, new_len > old_len ? new_len : old_len);
        if (new_current == NULL)
        {
            perror("realloc"); //need to exit
            exit(-1);
        }
        current = new_current;
        pos = current + pos_offset;

        //shift remaining content to adjust for new length
        size_t len = strlen(pos + token_len) + 1;

        //memmove because the old and new positions can overlap
        memmove(pos + var_value_len, pos + token_len, len);

        //if NULL copy empty and copy if not
        if (var_value != NULL) {
          for (size_t i = 0; i < var_value_len; i++) {
            pos[i] = var_value[i];
          }
        } else {
          for (size_t i = 0; i < var_value_len; i++) {
            pos[i] = '\0';
          }
        }
      }
    }
    else if (token[0] == '$')
    {
        //leave token as is. we do this because in the case echo $(echo $MYVAR), we want to not change it
        //this is so that we can use this full statement lateron in replaceCommandSubstitution
        //if we changed it now, then we would run into a lot of problems
        //it took me a long time to figure this out

        //FUTURE: can delete but just leave, it was with the starter code
        
    }
    token = strtok_r(NULL, delimiters, &saveptr);
  }
  free(commandCopy);
  return current;
}

/* Replace command substitutions of the form $(subcommand) recursively
  Returns null in case of any errors */
char *replaceCommandSubstitution(const char *command)
{
  //printf("\nTHIS IS MY COMMAND: %s\n", command);
  //fflush(stdout); //this was super useful for number 3 debugging
  char *result = strdup(command);
  if (!result)
  {
    perror("strdup");
    exit(-1);
  }

  //printf("\nTHIS IS MY RESULT: %s\n", result);
  //fflush(stdout); //this was super useful for number 3 debugging

  char *sub_start;
  while ((sub_start = strstr(result, "$(")) != NULL)
  {
    char *sub_end = sub_start + 2;
    int paren_count = 1;
    while (*sub_end && paren_count > 0)
    {
      if (*sub_end == '(')
        paren_count++;
      else if (*sub_end == ')')
        paren_count--;
      sub_end++;
    }
    if (paren_count != 0)
    {
      app_error("Unmatched parentheses in command substitution\n");
      free(result);
      return NULL;
    }

    int sub_len = (sub_end - 1) - (sub_start + 2);
    char *sub_cmd = malloc(sub_len + 1);
    if (!sub_cmd)
    {
      perror("malloc");
      exit(-1);
    }
    strncpy(sub_cmd, sub_start + 2, sub_len);
    sub_cmd[sub_len] = '\0';

    /* TODO: Execute the subcommand and capture its output */
    /* sub_cmd has the value of the innermost nested command */
    /* Create a pipe to capture output */
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
      perror("pipe");
      free(sub_cmd);
      free(result);
      return NULL;
    }

    //this was the only way i could figure out how to remove local variables for the child process wihtout removing
    //for the parent. basically, i save a temporary copy of the local variables then I am able to go back later
    //and reset them once the child process is over.
    char **temp_var_names = malloc(var_count * sizeof(char *));
    char **temp_var_values = malloc(var_count * sizeof(char *));
    int temp_var_count = var_count;

    if (!temp_var_names) {
      perror("malloc");
      free(sub_cmd);
      free(result);
      close(pipefd[0]);
      close(pipefd[1]);
      exit(-1);
    }
    if (!temp_var_values) {
      perror("malloc");
      free(temp_var_names);
      free(sub_cmd);
      free(result);
      close(pipefd[0]);
      close(pipefd[1]);
      exit(-1);
    }

    for (int i = 0; i < var_count; i++) {
        temp_var_names[i] = strdup(local_var_names[i]);  // Backup names
        temp_var_values[i] = strdup(local_var_values[i]);  // Backup values
        if(!temp_var_names[i] | !temp_var_values[i]) {
          for(int j = 0; j <= i; j++) {
            free(temp_var_names[j]);
            free(temp_var_values[j]);
          }
          free(temp_var_names);
          free(temp_var_values);
          free(sub_cmd);
          free(result);
          close(pipefd[0]);
          close(pipefd[1]);
          perror("strdup");
          exit(-1);
        }
    }

    pid_t pid = fork();
    //fork did not work
    if (pid == -1)
    {
      perror("fork");
      for(int i = 0; i < temp_var_count; i++) {
        free(temp_var_names[i]);
        free(temp_var_values[i]);
      }
      free(temp_var_names);
      free(temp_var_values);
      free(sub_cmd);
      free(result);
      close(pipefd[0]);
      close(pipefd[1]);
      return NULL;
    }
    //child process
    else if (pid == 0)
    {
      //these are some of the basic things i learned we had to do with pipes in a youtube video but i am not 100%
      // closes read end, write end, and connect stdout to the pipe
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[1]);

       //free local variables for now
       for (int i = 0; i < var_count; i++) {
           free(local_var_values[i]);
           local_var_values[i] = strdup("");
       }
       var_count = 0;

       eval(sub_cmd);
       _exit(0);
   }

    //set back local variables so the parent can access them. it is why i had those methods above to save to a temp
    for (int i = 0; i < temp_var_count; i++) { 
        free(local_var_names[i]);
        free(local_var_values[i]);
        local_var_names[i] = temp_var_names[i];
        local_var_values[i] = temp_var_values[i];
    }

    var_count = temp_var_count;
    free(temp_var_names);
    free(temp_var_values);

    /* Parent process */
    close(pipefd[1]); // Close write end
    char buffer[MAXLINE];
    ssize_t nbytes;
    char *output = malloc(1);
    if (!output)
    {
      perror("malloc");
      free(sub_cmd);
      free(result);
      return NULL;
    }
    output[0] = '\0';

    while ((nbytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
    {
      buffer[nbytes] = '\0';
      char *temp = realloc(output, strlen(output) + nbytes + 1);
      if (!temp)
      {
        perror("realloc");
        free(output);
        free(sub_cmd);
        free(result);
        close(pipefd[0]);
        return NULL;
      }
      output = temp;
      strcat(output, buffer);
    }
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    free(sub_cmd);

    /* Remove all trailing newlines if present */
    size_t out_len = strlen(output);
    while (out_len > 0 && output[out_len - 1] == '\n')
      output[--out_len] = '\0';

    size_t prefix_len = sub_start - result;
    size_t output_len = strlen(output);
    size_t suffix_len = strlen(sub_end);
    size_t new_len = prefix_len + output_len + suffix_len + 1;
    char *new_result = malloc(new_len);
    if (!new_result)
    {
      perror("malloc");
      exit(-1);
    }
    memcpy(new_result, result, prefix_len);
    memcpy(new_result + prefix_len, output, output_len);
    memcpy(new_result + prefix_len + output_len, sub_end, suffix_len + 1);
    free(output);
    free(result);
    result = new_result;
  }
  return result;
}

/* Tokenize without further substitution.
   This version now strdup()’s each token so that the returned argv tokens remain valid. */
void parseline_no_subst(const char *cmdline, char **argv, int *argc)
{
  char *buf = strdup(cmdline);
  if (!buf)
  {
    perror("strdup");
    exit(-1);
  }
  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = ' ';
  else
  {
    buf = realloc(buf, len + 2);
    if (!buf)
    {
      perror("realloc");
      exit(-1);
    }
    strcat(buf, " ");
  }

  int count = 0;
  char *p = buf;
  while (*p && (*p == ' '))
    p++; /* skip leading spaces */

  while (*p)
  {
    char *token_start = p;
    char *token;
    if (*p == '\'')
    {
      token_start = ++p;
      token = strchr(p, '\'');
      if (!token)
      {
        app_error("Missing closing quote\n");
        //clear what was parsed so far so the command does not run
        argv[0] = NULL;
        *argc = 0;
        exit_value = -1;
        free(buf);
        return;
      }
      *token = '\0';
      p = token + 1;
    }
    else
    {
      token = strchr(p, ' ');
      if (!token)
        break;
      *token = '\0';
      p = token + 1;
    }
    argv[count++] = strdup(token_start);
    while (*p && (*p == ' '))
      p++;
  }
  argv[count] = NULL;
  *argc = count;
  free(buf);
}

typedef struct
{
  u_int16_t width;
  const char *header;
} ps_out_t;

static const ps_out_t out_spec[] = {
    {5, "PID"},
    {5, "PPID"},
    {1, "S"},
    {16, "CMD"},
};

/* Print the header with appropriate formatting for ps command */
void print_ps_header()
{
  for (size_t i = 0; i < sizeof(out_spec) / sizeof(out_spec[0]); i++)
  {
    printf("%-*s ", out_spec[i].width, out_spec[i].header);
  }
  printf("\n");

  /* HINT: of how to print the actual entry
     printf("%*d %*d %*c %-*s\n",
           out_spec[0].width, pid, // pid is int
           out_spec[1].width, ppid, // ppid is int
           out_spec[2].width, state, // state is char
           out_spec[3].width, comm); // command is char array
  */
}

/*exit: When the user types exit, your shell should simply call the exit system call. 
    It is an error to pass any arguments to exit.
    Step 1 and Step 5*/
void exit_built_in(int argc) {
  if(argc > 1) {
    fprintf(stderr, "Incorrect usage of exit. Usage: exit\n"); //do not exit
    exit_value = -1;
    return;
  }
  exit(exit_value);
}

/*export: Used as export VAR=<value> to create or assign variable VAR as an environment variable. 
    If a user types export VAR or export VAR= (with no =<value> or <value> respectively), you should 
    unset the enviornment variable.*/
void export_built_in(char **argv) {
  //told to "When export command does not have exactly two tokens (including export)", made sure that we did not exit
  if (argv[1] == NULL || argv[2] != NULL) {
    fprintf(stderr, "Incorrect usage of export. Usage: export {VariableName}={VariableValue}\n"); //do not exit
    exit_value = -1;
    return;
  }

  // NULL if no equal that exists used later
  char *equal_sign = strchr(argv[1], '=');

  if(equal_sign != NULL) {
    *equal_sign = '\0';
    char *var_name = argv[1];
    char *var_value = equal_sign + 1;

    //i used to check if variable is valid such that it has characters that are possible but in the instructions and 
    // errors we do not need to account for this

    //this is for when we are checking if there is a= in this case we just want to set a to an empty string
    if (*var_value == '\0') {
      if (unsetenv(var_name) != 0) {
        perror("unsetenv");
        exit_value = -1;
      } else {
        exit_value = 0;
      }
    } else {
      //set enviornment variables to new value
      if (setenv(var_name, var_value, 1) != 0) {
        perror("setenv");
        exit_value = -1;
      } else {
        exit_value = 0;
      }
    }
  } 
  else {
    //unset the environment variable bc we had just the variable by itself
    if (unsetenv(argv[1]) != 0) {
      perror("unsetenv");
      exit_value = -1;
    } else {
     exit_value = 0;
    }
  }
}

/*local: Used as local VAR=<value> to create or assign variable VAR as a shell variable. If a user types local VAR or local 
    VAR= (with no =<value> or <value> respectively), you should set that local variable to empty string. An assignment to an
    empty string will clear the variable. Cleared variable will be still visible in the list of variables (vars). Important 
    note here is that though the value is optional, key is not, invocation like local =<value> or local are errors.*/
void local_built_in(char **argv) {
    exit_value = 0;

    // Check for incorrect usage (must have a variable name and at most one argument)
    if (argv[1] == NULL || argv[2] != NULL) {
        fprintf(stderr, "Incorrect usage of local. Usage: local {VariableName}={VariableValue}\n");
        exit_value = -1;
        return;
    }

    // Check if the variable has an '=' sign
    char *equal_sign = strchr(argv[1], '=');
    char *var_name = argv[1];
    char *var_value = "";  // Default to empty string if no value is provided

    if (equal_sign) {
        *equal_sign = '\0';  // Split at '='
        var_value = equal_sign + 1;
    }

    // Validate variable name (should not contain spaces or '=')
    if (var_name[0] == '\0' || strchr(var_name, ' ') || strchr(var_name, '=')) {
        fprintf(stderr, "Invalid variable name: %s\n", var_name);
        exit_value = -1;
        return;
    }

    // Reject variable names starting with '$' (they should not be assigned)
    if (var_name[0] == '$') {
        fprintf(stderr, "Invalid variable name: %s\n", var_name);
        exit_value = -1;
        return;
    }

    // Store or update the variable in the shell session
    // Check if the variable already exists
    for (int i = 0; i < var_count; i++) {
        if (strcmp(local_var_names[i], var_name) == 0) {
            // Update the value if the variable already exists
            free(local_var_values[i]);
            local_var_values[i] = strdup(var_value);
            if(!local_var_values[i]) {
              perror("strdup");
              exit(-1);
            }
            return;
        }
    }

    
   // If it's a new variable, store it
   local_var_names = realloc(local_var_names, (var_count + 1) * sizeof(char *));
   local_var_values = realloc(local_var_values, (var_count + 1) * sizeof(char *));
   if (!local_var_names || !local_var_values) {
       perror("realloc");
       exit(-1);
   }


   // Allocate memory for the new variable name and value
   local_var_names[var_count] = strdup(var_name);
   local_var_values[var_count] = strdup(var_value);
   var_count++;
}



/*vars: Described below in the "Displaying Variables" section.*/
void vars_built_in() {
  exit_value = 0;

  //print all local variables
  for (int i = 0; i < var_count; i++) {
      //print each variable such as a = b
      printf("%s=%s\n", local_var_names[i], local_var_values[i]);
  }
}

/*ls: Produces the same output as LANG=C ls -1 --color=never, however you cannot spawn ls program because this is a built-in. 
    This built-in does not implement any parameters. Some implementations print slash / at the end of a directory name, it is up to you if you print it or not.*/
void ls_built_in() {
  DIR *dir = opendir(".");
  if (!dir) {
    perror("opendir");
    exit_value = -1;
    return;
  }

  char *file_list[MAXLINE];
  int file_count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] != '.') {
      file_list[file_count++] = strdup(entry->d_name);
    }
  }

  closedir(dir);

  //i was looking into q sort but just doing a double for loop was easiest for in printing out in lexi order
    for (int i = 0; i < file_count - 1; i++) {
        for (int j = i + 1; j < file_count; j++) {
            if (strcmp(file_list[i], file_list[j]) > 0) {
                char *temp = file_list[i];
                file_list[i] = file_list[j];
                file_list[j] = temp;
            }
        }
    }

  //print and free
  for (int i = 0; i < file_count; i++) {
      printf("%s\n", file_list[i]);
      free(file_list[i]);
  }

  exit_value = 0;
}

/*ps: Produces a subset of ps -eo pid:5,ppid:5,s:1,comm --sort=pid command, We will print processes with only digit name directories in /proc/, you can can  these parameters
    from the stat file in each process directory. Do not spawn the ps program because this is a built-in. This built-in does not implement any parameters.*/
void ps_built_in() {
  exit_value = 0;

  DIR *proc_dir = opendir("/proc");
  if (!proc_dir)
  {
      perror("opendir");
      exit_value = -1;
      return;
  }
  struct dirent *entry;

  //given format
  print_ps_header();

  while ((entry = readdir(proc_dir)) != NULL)
  {
    //check for valid directory
    int valid_pid = 1;
    for (char *p = entry->d_name; *p; p++)
    {
      if (!isdigit(*p))
      {
        valid_pid = 0;
        break;
      }
    }
    if (!valid_pid)
      continue;

    //find the stats file path and then read it
    char stat_path[256];
    
    //this will allow us to create the path by taking every single part. there is /proc, PID directory, and /stat
    strcpy(stat_path, "/proc/");
    strcat(stat_path, entry->d_name);
    strcat(stat_path, "/stat");
    FILE *stat_file = fopen(stat_path, "r");
    if (!stat_file) {
      continue;
    }
      
    int pid, ppid;
    char state;
    char comm[256];

    //read only necessary files
    if (fscanf(stat_file, "%d (%255[^)]) %c %d", &pid, comm, &state, &ppid) != 4)
    {
      fclose(stat_file);
      continue;
    }

    fclose(stat_file);

    //print process information based on format
    printf("%*d %*d %*c %-*s\n",
            out_spec[0].width, pid,
            out_spec[1].width, ppid,
            out_spec[2].width, state,
            out_spec[3].width, comm);
  }

  closedir(proc_dir);
}



/*finds the current path we need to work with*/
char *find_path(const char *command) {
  
  //check if full or relative path. already good if this condition is met but if not accessable according to the
  // errors section then we have to return the string
  if (command[0] == '/' || command[0] == '.') {
    if (access(command, X_OK) == 0) {
      return strdup(command);
    } else {
      fprintf(stderr, "Command not found or not executable\n");
      return NULL;
    }
  }

  //path envrionemnt and handles execption
  char *path = getenv("PATH");
  if (!path) {
    fprintf(stderr, "PATH not set\n");
    return NULL;
  }

  //path needs to be tokenized. the only way to do this is by duplication
  char *path_copy = strdup(path);
  if (!path_copy) {
    perror("strdup");
    return NULL;
  }

  //allocate for path
  char *full_path = malloc(MAXLINE);
  if (!full_path) {
    perror("malloc");
    free(path_copy);
    return NULL;
  }

  //split path by :
  char *token = strtok(path_copy, ":");

  //makes the path into string and checks that we can use it
  while (token != NULL) {
    snprintf(full_path, MAXLINE, "%s/%s", token, command);

    if (access(full_path, X_OK) == 0) {
      free(path_copy);
       exit_value = 0;
      return full_path;
    }
    token = strtok(NULL, ":");
  }

  //command not found
  fprintf(stderr, "Command not found or not executable\n");
  exit_value = -1;
  free(path_copy);
  free(full_path);
  return NULL;
}