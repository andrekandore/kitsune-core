#define _GNU_SOURCE 
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <search.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <kitsune.h>

#include "tree.h"

struct tree E_G(struct tnode) *store = NULL;
int connected_clients_count = 0;

struct tnode {
  char *key;
  char *val;
};

int
compare_tnode(struct tnode *a, struct tnode *b)
{
  return strcmp(a->key, b->key);
}

int 
init_listen_socket(int port)
{
  int rec_sock, status;
  struct sockaddr_in saddr;
  int i = 0, yes = 1;

  rec_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(rec_sock > 0);

  memset(&saddr, 0, sizeof(struct sockaddr_in));
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);

  status = setsockopt(rec_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  if (bind(rec_sock, (struct sockaddr *)&saddr, 
           sizeof(struct sockaddr_in)) < 0)
    abort();
  if (listen(rec_sock, 10) < 0)
    abort();

  return rec_sock;
}

void
handle_client(int client)
{
  const int line_size = 1000;
  char * line = calloc(sizeof(char), line_size);
  const char *delim = " \n";
  char *command = NULL, *arg1 = NULL, *arg2 = NULL, *savptr = NULL;
  int ret = 0;
  
  while (1) {
    kitsune_update("client");
    
    memset(line, 0, line_size);
    ret = recv(client, line, line_size, 0);
    if (ret == -1) {
      printf("%s\n", strerror(errno));
      abort();
    }
      

    /* parse command */
    command = strtok_r(line, delim, &savptr);
    if (command == NULL)        /* client sent EOF */
      break;
    arg1 = strtok_r(NULL, delim, &savptr);
    if (strcmp("set", command) == 0)
      arg2 = strtok_r(NULL, delim, &savptr);

    /* handle */
    if (strcmp("set", command) == 0) {
      struct tnode *n = calloc(1, sizeof(struct tnode));

      n->key = strdup(arg1);
      n->val = strdup(arg2);
      tree_delete(n, &store, compare_tnode);
      tree_search(n, &store, compare_tnode);
    } 
    else if (strcmp("get", command) == 0) {
      struct tnode n, *target;
      char *tmp = NULL;

      n.key = strdup(arg1);
      target = tree_search(&n, &store, compare_tnode);
      if (target == NULL)
        continue;
      
      sprintf(line, "%s\n", target->val);

      ret = send(client, line, strlen(line) + 1, 0);
      if (ret == -1) {
        printf("%s\n", strerror(errno));
        abort();
      }
    }
  }
  close(client);
  connected_clients_count++;
}

int 
main(int argc, char **argv) E_NOTELOCALS
{
  int server_sock = 0, client_sock = 0, 
    client_addrlen = sizeof(struct sockaddr_in);
  struct sockaddr_in client_addr;

  kitsune_do_automigrate();
  MIGRATE_LOCAL(server_sock);
  MIGRATE_LOCAL(client_sock);

  if (kitsune_is_updating())
    printf("Updating!\n");

  if (!kitsune_is_updating()) {
    server_sock = init_listen_socket(5000);
  }
  assert(server_sock > 0);

  if (kitsune_is_updating_from("client"))
    handle_client(client_sock);
  
  while (1) {
    kitsune_update("main");
    client_sock = accept(server_sock, &client_addr, 
                         &client_addrlen);
    if (client_sock < 0) {
      printf("%s\n", strerror(errno));
      abort();
    }
    handle_client(client_sock);
  }
  exit(0);
}
