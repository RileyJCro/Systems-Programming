#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include "jrb.h"
#include "dllist.h"

/* tarc.c
   Riley Crockett
   10/12/2022

   This program prints a given directory in .tarc format to stdout.
   */

/* @name: PrintFileNameInfo
   @brief: Prints a filename with its length, and inode number.
   */
void PrintFileNameInfo(char* name, long inode) {
  unsigned int len = strlen(name);
  fwrite(&len, 4, 1, stdout);
  printf("%.*s", len, name);
  fwrite(&inode, 8, 1, stdout);
}

/* @name: PrintFileModeTime
   @brief: Prints a file's mode and last modification time.
   @param[in] m: The file mode.
   @param[in] t: The last modification time.
   */
void PrintFileModeTime(unsigned int m, long t) {
  fwrite(&m, 4, 1, stdout);
  fwrite(&t, 8, 1, stdout);
}

/* @name: PrintFileSizeBytes
   @brief: Prints a file's size, then reads/prints its bytes.
   @param[in] fn: The filename.
   @param[in] size: The size of the file.
   */
void PrintFileSizeBytes(char *fn, long size) {
  FILE *f;
  char c;
  int i = 0;

  fwrite(&size, 8, 1, stdout);

  f = fopen(fn, "r");
  if (f == NULL) { return; }

  while ((c = fgetc(f)) != EOF && i < size) {
    printf("%c", c);
    i++;
  }
  fclose(f);
}

/* @name: MakeTarc
   @brief: Recursively traverses directories and prints file
           information in the .tarc format.
   @param[in] base: The basename of the current file/directory.
   @param[in] dir: The dirname of the current file/directory.
   @param[in] inodes: A tree for storing the inodes.
   */
void MakeTarc(char *base, char *dir, JRB inodes) {
  DIR *d;
  struct dirent *de;
  struct stat buf;
  int exists;
  char *s;
  Dllist directories, tmp;

  char full_name[PATH_MAX];
  strcpy(full_name, dir);
  strcat(full_name, "/");
  strcat(full_name, base);

  d = opendir(full_name);
  if (d == NULL) { return; }

  s = (char *) malloc(sizeof(char)*(strlen(full_name)+258));
  directories = new_dllist();

  for (de = readdir(d); de != NULL; de = readdir(d)) {
    if (strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, ".") == 0) {
      continue;
    }
    sprintf(s, "%s/%s", base, de->d_name);
    char abs[PATH_MAX];
    strcpy(abs, full_name);
    strcat(abs, "/");
    strcat(abs, de->d_name);

    exists = stat(abs, &buf);
    if (exists < 0) { continue; }

    PrintFileNameInfo(s, buf.st_ino);

    if (jrb_find_int(inodes, buf.st_ino) != NULL) { continue; }
    jrb_insert_int(inodes, buf.st_ino, JNULL);
    PrintFileModeTime(buf.st_mode, buf.st_mtime);

    if (S_ISREG(buf.st_mode)) {
      PrintFileSizeBytes(abs, buf.st_size);
    }
    else if (S_ISDIR(buf.st_mode)) {
      dll_append(directories, new_jval_s(strdup(s)));
    }
  }
  
  closedir(d);
  dll_traverse(tmp, directories) {
    MakeTarc(tmp->val.s, dir, inodes);
    free(tmp->val.s);
  }

  free_dllist(directories);
  free(s);
  return;
}

/* @name: InitializeRoot
   @brief: Checks if the initial directory argument in main exists,
           and sets the program up for recursively traversing it.
   @param[in] root: The name of the initial directory.
   @param[in] base_n: A char array address to store the basename.
   @param[in] dir_n: A char array address to store the dirname.
   @param[in] inodes: A tree for storing the inodes.
   */
void InitializeRoot(char *root, char **base_n, char **dir_n, JRB inodes) {
  char *full_name, *abs;
  struct stat buf;

  full_name = strdup(root);
  abs = strdup(root);
  *dir_n = strdup(dirname(root));
  *base_n = strdup(basename(full_name));

  if (stat(abs, &buf) >= 0) {
    jrb_insert_int(inodes, buf.st_ino, JNULL);
    PrintFileNameInfo(*base_n, buf.st_ino);
    PrintFileModeTime(buf.st_mode, buf.st_mtime);
  }
}

int main(int argc, char *argv[]) {
  char *base_name, *dir_name;
  JRB inodes = make_jrb();;

  if (argc < 2) { return -1; }

  InitializeRoot(argv[1], &base_name, &dir_name, inodes);

  MakeTarc(base_name, dir_name, inodes);

  return 0;
}