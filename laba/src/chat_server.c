#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "sockettome.h"
#include "dllist.h"
#include "jrb.h"

/* chat_server.c
 * Riley Crockett
 * 12/11/2022
 *
 * This program is a chat server utilizing pthreads and sockets.
 * For manual testing:    - Run ./chatserver port roomname1...
 *                        - Run nc hostname port '
 * */

/* @name: RTS
   @brief: A struct for room threads.
   @param lock: The lock mutex.
   @param cv: The condition variable.
   @param name: The room name.
   @param list: A list of messages.
   @param clients: A list of client threads.
   */
typedef struct {
  pthread_mutex_t *lock;
  pthread_cond_t *cv;
  char *name;
  Dllist *list;
  Dllist *clients;
} RTS;

/* @name: CTS
   @brief: A struct for client threads.
   @param fd: The file descriptor for the client.
   @param fp: The reading stream on the client.
   @param name: The client's name.
   */
typedef struct {
  int fd;
  FILE *fp;
  char *name;
} CTS;

/* The file descriptor for the newest unnamed client. */
int cur_fd;
/* A tree for sorting and comparing room names. */
JRB Rooms;

/* @name: PrintToClients
   @brief: Prints messages from a room's list to all clients in the room.
   @param[in] rts: The room thread struct.
   */
void PrintToClients(RTS *rts) {
  Dllist clist = *rts->clients;
  Dllist msgs = *rts->list;
  Dllist m, c;
  dll_traverse(c, clist) {
  CTS *cts = (CTS *) jval_v(c->val);
  if (cts->fp != NULL) fflush(cts->fp);
    if (cts->fd == cur_fd) cts->fp = fdopen(cts->fd, "a+");
    dll_traverse(m, msgs) fputs(m->val.s, cts->fp);
    fflush(cts->fp);
  }
  while (!dll_empty(msgs)) dll_delete_node(dll_first(msgs));
}

/* @name: ProcessRoom
   @brief: This makes a room wait for new messages to print.
   @param[in] rts: The room thread struct.
   */
void ProcessRM(RTS *rts) {
  PrintToClients(rts);
  pthread_cond_wait(rts->cv, rts->lock);
  ProcessRM(rts);
}

/* @name: InitRoom
   @brief: The start routine of pthread_create for the room threads.
   @param[in] arg: A pointer to the start of the rts array.
   */
void *InitRoom(void *arg) {
  RTS *rts = (RTS *) arg;
  pthread_mutex_lock(rts->lock);
  pthread_cond_wait(rts->cv, rts->lock);
  ProcessRM(rts);
  return NULL;
}

/* @name: NewMsg
   @brief: This puts a new message on the room's list and notifies the room.
   @param[in] name: The client's name.
   @param[in] msg: The message from the client.
   @param[in] rts: The room thread struct.
   @param[in] cj: A boolean for a client joining.
   */
void NewMsg(char *name, char *msg, RTS *rts, int cj) {
  char *new_msg;
  pthread_mutex_lock(rts->lock);
  if (msg == NULL) {
    new_msg = malloc(sizeof(char)*(strlen(name)+12));
    strcpy(new_msg, name);
    (cj == 1) ? strcat(new_msg," has joined\n") : strcat(new_msg," has left\n");
  } else {
    new_msg = malloc(sizeof(char)*(strlen(name)+strlen(msg)+3));
    strcpy(new_msg, name);
    strcat(new_msg, ": ");
    strcat(new_msg, msg);
  }
  dll_append(*rts->list, new_jval_s(new_msg));
  pthread_cond_signal(rts->cv);
  pthread_mutex_unlock(rts->lock);
}

/* @name: ListenClient
   @brief: Attempts to read input from the client.
   @param[in] name: The client's name.
   @param[in] fp: The read stream on the client.
   @param[in] rts: The room thread struct the client is in.
   */
void ListenClient(char *name, FILE *fp, RTS *rts) {
  char msg[1024];
  if (fgets(msg, 1024, fp) != NULL) {
    NewMsg(name, msg, rts, -1);
    fflush(fp);
    ListenClient(name, fp, rts);
  }
  fflush(fp);
}

