#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include "fields.h"
#include <fcntl.h>

/* jsh.c (Part 3)
 * Riley Crockett
 * November 22, 2022
 *
 * This program implements a basic shell. It takes a prompt name as an optional
 * command line argument, attempts to execute commands from stdin, and supports
 * I/O file redirects with '<', '>', '>>', '|'.
 * */

int IsRedirect(char *arg) {
  return (!strcmp(arg, "<") || !strcmp(arg, ">") || !strcmp(arg, ">>"));
}

void CallExecvp(int start, IS is) {
  if (execvp(is->fields[start], is->fields+start) == -1) {
    perror(is->fields[start]);
    exit(1);
  }
}

/* @name: ProcessRedirect
   @brief: Sets the open modes for a file redirect and calls dup2.
   @param[in] type: The type of file redirect.
   @param[in] fname: The filename.
   */
void ProcessRedirect(char *type, char *fname) {
  int fd, flag, xit;
  mode_t mode;

  if (!strcmp(type, "<")) {
    flag = O_RDONLY; xit = 1; mode = 0444;
  } else {
    flag = (O_WRONLY | O_CREAT); xit = 2; mode = 0644;
    flag |= (!strcmp(type, ">")) ? O_TRUNC : O_APPEND;
  }
  if ((fd = open(fname, flag, mode)) < 0) exit(xit);
  if (dup2(fd, xit-1) != xit-1) exit(1);
  close(fd);
}

/* @name: ProcessSubCMD
   @brief: Loops through a sub command, processes commands and calls execvp.
   @param[in] start: The loop start in fields.
   @param[in] is: The input struct with the whole command.
   */
void ProcessSubCMD(int start, IS is) {
  for (int i = start; is->fields[i] != NULL; i++) {
    if (IsRedirect(is->fields[i])) {
	  ProcessRedirect(is->fields[i], is->fields[i+1]);
      is->fields[i] = NULL;
    }
  }
  CallExecvp(start, is);
}

/* @name: ParsePipes
   @brief: Writes NULL at every pipe and stores pipe argument indices.
   @param[in] is: The input struct with the command.
   @param[in] p_cmds[]: The indices of pipe commands.
   @param[out] n_pipes: The number of pipes.
   */
int ParsePipes(IS is, int p_cmds[]) {
  int i, n_pipes = 0;
  for (i = 0; i < is->NF; i++) {
    if (!strcmp(is->fields[i], "|")) {
      is->fields[i] = NULL;
      if (n_pipes == 0) p_cmds[n_pipes++] = 0;
      p_cmds[n_pipes++] = i+1;
    }
  }
  return n_pipes;
}

/* @name: PrevPipeToInput
   @brief: Redirects the previous pipe to the current pipe's input.
   @param[in] prev_pfd: The previous pipe file descriptor.
   */
void PrevPipeToInput(int prev_pfd) {
  if (prev_pfd != 0) {
    dup2(prev_pfd, 0);
    close(prev_pfd);
  }
}

/* @name: ProcessCMD
   @brief: Forks each pipe (or once for none), processes subcommands, 
           and connects all of the pipes.
   @param[in] is: An input struct with the command.
   @param[in] bg: A boolean for background process.
   */
void ProcessCMD(IS is, int bg) {
  if (bg == 0) is->fields[--is->NF] = NULL;
  is->fields[is->NF] = NULL;

  int pipefd[2];
  int p_cmds[30];
  int status, i = 0, prev_pfd = 0, n_pipes = ParsePipes(is, p_cmds);
  pid_t pid;

  if (n_pipes == 0) {
    if ((pid = fork()) == 0) ProcessSubCMD(i, is);
  } else {
    for (i = 0; i < n_pipes-1; i++) {
      pipe(pipefd);
      if (fork() == 0) {
        PrevPipeToInput(prev_pfd);
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        ProcessSubCMD(p_cmds[i], is);
      }
      close(prev_pfd);
      close(pipefd[1]);
      prev_pfd = pipefd[0];
    }
    PrevPipeToInput(prev_pfd);
    if ((pid = fork()) == 0) ProcessSubCMD(p_cmds[i], is);
  }
  if (bg == 0) return;
  while ((wait(&status)) != pid);
}

int main(int argc, char** argv) {
  IS is = new_inputstruct(NULL);
  char *prompt = (argv[1]) ? strdup(argv[1]) : strdup("jsh1: ");
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
}
