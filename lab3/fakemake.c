#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dllist.h"
#include "fields.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* fakemake.c
   Riley Crockett
   9/26/2022

   This program helps automate the compiling of a single executable.
   It reads file information from a .fm file, and compiles the necessary
   files for the executable.
   */

/* @name: ListStore
   @brief: This loops through an input line and stores words in a Dllist.
   @param[in] l: A Dllist.
   @param[in] is: An input struct. */

void ListStore(Dllist *l, IS is) {
  for (int i=1; i < is->NF; i++) {
	dll_append(*l, new_jval_s(strdup(is->fields[i])));
  }
}

/* @name: ProcessFiles
   @brief: This traverses a list of header or .c files, flags remakes,
           and updates the maximum modification time of header and .o files.
   @param[in] l: A Dllist with header or C file names.
   @param[in] max_ht: The maximum header modification time. 
   @param[in] max_ot: The maximum .o file modification time. 
   @param[in] rmks: A Dllist for the remakes.
   @param[in] spec: Is 0 for header files, and 1 for C files.
   @param[out]: Returns 1 if a C or header file does not exist, 0 otherwise. */

int ProcessFiles(Dllist l, time_t *max_ht, time_t *max_ot, Dllist *rmks, int spec) {
  char *cf, *obj;
  Dllist lt;
  struct stat buf;
  int exists, ct;

  dll_traverse(lt, l) {
    exists = stat(lt->val.s, &buf);
	if (exists < 0) {
	  fprintf(stderr, "fmakefile: %s: No such file or directory\n", lt->val.s);
	  return 1;
	} else { 
	  if (spec == 0) {
	    if (*max_ht < buf.st_mtime) { *max_ht = buf.st_mtime; }
	  } else if (spec == 1) {
	    cf = strdup(lt->val.s);
	    obj = strdup(lt->val.s); obj[strlen(lt->val.s)-1] = 'o';
	    ct = buf.st_mtime;
	    exists = stat(obj, &buf);
	    if (exists < 0) {
		  dll_append(*rmks, new_jval_s(strdup(cf)));
	    } else {
	      if (buf.st_mtime < *max_ht || buf.st_mtime < ct) {
		    dll_append(*rmks, new_jval_s(strdup(cf)));
	 	  }
		  if (*max_ot < buf.st_mtime) { *max_ot = buf.st_mtime; }
	    }
	  }
	}
  }
  return 0;
}

/* @name: RemakeCFile
   @brief: This creates a string for compiling a .c file.
   @param[in] dt: A dllist node for traversing.
   @param[in] f: A dllist with compile flags. 
   @param[in] c_name: The name of the .c file. 
   @param[out]: Returns a 1 if an error occured, otherwise returns 0. */

int RemakeCFile(Dllist dt, Dllist f, char *c_name) {
  char rmk_str[100];
  strcpy(rmk_str, "gcc -c");
  dll_traverse(dt, f) { strcat(rmk_str, " "); strcat(rmk_str, dt->val.s); }
  strcat(rmk_str, " "); strcat(rmk_str, c_name);
  
  printf("%s\n", rmk_str);
  int ret = system(rmk_str);
  
  if (ret != 0) {
    if (ret == -1 || ret == 127 || ret == 1) {
      fprintf(stderr, "Command failed.  Fakemake exiting\n");
    } else {
      fprintf(stderr, "Command failed.  Exiting\n");
	}
	return 1;
  } 
  return 0;
}

/* @name: CheckExec
   @brief: This checks the conditions for an executable to be made.
   @param[in] e: The name of the executable.
   @param[in] max_otime: The maximum modification time of the .o files.
   @param[in] cmod: Is 0 if no .c files were remade, or 1 if any were remade.
   @param[out]: Returns 1 if the executable needs to be remade, otherwise 0. */

int CheckExec(char *e, time_t max_otime, int cmod) {
  int exists;
  struct stat buf;
  
  exists = stat(e, &buf);
  if (exists < 0) { return 1; }
  if (cmod == 1) { return 1; }
  if (buf.st_mtime < max_otime) { return 1; }
  return 0;
}

/* @name: RemakeExec
   @brief: This creates a string for creating an executable.
   @param[in] ex: The executable name.
   @param[in] dt: A dllist node for traversing.
   @param[in] f: A dllist with compile flags.
   @param[in] l: A dllist with libraries.
   @param[in] c: A dllist with .c files.
   @param[out]: Returns a 1 if an error occured, otherwise returns 0. */