/* @name: ClientExiting
   @brief: This is called after ListenClient() returns from EOF or an error.
           This removes the client from the room's list, closes the output
           stream, and calls NewMsg() to tell the room that client has left.
   @param[in] name: The client's name.
   @param[in] rts: The room thread struct.
   */
void ClientExiting(char *name, RTS *rts) {
  pthread_mutex_lock(rts->lock);
  Dllist l = *rts->clients, tmp;
  dll_traverse(tmp, l) {
	CTS *cts = (CTS *) jval_v(tmp->val);
	if (!strcmp(cts->name, name)) {
	  fflush(cts->fp);
	  fclose(cts->fp);
	  dll_delete_node(tmp);
	  break;
	}
  }
  pthread_mutex_unlock(rts->lock);
  NewMsg(name, NULL, rts, 0);
}

/* @name: InitClient
   @brief: The start routine of pthread_create for the clients.
           This prints a welcome message to the new client and prompts them
		   for a chat name and room. After they've entered their
		   name and room it adds them to the room and listens for more input.
   @param[in] arg: A pointer to the start of the room thread struct array.
   */
void *InitClient(void *arg) {
  FILE *fp = fdopen(cur_fd, "w+");
  Dllist l, tmp;
  RTS *rts;
  CTS cts;
  JRB r;
  
  fputs("Chat Rooms:\n\n", fp);
  jrb_traverse(r, Rooms) {
    rts = (RTS *) r->val.v;
    fputs(rts->name, fp);
    fputs(":", fp);
    l = *rts->clients;
    dll_traverse(tmp, l) {
      CTS *c = (CTS *) jval_v(tmp->val);
      fputs(" ", fp);
      fputs(c->name, fp);
    }
    fputs("\n", fp);
  }
  char name[32];
  char room[32];
  fputs("\nEnter your chat name (no spaces):\n", fp);
  fgets(name, 32, fp);
  fputs("Enter chat room:\n", fp);
  fgets(room, 32, fp);
  name[strcspn(name, "\n")] = 0;
  room[strcspn(room, "\n")] = 0;

  jrb_traverse(r, Rooms) {	
    rts = (RTS *) r->val.v;
    if (!strcmp(room, rts->name)) {
      cts.name = strdup(name);
      cts.fd = cur_fd;
      dll_append(*rts->clients, new_jval_v(&cts));
      NewMsg(name, NULL, rts, 1);
      break;
    }
  }
  ListenClient(name, fp, rts);
  fflush(fp);
  fclose(fp);
  ClientExiting(name, rts);
  return NULL;
}

/* @name: main
   @brief: This serves a socket and acts as the main thread.
   It initializes room threads and accepts incoming client 
   connections on the socket.
   */
int main(int argc, char** argv) {
  int i, port = atoi(argv[1]), nrt = argc-2;
  int socket = serve_socket(port);
  pthread_t rmtid, ctid;
  pthread_mutex_t rm_locks[nrt];
  pthread_cond_t rm_cvs[nrt];
  Dllist msg_lists[nrt], client_lists[nrt];
  Rooms = make_jrb();
  RTS rts[nrt];
  
  for (i = 0; i < nrt; i++) {
    pthread_mutex_init(&rm_locks[i], NULL);
    pthread_cond_init(&rm_cvs[i], NULL);
    msg_lists[i] = new_dllist();
    client_lists[i] = new_dllist();
    rts[i].lock = &rm_locks[i];
    rts[i].cv = &rm_cvs[i];
    rts[i].name = argv[i+2];
    rts[i].list = &msg_lists[i];
    rts[i].clients = &client_lists[i];
    jrb_insert_str(Rooms, rts[i].name, new_jval_v(&rts[i]));
    pthread_create(&rmtid, NULL, InitRoom, rts+i);
  }

  while (1) {
    cur_fd = accept_connection(socket);
    pthread_create(&ctid, NULL, InitClient, rts);
  }

  return 0;
}
