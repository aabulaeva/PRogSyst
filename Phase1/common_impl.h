#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>

#define LEN_NAME 128

/* autres includes (eventuellement) */
#define ERROR_EXIT(str) {perror(str);exit(EXIT_FAILURE);}

/* definition du type des infos */
/* de connexion des processus dsm */
struct connect_index{
  int rang;
  int *fd;
};
struct accept_index{
  int rang;
  int *fd;
};

struct dsm_proc_conn  {
   int rank;
   char host[128];
   int port;
   int fd_sock;
};
typedef struct dsm_proc_conn dsm_proc_conn_t;

/* definition du type des infos */
/* d'identification des processus dsm */
struct dsm_proc {
  pid_t pid;
  dsm_proc_conn_t connect_info;
};
typedef struct dsm_proc dsm_proc_t;

int creer_socket();
char** lecturemachine_file(char *name);
int nombreMachines(char *name);
int ecoutesocket(int sock);
int poll_dsmexec(int sock);
