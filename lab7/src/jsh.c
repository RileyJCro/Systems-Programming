#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include "fields.h"
#include <fcntl.h>

/* jsh.c (Part 1 and 2)
 * Riley Crockett
 * November 9, 2022
 *
 * This program implements a basic shell. It takes a prompt name as an optional
 * command line argument, tries to execute lines from stdin, and supports 
 * I/O file redirect with '<', '>', and '>>'.
 * */

typedef struct redirect {
  char *name;
  int flag;
  int xit;
  mode_t mode;
} *Redirect;

/* @name: InitRedirect
   @brief: Initializes struct values for file redirection.
   @param[in] r: The struct to initialize.
   @param[in] name: The name of the file.
   @param[in] flag: The flag value for open().
   @param[in] xit: The exit value when an error occurs in open() or dup2().
   @param[in] mode: The permissions/mode for open().
   */
void InitRedirect(Redirect r, char *name, int flag, int xit, mode_t mode) {
  r->name = name;
  r->flag = flag;
  r->xit = xit;
  r->mode = mode;
}

int IsRedirect(char *arg) {
  return (!strcmp(arg, "<") || !strcmp(arg, ">") || !strcmp(arg, ">>"));
}

/* @name: SetupRedirect
   @brief: Determines the redirection type and creates/initializes a struct.
   @param[in] type: The redirection symbol.
   @param[in] name: The name of the file.
   @param[out]: Returns the initialized struct.
   */
Redirect SetupRedirect(char *type, char *name) {
  Redirect r = malloc(sizeof(Redirect));
  if (!strcmp(type, "<")) {
    InitRedirect(r, name, O_RDONLY, 1, 0444);
  } else {
	InitRedirect(r, name, (O_WRONLY | O_CREAT), 2, 0644);
	if (!strcmp(type, ">")) r->flag |= O_TRUNC;
    if (!strcmp(type, ">>")) r->flag |= O_APPEND;
  }
  return r;
}

/* @name: FindRedirects
   @brief: Loops through the cmd, finding redirects and making an argument list.
   @param[in] is: An input struct with the command.
   @param[in] arg_list: The arguments for execvp().
   @param[in] r_arr: The array for redirection structs.
   @param[out]: Returns the number of redirections found.
   */
int FindRedirects(IS is, char *arg_list[], Redirect r_arr[]) {
  int i, n_redirs = 0;
  for (i = 0; i < is->NF; i++) {
	if (IsRedirect(is->fields[i])) {
      r_arr[n_redirs++] = SetupRedirect(is->fields[i], is->fields[i+1]);
	} else if (n_redirs == 0) {
	  arg_list[i] = is->fields[i];
	  arg_list[i+1] = NULL;
    }
  }
  return n_redirs;
}

/* @name: ProcessCMD
   @brief: Calls fork() to create a process. If the fork returns 0, it creates 
           the new child process. Otherwise the calling process is the parent,
		   and returns or waits for the child based on the background parameter.
   @param[in] is: An input struct with the command.
   @param[in] bg: A boolean for background process.
   */
void ProcessCMD(IS is, int bg) {
  int i, status, n_redirects, fd;
  char *arg_list[is->NF];
  Redirect r_arr[2];
  pid_t pid = fork();

  if (bg == 0) is->fields[--is->NF] = NULL;
  
  if (pid == 0) {
    n_redirects = FindRedirects(is, arg_list, r_arr);
	for (i = 0; i < n_redirects; i++) {
      fd = open(r_arr[i]->name, r_arr[i]->flag, r_arr[i]->mode);
      if (fd < 0) exit(r_arr[i]->xit);
      if (dup2(fd, r_arr[i]->xit-1) != r_arr[i]->xit-1) exit(1);
      close(fd);
	}
    if (execvp(is->fields[0], arg_list) == -1) {
	  perror(is->fields[0]);
	  exit(1);
    }
  }
  if (bg == 0) return;
  while (wait(&status) != pid);
}

int main(int argc, char** argv) {
  char *prompt;
  IS is = new_inputstruct(NULL);
  
  prompt = (argv[1]) ? strdup(argv[1]) : strdup("jsh1: ");
  if (prompt[0] == '-') prompt[0] = '\0';  
  printf("%s", prompt); 

  while (get_line(is) > 0) {
    if (is->NF > 0) {	
	  (!strcmp(is->fields[is->NF-1],"&")) ? ProcessCMD(is,0) : ProcessCMD(is,1);
      printf("%s", prompt);
    } 
  }
  
  free(prompt);
  jettison_inputstruct(is);
  return 0;
}
