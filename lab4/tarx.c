#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <fcntl.h>
#include <utime.h>
#include <unistd.h>
#include <limits.h>
#include "jrb.h"
#include "dllist.h"

/* tarc.x
   Riley Crockett
   10/12/2022

   This program reads/takes a .tarc file as std input and extracts/creates
   the files and directories that it specifies.
   */

/* @name: FileStruct
   @brief: A struct for storing a file's information.
  */
typedef struct fs {
  int name_len;
  char *name;
  long inode;
  unsigned int mode;
  long mod_time;
  long old_mod_time;
  long size;
  char *bytes;
  JRB children;
  JRB hard_links;

} FileStruct;

/* @name: StoreFile
   @brief: Stores a regular file's information under its parent
           directory in a JRB tree. directory tree.
   @param[in] fs: A struct with the file's information.
   @param[in] dir_tree: The directory tree.
   */
void StoreFile(FileStruct *fs, JRB dir_tree) {
  char *name_cpy = strdup(fs->name);
  char *parent = strdup(dirname(name_cpy));
  JRB P = jrb_find_str(dir_tree, parent);

  if (P != NULL) {
    FileStruct *par = (FileStruct *) P->val.v;
    if (par->children == NULL) {
      par->children = make_jrb();
    }
    jrb_insert_str(par->children, fs->name, new_jval_v((void *)fs));
  }
}

/* @name: LinkUpdate
   @brief: Recursively checks for hard links and sets them.
           Then updates file modification times and permissions.
   @param[in] dir: The current directory.
   */
void LinkUpdate(JRB dir) {
  JRB dp, hl;
  FileStruct *f;
  struct utimbuf ubuf;

  jrb_traverse(dp, dir) {
    f = (FileStruct *) dp->val.v;
    if (f->hard_links != NULL) {

      if (f->children != NULL) { LinkUpdate(f->children); }

      jrb_traverse(hl, f->hard_links) {
        FileStruct *l = (FileStruct *) hl->val.v;
        link(f->name, l->name);
      }
    }
  }
  jrb_traverse(dp, dir) {
    f = (FileStruct *) dp->val.v;
    ubuf.modtime = f->old_mod_time;
    utime(f->name, &ubuf);
    chmod(f->name, f->mode);
  }
}

/* @name: StoreFile
   @brief: Stores a regular file's information under its parent
           directory in a JRB tree. directory tree.
   @param[in] fs: A struct with the file's information.
   @param[in] dir_tree: The directory tree.
   */
void BuildDir(JRB dir) {
  JRB dp;
  FileStruct *f;
  FILE *file;

  jrb_traverse(dp, dir) {
    f = (FileStruct *) dp->val.v;
    if (S_ISDIR(f->mode)) {
      mkdir(f->name, 0777);
      if (f->children != NULL) { BuildDir(f->children); }
    }
    else if (S_ISREG(f->mode)) {
      chmod(f->name, 0777);
      file = fopen(f->name, "wb");
      fwrite(f->bytes, f->size, 1, file);
      fclose(file);
    }
  }
}

/* @name: PrintErrorMSG
   @brief: Prints a read error based on given parameters.
   @param[in] name: A filename.
   @param[in] type: The type of error associated with a filename.
   @param[in] pos: The current byte position in the tarc.
   @param[in] tried_sz: The attempted read size in bytes.
   @param[in] read_sz: The actual number of bytes read.
   */
int PrintErrorMSG(char *name, char *type, long pos, int tried_sz, int read_sz) {
  if (name != NULL) {
    fprintf(stderr, "Bad tarc file for %s.  Couldn't read %s\n", name, type);
  } else {
    fprintf(stderr, "Bad tarc file at byte %ld.  ", pos);
    fprintf(stderr, "Tried to read %d but bytes read = %d.\n", tried_sz, read_sz);
  }
  return -1;
}

/* @name: ReadFromTar
   @brief: Reads the tarc and stores files in the directory tree.
   @param[in] i: The elements read from the last read.
   @param[in] dir_tree: The directory tree.
   @param[in] inodes: A tree for storing inodes.
   */
int ReadFromTar(int i, JRB dir_tree, JRB inodes) {
  FileStruct *fs;
  fs = malloc(sizeof(FileStruct));

  /* Read the name length */
  i = fread(&fs->name_len, 1, 4, stdin);
  if (i == 3) { PrintErrorMSG(NULL, NULL, ftell(stdout), 4, i); }

  /* Read the file name. */
  char *tmp_name = malloc(sizeof(char)*(fs->name_len)+1);
  i = fread(tmp_name, 1, fs->name_len, stdin);
  if (i != fs->name_len) {
    return PrintErrorMSG(NULL, NULL, ftell(stdout), fs->name_len, i);
  }
  fs->name = strdup(tmp_name);

  /* Read the inode number */
  if (i > 0 && !fread(&fs->inode, 8, 1, stdin)) {
    return PrintErrorMSG(fs->name, "inode", -1, -1, -1);
  }

  /* If the inode is already in the tree, store as a hardlink.
     Otherwise insert the inode to the inode tree. */
  JRB f = jrb_find_int(inodes, fs->inode);
  if (f != NULL) {
    FileStruct *o = (FileStruct *) f->val.v;
    jrb_insert_str(o->hard_links, fs->name, new_jval_v((void *) fs));
    return i;
  } else {
    jrb_insert_int(inodes, fs->inode, new_jval_v((void *) fs));
    fs->hard_links = make_jrb();
  }

  /* Read the file mode and last modification time. */
  if (i > 0 && !fread(&fs->mode, 4, 1, stdin)) {
    return PrintErrorMSG(fs->name, "mode", -1, -1, -1);
  }
  if (i > 0 && !fread(&fs->mod_time, 8, 1, stdin)) {
    return PrintErrorMSG(fs->name, "mod time", -1, -1, -1);
  }
  fs->old_mod_time = fs->mod_time;

  /* If the file is regular, read the size/bytes and store the file under
     a directory. Otherwise if it is a directory add it to the tree. */
  if (S_ISREG(fs->mode)) {
    if (i > 0 && !fread(&fs->size, 8, 1, stdin)) {
      return PrintErrorMSG(fs->name, "size", -1, -1, -1);
    }
    fs->bytes = malloc(sizeof(char)*(fs->size)+1);
    if (i > 0 && fread(fs->bytes, 1, fs->size, stdin) != fs->size) {
      return PrintErrorMSG(fs->name, "EOF", -1, -1, -1);
    }
    StoreFile(fs, dir_tree);
  }
  else if (S_ISDIR(fs->mode) && i > 0) {
    jrb_insert_str(dir_tree, fs->name, new_jval_v((void *) fs));
  }
  return i;
}

int main() {
  JRB dir_tree = make_jrb();
  JRB inodes = make_jrb();
  int ret = 1;

  /* Reads the tarc and returns when done, or exits on an error. */
  while (ReadFromTar(ret, dir_tree, inodes)) {}
  if (ret == -1) { return -1; }

  /* Recursively makes the directories/files. */
  BuildDir(dir_tree);

  /* Creates hardlinks and updates the modification times/permissions. */
  LinkUpdate(dir_tree);

  return 0;
}