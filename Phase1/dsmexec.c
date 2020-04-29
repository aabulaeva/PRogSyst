#include "common_impl.h"

/* Variables globales */

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t proc_array[100];

/* Le nombre de processus effectivement crees */
volatile int num_procs_creat = 0;

void usage(void)
{
  fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
  fflush(stdout);
  exit(EXIT_FAILURE);
}

void sigchld_handler(int sig)
{
   /* On traite les fils qui se terminent */
   /* pour eviter les zombies */
  int val=0;
  do{
    val = waitpid(-1,NULL,WNOHANG);
  }while(val > 0);
}


int main(int argc, char *argv[])
{
  if (argc < 3){
    usage();
  } else {
    pid_t pid;
    int num_procs = 0;
    int i;

    /* Récupération du nom de la machine executant dsmexec*/
    char hostname[LEN_NAME];
    memset(hostname,0,LEN_NAME);
    gethostname(hostname,LEN_NAME);

    /* Mise en place d'un traitant pour recuperer les fils zombies*/
    /* XXX.sa_handler = sigchld_handler; */
    struct sigaction signal_sighold;
    memset(&signal_sighold,0,sizeof(signal_sighold));
    signal_sighold.sa_handler = sigchld_handler;
    signal_sighold.sa_flags = SA_RESTART;

    sigaction(SIGCHLD,&signal_sighold,NULL);

    /* Lecture du fichier de machines */

    /* Recuperation du nombre de processus a lancer */
    num_procs = nombreMachines(argv[1]);
    printf(" %i machines \n", num_procs);

    /* Recuperation des noms des machines */
    char **procs = lecturemachine_file(argv[1]);
    /* la machine est un des elements d'identification */

    /* Creation de la socket d'ecoute */
    int sock = creer_socket();

    /* Ecoute effective sur la socket */
    ecoutesocket(sock);

    /* Recuperation du port dynamique */
    int port = 0;
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sock, (struct sockaddr *)&sin, &len) == -1)
      perror("getsockname");
    else{
      port = htons(sin.sin_port);
    }
    char port_char[32];
    memset(port_char,0,32);
    sprintf(port_char, "%i", port);

    /* Creation du fdsOut et fdsErr pour le poll du père */
    struct pollfd *fdsOut = malloc(num_procs*sizeof(struct pollfd));
    memset(fdsOut,0,sizeof(struct pollfd));
    struct pollfd *fdsErr = malloc(num_procs*sizeof(struct pollfd));
    memset(fdsErr,0,sizeof(struct pollfd));

    for(i = 0; i < num_procs ; i++) {

      /* Création du tube pour rediriger stdout */
      int tabTubeOut[2];
      int tubeOut = pipe(tabTubeOut);
      if (tubeOut == -1) {
        perror("erreur tube out\n");
        return -1;
      }

      /* Création du tube pour rediriger stderr */
      int tabTubeErr[2];
      int tubeErr = pipe(tabTubeErr);
      if (tubeErr == -1) {
        perror("erreur tube err\n");
        return -1;
      }

      /* Création des fils */
      /* fork qui permet au fils de récuperer les tubes */
      pid = fork();
      if(pid == -1) ERROR_EXIT("fork");

      /* Code executé par le fils */
      if (pid == 0) {

        close(tabTubeOut[0]);
        close(tabTubeErr[0]);

        /* Redirection stdout */
        close(STDOUT_FILENO);
        dup(tabTubeOut[1]);
        close(tabTubeOut[1]);

        /* Redirection stderr */
        close(STDERR_FILENO);
        dup(tabTubeErr[1]);
        close(tabTubeErr[1]);

        /* Fermetures des descripteurs de fichiers */
        /*herité par le fils mais inutiles pour lui*/
        int j = 0;
        for(j = 0 ; j < i ; j++){
          close(fdsOut[j].fd);
          close(fdsErr[j].fd);
        }

        /* Creation du tableau d'arguments pour le ssh */
        char rang[32];
        sprintf(rang, "%d", i);

        char **ssh_args = malloc(32*sizeof(char*));

        ssh_args[0] = "ssh";
        ssh_args[1] = procs[i];
        ssh_args[2] = "./pr204-7612/Phase1/bin/dsmwrap";
        ssh_args[3] = port_char;
        ssh_args[4] = hostname;
        ssh_args[5] = rang;

        /* Ajout des arguments de la ligne de commande */
        for(j = 2 ; j < argc ; j++)
          ssh_args[j+4] = argv[j];

        ssh_args[argc + 5 - 1] = NULL;

        /* Connexion ssh effectuée*/
        /* jump to new prog : */
        execvp("ssh",ssh_args);

      /* Code executé par le pere */
      } else  if(pid > 0) {

        /* Fermeture des extremites des tubes non utiles */
        close(tabTubeOut[1]);
        close(tabTubeErr[1]);

        /* Remplissage desdescripteurs de fichiers pour chaque fils */
        fdsOut[i].fd = tabTubeOut[0];
        fdsOut[i].events = POLLIN;
        fdsOut[i].revents = 0;
        fdsErr[i].fd = tabTubeErr[0];
        fdsErr[i].events = POLLIN;
        fdsErr[i].revents = 0;

        num_procs_creat++;
      }
    /* Accepte les connmachine_file exemple arg arg arg3exions des processus dsm distants */

  }

  int sock2[num_procs];

  for(i = 0; i < num_procs ; i++){

    struct sockaddr result;
    socklen_t result_size;

    /*  connexion aux machine distante */
    do{
      sock2[i] = accept(sock,&result,&result_size);
    }while ((sock2[i]== -1) && ((errno == EINTR) || (errno == ECONNABORTED)|| (errno == EAGAIN)) );

    fprintf(stdout,"===============================%i / %i\n",i,num_procs-1);
    if (sock2[i] == -1){
      printf("erreur accept\n");
      return 1;
    }

    /*  Recuperation des informations de la machine distante */
    dsm_proc_t proc_dist;
    recv(sock2[i], &proc_dist, sizeof(dsm_proc_t), 0);
    /* 1- Le nom de la machine */
    strcpy(proc_array[proc_dist.connect_info.rank].connect_info.host, proc_dist.connect_info.host);
    /* 2- Le rang de la machine */
    proc_array[proc_dist.connect_info.rank].connect_info.rank = proc_dist.connect_info.rank;
    /* 3- Le port de la machine */
    proc_array[proc_dist.connect_info.rank].connect_info.port = proc_dist.connect_info.port;
    /* 4- Le numero de port de la socket d'ecoute des processus distants */
    proc_array[proc_dist.connect_info.rank].connect_info.fd_sock = 4;
    /* 5- Le pid du processus distant  */
    proc_array[proc_dist.connect_info.rank].pid = proc_dist.pid;

  }

  for(i = 0; i < num_procs ; i++){

    /* Envoi du nombre de processus aux processus dsm*/
    int h = send(sock2[i], &num_procs, sizeof(num_procs), 0);
    /* Envoi des infos de connexion aux processus */
    int d = send(sock2[i],proc_array, sizeof(proc_array), 0);

    if (h == -1) {
      perror("erreur envoi nombre de processus\n");
      return -1;
    }

    if (d == -1) {
      perror("erreur envoi infos de connexions de processus\n");
      return -1;
    }

  }

  /*
    je recupere les infos sur les tubes de redirection
    jusqu'à ce qu'ils soient inactifs (ie fermes par les
    processus dsm ecrivains de l'autre cote ...)
  */
  int a = 0;
  char *buffer = malloc(1024);
  memset(buffer,0,1024);

  while(1){
    do{
      a = poll(fdsOut,num_procs,-1);
    }while(a == -1 && errno == EINTR);
    if (a == -1) {
      perror("erreur poll\n");
      return -1;
    }

    int j = 0;

    /* gestion des E/S : on recupere les caracteres */
    /* sur les tubes de redirection de stdout/stderr */
    for (j = 0; j < num_procs+1 ; j++){
      if(fdsOut[j].revents == POLLHUP || fdsOut[j].revents == POLLERR || fdsOut[j].revents == POLLNVAL){
        close(fdsOut[j].fd);
        fdsOut[j].fd = -1;
        fdsOut[j].events = -1;
        fdsOut[j].revents = -1;
      }
      else if (fdsOut[j].revents == POLLIN){
        char *buffer = malloc(1024);
        memset(buffer,0,1024);
        read(fdsOut[j].fd,buffer,1024);
        printf("[Proc %d : %s : stdout] buffer %s\n",j,procs[j],buffer);
        fdsOut[j].revents = 0;
        break;
      }
      if(fdsErr[j].revents == POLLHUP || fdsErr[j].revents == POLLERR || fdsErr[j].revents == POLLNVAL){
        close(fdsErr[j].fd);
        fdsErr[j].fd = -1;
        fdsErr[j].events = -1;
        fdsErr[j].revents = -1;
      }
      else if (fdsErr[j].revents == POLLIN){
        char *buffer = malloc(1024);
        memset(buffer,0,1024);
        read(fdsErr[j].fd,buffer,1024);

        printf("[ proc %d : %s : stdout ] buffer %s\n",j,procs[j],buffer);
        fdsErr[j].revents = 0;
        break;
      }
    }
  }

  /* Attente des processus fils */
  wait(NULL);

  /* Fermeture des descripteurs proprement */
  for(i = 0 ; i < num_procs ; i++){
    close(close(fdsErr[i].fd));
    close(close(fdsOut[i].fd));
  }

  /* Fermeture de la socket d'ecoute */
  close(sock);
  }
  exit(EXIT_SUCCESS);
}
