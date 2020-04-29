#include "common_impl.h"

int main(int argc, char **argv)
{
  /* Récupération du PID du processus en cours*/
  pid_t pid;
  pid = getpid();

  /* Creation d'une socket pour se connecter au */
  /* au lanceur et envoyer/recevoir les infos */
  /* necessaires pour la phase dsm_init */
  int fd = socket(AF_INET,SOCK_STREAM,0);
  if (fd == -1) {
    printf("erreur a la creation de la socket\n");
    return 1;
  }

  struct addrinfo hints;
  struct addrinfo *result;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = 0;

  int rgai = getaddrinfo(argv[2],argv[1],&hints,&result);

  if (rgai != 0) {
     fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rgai));
     printf("%d\n",rgai );
     exit(EXIT_FAILURE);
  }

  /* Connexion au lanceur */
  int co = 0;
  do{
    co = connect(fd,result->ai_addr,result->ai_addrlen);
  }while ((co == -1) && ((errno == EINTR) || (errno == ECONNREFUSED)) );
  if (co == -1) {
    perror("erreur a la connexion");
    return 1;
  }

  /* Récupération du PID et rang */
  int rang = atoi(argv[3]);
  printf("%s\n", argv[3] );
  char mypid[6];
  sprintf(mypid, "%i", pid);
  dsm_proc_t proc_dist;
  proc_dist.pid = pid;
  proc_dist.connect_info.rank = rang;

  gethostname(proc_dist.connect_info.host,LEN_NAME);

  /* Création de la socket d'ecoute */
  int sock = creer_socket();
  ecoutesocket(sock);

  /* Récupération du port */
  int port = 0;
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);

  if (getsockname(sock, (struct sockaddr *)&sin, &len) == -1)
   perror("getsockname");
  else{
   port = htons(sin.sin_port);
  }
  char port_char[6];
  memset(port_char,0,6);
  sprintf(port_char, "%i", port);

  proc_dist.connect_info.port = port;

  /* Envoi de toutes les infos au lanceur */
  send(fd, &proc_dist, sizeof(dsm_proc_t), 0);

  /* Reception du nombre de processus*/
  int num_procs = 1;
  recv(fd, &num_procs, sizeof(num_procs), 0);

  /*Intialisation du tableau d'infos sur les autres processus*/
  dsm_proc_t proc_array[num_procs];

  /* Reception du proc_array de la part du lanceur */
  recv(fd, &proc_array, sizeof(proc_array), 0);
  int i = 0;

  char **args = malloc((num_procs*5)*sizeof(char*));
  int j = 0;
  char commande[1024] = "./pr204-7612/Phase2/";
  strcat(commande,argv[4]);

  char rang_char[32];
  sprintf(rang_char, "%d", rang);

  char num_procs_char[32];
  sprintf(num_procs_char, "%d", num_procs);

  /* sauvegarde d'un tableau d'argument sous forme de cahines de caracteres pour le execvp */
  for(i = 0 ;  i < num_procs*5 ;i = i+5){
    args[i] = malloc(32);
    args[i+1] = malloc(32);
    args[i+3] = malloc(32);
    args[i+4] = malloc(32);

    sprintf(args[i], "%i", proc_array[j].pid);
    sprintf(args[i+1], "%i", proc_array[j].connect_info.rank);
    args[i+2] = proc_array[j].connect_info.host;
    sprintf(args[i+3], "%i", proc_array[j].connect_info.port);
    sprintf(args[i+4], "%i", proc_array[j].connect_info.fd_sock);
    j++;
 }

  char **args_proc = malloc(((argc -2-1+5)+num_procs*5+1+1)*sizeof(char*));

  args_proc[0] = commande;
  args_proc[1] = num_procs_char;
  args_proc[2] = rang_char;

   for(i = 3 ; i < argc-2 ; i++){
     args_proc[i] = argv[i+2];
  }

  for(i = argc-2 ; i < (argc -2)+num_procs*5 ; i = i+5){
    args_proc[i] = args[i-(argc - 2)];
    args_proc[i+1] = args[i-(argc - 2)+1];
    args_proc[i+2] = args[i-(argc - 2)+2];
    args_proc[i+3] = args[i-(argc - 2)+3];
    args_proc[i+4] = args[i-(argc - 2)+4];
  }

  args_proc[(argc -2-1+5)+num_procs*5] = NULL;

  /* on execute la bonne commande */
  execvp(commande,args_proc);

  return 0;
}
