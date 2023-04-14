#include <stdlib.h>
#include <string.h>
#include "fields.h"
#include "jrb.h"
#include "dllist.h"

/* famtree.c
   Riley Crockett
   9/3/2022

   This program reads people from standard input, stores them, establishes/error checks 
   their relationships, and then reprints their complete information.
   */

/* This struct defines a 'Person'. A person has strings to store their name and sex, 
   pointers to their parents, a list of children pointers, a visited field for DFS, 
   and a printed field for printing with BFS. */

typedef struct person {
  char *name;
  char *sex;
  struct person *father;
  struct person *mother;
  Dllist children;
  int visited;
  int printed;
} Person;

/* @name: CreateLink 
   @brief: Creates a link between two people based on their relationship. 
           It also error checks sex and parent assignments. 
   @param[in] line: The line number where the error occurs. 
   @param[in] rel: The relationship between the two people. 
   @param[in] op: The overlying person in the relationship. 
   @param[in] p: The current person. */

void CreateLink(int line, char *rel, Person *op, Person *p) {
  if (!strcmp(rel, "MOTHER_OF")) {
    if (!strcmp(op->sex, "Male")) {
      fprintf(stderr, "%s %d\n", "Bad input - sex mismatch on line", line);
      exit(1);
    }
    p->mother = op;
    strcpy(op->sex, "Female");
  }
  if (!strcmp(rel, "FATHER_OF")) {
    if (strcmp(op->sex, "Female") == 0) {
      fprintf(stderr, "%s %d\n", "Bad input - sex mismatch on line", line);
      exit(1);
    }
    p->father = op;
    strcpy(op->sex, "Male");
  }
  if (!strcmp(rel, "MOTHER")) {
    if (op->mother == NULL) {
      if (!strcmp(p->sex, "Male")) {
        fprintf(stderr, "%s %d\n", "Bad input - sex mismatch on line", line);
        exit(1);
      }
      op->mother = p;
      strcpy(p->sex, "Female");
    } 
	else {
      fprintf(stderr, "%s %d\n", "Bad input -- child with two mothers on line", line);
      exit(1);
    }
  }
  if (!strcmp(rel, "FATHER")) {
    if (op->father == NULL) {
      op->father = p;
      strcpy(p->sex, "Male");
    }
    else {
      fprintf(stderr, "%s %d\n", "Bad input -- child with two fathers on line", line);
      exit(1);
    }
  }
  if (!strcmp(rel, "MOTHER_OF") || !strcmp(rel, "FATHER_OF")) {
    dll_append(op->children, new_jval_v((void *) p));
  }
  if (!strcmp(rel, "MOTHER") || !strcmp(rel, "FATHER")) {
    dll_append(p->children, new_jval_v((void *) op));
  } 
}

/* @name: PrintPerson
   @brief: Prints a person's name, sex, parents, and children.
   @param[in] p: The person to print */

void PrintPerson(Person *p) {
  Dllist dt;
  Person *c;

  printf("%s\n  Sex: %s\n", p->name, p->sex);
  printf("  Father: ");
  (p->father == NULL) ? printf("Unknown\n") : printf("%s\n", p->father->name);
  printf("  Mother: ");
  (p->mother == NULL) ? printf("Unknown\n") : printf("%s\n", p->mother->name);
  printf("  Children:");
  if (dll_empty(p->children)) {
	printf(" None\n\n"); 
	return;
  }
  printf("\n");
  dll_traverse(dt, p->children) {
    c = (Person *) dt->val.v;
    printf("    %s\n", c->name);
  }
  printf("\n");
}

/* @name: Is_Descendant
   @brief: Uses DFS to determine if a person is a descendant of themselves. 
   @param[in] p: The person. 
   @return: Returns 1 if a person is a descendant of themselves, otherwise returns 0. */

int Is_Descendant(Person *p) {
  Dllist dt;
  Person *c;

  if (p->visited == 1) return 0;
  if (p->visited == 2) return 1;
  p->visited = 2;
  dll_traverse(dt, p->children) {
    c = (Person *) dt->val.v;
	if (Is_Descendant(c)) return 1;
  }
  p->visited = 1;
  return 0;
}

