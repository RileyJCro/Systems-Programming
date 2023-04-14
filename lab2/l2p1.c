#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jrb.h"
#include "dllist.h"

/* l2p1.c
   Riley Crockett
   9/15/2022

   This program reads host information from the 'converted' file
   using buffered I/O routines. 
   */

/* This struct defines an IP with an address and list of names. */
typedef struct ip {
  unsigned char address[4];
  Dllist names;
} IP;

/* @name: PrintHosts
   @brief: Finds all entries in the tree with the given host name.
   @param[in] host_buf: The host name.
   @param[in] mn: A machine tree node.
   @param[in] machs: The machine tree.
   @param[in] ip: An ip pointer.
   @param[in] dt: A dllist name node. */
void PrintHosts(char *host_buf, JRB mn, JRB machs, IP *ip, Dllist dt)
{
  while (fscanf(stdin, "%s", host_buf) != EOF) {
    mn = jrb_find_str(machs, host_buf);    
	if (mn == NULL) printf("no key %s\n", host_buf);
    if (mn != NULL) {
	  jrb_traverse(mn, machs) {
        if (!strcmp(jval_s(mn->key), host_buf)) {
          ip = (IP *) mn->val.v;
          printf("%d.%d.%d.%d:", ip->address[0], ip->address[1], ip->address[2], ip->address[3]);
          dll_traverse(dt, ip->names) printf(" %s", dt->val.s);
          printf("\n");
        }
      }
	}
	printf("\nEnter host name: ");  
  }
}

/* @name: ReadNames
   @brief: Adds host names in a buffer to an ip.
   @param[in] buf: The host name buffer.
   @param[in] nnames: The number of names.
   @param[in] prev_i: The start of the name.
   @param[in] i: The current index.
   @param[in] end: The end of the buffer.  
   @param[out] : Returns the current index in the buffer. */
int ReadNames(char *buf, int nnames, int prev_i, int i, int end, IP *ip)
{
  char *prefix;
  char *name;
  int name_sz;
  int tmp = 0;

  while (i < end && nnames > 0) {
    if (buf[i] == '\0') {
	  for (tmp = prev_i; tmp < i; tmp++) if (buf[tmp] == '.') break;
	  if (tmp != i) {
        prefix = malloc(sizeof(char)*(tmp-prev_i+1));
        memcpy(prefix, buf+prev_i, tmp-prev_i);
        prefix[tmp-prev_i] = '\0';
        dll_append(ip->names, new_jval_s(prefix));
      }
      name_sz = i - prev_i;
      name = malloc(sizeof(char)*name_sz);
      memcpy(name, buf+prev_i, name_sz);
      dll_append(ip->names, new_jval_s(name));
      prev_i = i+1;
      nnames--;
	}
	i++;
  }
  return i;
}

int main()
{
  FILE *f;
  JRB machs, mn;
  Dllist dt;
  IP* ip;
  int i;
  int buf_index;
  int bufsz = 200;
  int nnames;
  char *buf;
  char *num_names;

  f = fopen("converted", "r");
  if (f == NULL) return -1;

  machs = make_jrb();

  i = 1;
  while (i > 0) {
    /* Initialize an IP, then read and set the address. */ 
    ip = malloc(sizeof(IP));
    ip->names = new_dllist();
	if (i > 0) i = fread(ip->address, 1, 4, f);

	/* Read the bytes for the number of names, then convert them to an integer. */
    num_names = malloc(sizeof(char)*4);
    if (i > 0) i = fread(num_names, 1, 4, f);
    nnames = num_names[3] + num_names[2]*10+num_names[1]*100+num_names[0]*1000;

	/* Allocate a buffer and read names. */
    buf = malloc(sizeof(char)*bufsz);
    if (i > 0) i = fread(buf, 1, bufsz, f);
	buf_index = ReadNames(buf, nnames, 0, 0, i, ip);
    fseek(f, buf_index-i, SEEK_CUR);

	/* Make an entry in the tree for each name. */
	dll_traverse(dt, ip->names) jrb_insert_str(machs, dt->val.s, new_jval_v((void *) ip));
  }

  fclose(f);

  /* Prompt the user for host names until EOF. */
  char input_buf[30];
  printf("Hosts all read in\n\n");
  printf("Enter host name: ");
  PrintHosts(input_buf, mn, machs, ip, dt);
}
