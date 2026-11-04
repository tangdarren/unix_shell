// Author:  Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2023
// Revised: John Shawger <shawgerj@cs.wisc.edu>, Spring 2024
// Revised: Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2024
// Revised: Leshna Balara <lbalara@cs.wisc.edu>, Spring 2025

#ifndef WSH_H_
#define WSH_H_

#include <sys/types.h>

#define MAXLINE 1024 /* max line size */
#define MAXARGS 128  /* max args on a command line */

char prompt[] = "wsh> "; /* prompt */
char cwd[MAXLINE];       /* current working directory */

// /**
//  * Functions declarations, see .c file for description
//  */
void check_params(char **argv, int argc);
void interactive_main(void);
int batch_main(char *scriptFile);
void eval(char *cmdline);
void eval_pipe(char *cmdline);
char *do_variable_substitution(const char *command);
char *replaceCommandSubstitution(const char *command);
void parseline_no_subst(const char *cmdline, char **argv, int *argc);
void exit_built_in(int argc);
void export_built_in(char **argv);
void local_built_in(char **argv);
void vars_built_in();
void ls_built_in();
void ps_built_in();
char *find_path(const char *command);
#endif