int RemakeExec(char *ex, Dllist dt, Dllist f, Dllist l, Dllist c) {
  char rmk_str[100];
  strcpy(rmk_str, "gcc -o "); strcat(rmk_str, ex);
  dll_traverse(dt, f) { strcat(rmk_str, " "); strcat(rmk_str, dt->val.s); }
  dll_traverse(dt, c) {
	strcat(rmk_str, " "); 
	strncat(rmk_str, dt->val.s, strlen(dt->val.s)-1);
	strcat(rmk_str, "o");
  }
  dll_traverse(dt, l) { strcat(rmk_str, " "); strcat(rmk_str, dt->val.s); }

  printf("%s\n", rmk_str);
  int ret = system(rmk_str);
  
  if (ret != 0) {
    if (ret == -1 || ret == 1) { 
	  fprintf(stderr, "Command failed.  Exiting\n");
	} else {
	  fprintf(stderr, "Command failed.  Fakemake exiting\n");
    }
	return 1;
  }
  return 0;
}

/* @name: FreeLists
   @brief: This function frees specified dllists.
   @param[in] c, h, f, l, rmks: 
   C files, header files, flags, libraries, and file remakes. */

void FreeLists(Dllist *c, Dllist *h, Dllist *f, Dllist *l, Dllist *rmks) {
  free_dllist(*c);
  free_dllist(*h);
  free_dllist(*f);
  free_dllist(*l);
  free_dllist(*rmks);
}

int main(int argc, char *argv[]) {
  char *fname;
  IS is;

  /* Check fakemake's usage, and if the fm file exists. */

  if (argc == 1) { fname = strdup("fmakefile"); }
  if (argc == 2) { fname = strdup(argv[1]); }
  if (argc >= 3) {
    fprintf(stderr, "usage: fakemake [ description - file ]\n");
	free(fname);
	return -1;
  }
  is = new_inputstruct(fname);
  if (is == NULL) {
    fprintf(stderr, "fakemake: %s: No such file or directory\n", fname);
    free(fname);
	jettison_inputstruct(is);
	return -1;
  }

  char *e = '\0';
  Dllist c = new_dllist(), h = new_dllist(), f = new_dllist(), l = new_dllist();
  Dllist dt;
  Dllist c_remakes = new_dllist();
  time_t max_htime = 0, max_otime = 0;
  int cmod = 0;

  /* Traverse the .fm file and store files in dllists. */
  
  while (get_line(is) >= 0) {
	if (is->NF == 0) { continue; }
    if (!strcmp(is->fields[0], "E") && e != NULL) {
      fprintf(stderr, "fmakefile (%d) cannot have more than one E line\n", is->line);
      FreeLists(&c, &h, &f, &l, &c_remakes);
	  jettison_inputstruct(is);
	  return -1;
	}
    if (!strcmp(is->fields[0], "E")) { e = strdup(is->fields[1]); }
	if (!strcmp(is->fields[0], "C")) { ListStore(&c, is); }
    if (!strcmp(is->fields[0], "H")) { ListStore(&h, is); }
    if (!strcmp(is->fields[0], "F")) { ListStore(&f, is); }
    if (!strcmp(is->fields[0], "L")) { ListStore(&l, is); }
  }

  if (e == NULL) {
	fprintf(stderr, "No executable specified\n");
	FreeLists(&c, &h, &f, &l, &c_remakes);
	jettison_inputstruct(is);
	return -1;
  }

  jettison_inputstruct(is);

  /* Check if the header and C files exist and flag remakes. */

  if (ProcessFiles(h, &max_htime, &max_otime, &c_remakes, 0)) { 
	FreeLists(&c, &h, &f, &l, &c_remakes);
	return -1;
  }
  if (ProcessFiles(c, &max_htime, &max_otime, &c_remakes, 1)) { 
	FreeLists(&c, &h, &f, &l, &c_remakes);
	return -1;
  }
  if (!dll_empty(c_remakes)) { cmod = 1; }

  /* Remake the C files and executable if needed. */

  dll_traverse(dt, c_remakes) {
	if (RemakeCFile(dt, f, dt->val.s) == 1) { 
	  FreeLists(&c, &h, &f, &l, &c_remakes);
	  return -1;
	} 
  }
  if (CheckExec(e, max_otime, cmod) == 1) {
	if (RemakeExec(e, dt, f, l, c) == 1) {
	  FreeLists(&c, &h, &f, &l, &c_remakes);
	  return -1;
	}
  } else { printf("%s up to date\n", e); }

  FreeLists(&c, &h, &f, &l, &c_remakes);

  exit(0);
}