int main(int argc, char **argv) 
{
  Person *op;     /* The overlying person (Used to help link a person to their relatives) */
  Person *p;      /* The current person */
  JRB people;     /* A red/black tree to store person structs. */
  JRB pn;         /* A person node to index people in the tree. */
  Dllist pq;      /* A doubly-linked list to serve as a print queue. */
  Dllist dt;      /* A dllist object for traversing doubly-linked lists. */
  IS is;          /* An input struct to organize standard input. */
  int i;
  int name_len;   /* An integer for finding the length of a person's name. */
  char *tmp;      /* A temporary string to hold a person's name. */

  is = new_inputstruct(NULL);
  if (is == NULL) exit(1);
  people = make_jrb();
  pq = new_dllist();

  while (get_line(is) >= 0) {
	if (is->NF <= 1) continue;

	if (strcmp(is->fields[0], "SEX")) {
	  /* Find the person's name length to allocate a string, then store that name */

	  name_len = 0;
	  for (i = 1; i < is->NF; i++) name_len += strlen(is->fields[i])+1;	
	  tmp = malloc(sizeof(char)*(name_len+1));
	  strcpy(tmp, is->fields[1]);
	  name_len = strlen(is->fields[1]);

	  for (i = 2; i < is->NF; i++) {
	    tmp[name_len] = ' ';
		strcpy(tmp+name_len+1, is->fields[i]);
		name_len += strlen(tmp+name_len);
	  }

	  pn = jrb_find_str(people, tmp);

	  /* If the person is not already in the tree, allocate/initialize their values, 
	     then add them to the tree. Otherwise set a pointer to the existing person. */
	  
	  if (pn == NULL) {
		p = malloc(sizeof(Person));
		p->name = malloc(sizeof(char)*(name_len+1));
		strcpy(p->name, tmp);
		p->visited = 0;
		p->printed = 0;
		p->sex = malloc(sizeof(char)*(7));
        strcpy(p->sex, "Unknown");
		p->children = new_dllist();
		jrb_insert_str(people, p->name, new_jval_v((void *) p));	    
	  } 
	  else { p = (Person *) pn->val.v; }
	  
	  if (!strcmp(is->fields[0], "PERSON")) op = p;

	  /* Create links between people based on their relationship */

	  CreateLink(is->line, is->fields[0], op, p); 
	}
	else {
	  /* When a person's sex is read, error check and set their sex */

      if (!strcmp(is->fields[1], "F")) {
	    if (!strcmp(op->sex, "Male")) {
		  fprintf(stderr, "%s %d\n", "Bad input - sex mismatch on line", is->line);
		  exit(1);
		}
		strcpy(op->sex, "Female");
	  } 
	  if (!strcmp(is->fields[1], "M")) {
	    if (!strcmp(op->sex, "Female")) {
          fprintf(stderr, "%s %d\n", "Bad input - sex mismatch on line", is->line);
          exit(1);
        }	
		strcpy(op->sex, "Male");
	  }
	}

  }

  /* After reading, traverse the tree. Check if a person is their 
     own descendant, and insert people with no parents into a print queue. */
 
  jrb_traverse(pn, people) {
	p = (Person *) pn->val.v;
    if (Is_Descendant(p)) {
	  fprintf(stderr, "Bad input -- cycle in specification\n");
	  exit(1);
	}
	if (p->mother == NULL && p->father == NULL) dll_append(pq, new_jval_v((void *) p));
  }

  /* Print out people in the queue using BFS. If a person's parents have been
     printed (or are non-existent) and they have not been printed, print that 
	 person and move their children to the back of the queue. */
  
  while (!dll_empty(pq)) {
    p = (Person *) dll_first(pq)->val.v;
	dll_delete_node(dll_first(pq));
    
	if (p->printed == 0) {
	  if ((p->mother == NULL || p->mother->printed == 1) && (p->father == NULL || p->father->printed == 1)) {
		PrintPerson(p);
		p->printed = 1;
		if (dll_empty(p->children)) continue;
        
		dll_traverse(dt, p->children) {
		  op = (Person *) dt->val.v;
		  dll_append(pq, new_jval_v((void *) op));
		}
	  }
	}
  }

  jettison_inputstruct(is);
}
