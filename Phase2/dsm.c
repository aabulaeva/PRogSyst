#include "dsm.h"


int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID;  /* rang (= numero) du processus */

struct pollfd *fdsProc;
volatile int fd_array[100];

int creer_socket()
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  struct addrinfo *result = malloc(sizeof(struct addrinfo));
  struct addrinfo *rp = malloc(sizeof(struct addrinfo));
  memset(result,0,sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  char hostname[128];
  memset(hostname,0,128);
  gethostname(hostname,128);
  int rgai = getaddrinfo(hostname,"0",&hints,&result);

  if (rgai == -1 || result == NULL){
    printf("erreur adresse\n");
    return 1;
  }

  int fd = 0;

  for (rp = result; rp != NULL; rp = rp->ai_next) {
   fd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
   if (fd == -1)
       continue;
   if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0)
       break;                  /* Success */
  }

  if (rp == NULL) { /* No address succeeded */
     fprintf(stderr, "Could not bind\n");
     exit(EXIT_FAILURE);
  }

  freeaddrinfo(result);

  return fd;
}

int ecoutesocket(int sock)
{
  int file = 512;
  int ecoute = listen(sock,file);

  if (ecoute == -1) {
    perror("ecoute de la socket\n");
    return -1;
  }

  return ecoute;
}

/* Indique l'adresse de debut de la page de numero numpage */
static char *num2address( int numpage )
{
   char *pointer = (char *)(BASE_ADDR+(numpage*(PAGE_SIZE)));

   if( pointer >= (char *)TOP_ADDR ){
      fprintf(stderr,"[%i] Invalid address !\n", DSM_NODE_ID);
      return NULL;
   }
   else return pointer;
}

/* Cette fonction permet de recuperer un numero de page */
/* a partir  d'une adresse  quelconque */
static int address2num( char *addr )
{
  return (((long int)(addr - BASE_ADDR))/(PAGE_SIZE));
}

/* Fonctions pouvant etre utiles */
static void dsm_change_info( int numpage, dsm_page_state_t state, dsm_page_owner_t owner)
{
  if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {
	   if (state != NO_CHANGE )
	table_page[numpage].status = state;
     if (owner >= 0 )
	table_page[numpage].owner = owner;
      return;
   }
   else {
	    fprintf(stderr,"[%i] Invalid page number !\n", DSM_NODE_ID);
      return;
   }
}

static dsm_page_owner_t get_owner( int numpage)
{
   return table_page[numpage].owner;
}

static dsm_page_state_t get_status( int numpage)
{
   return table_page[numpage].status;
}

/* Allocation d'une nouvelle page */
static void dsm_alloc_page( int numpage )
{
   char *page_addr = num2address( numpage );
   mmap(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   return ;
}

/* Changement de la protection d'une page */
static void dsm_protect_page( int numpage , int prot)
{
   char *page_addr = num2address( numpage );
   mprotect(page_addr, PAGE_SIZE, prot);
   return;
}

static void dsm_free_page( int numpage )
{
   char *page_addr = num2address( numpage );
   munmap(page_addr, PAGE_SIZE);
   return;
}

static int dsm_send(int dest,void *buf,size_t size)
{
   /* Envoi effectif de la requete */
   int res_send = 0;

   do{
     res_send = send(dest, &buf, size, 0);
   }while(res_send == -1 && errno == EINTR);

   if (res_send == -1) {
     perror("erreur send \n");
   }
   return res_send;
}

static void * dsm_recv(int from,void *buf,size_t size)
{
   /* Reception effective de la requete */
   int res_recv = 0;

   do{
     res_recv = recv(from, &buf, size, 0);
   }while((res_recv == -1 && errno == EINTR) || res_recv == 0);
   printf("[%i] J'ai reçu de la part de la socket %i : %i bits \n",DSM_NODE_ID,from , res_recv );

   if (res_recv == -1) {
     perror("erreur recv \n");
   }
   return buf;
}

/* Thread qui attend les requêtes des autres processus qui veulent acceder */
static void *dsm_comm_daemon( void *arg )
{
  struct pollfd *fdsProc = (struct pollfd*)arg;

  printf("[%i] Waiting for incoming reqs \n", DSM_NODE_ID);

  int j = 0;
  int a = 0;

  void *buffer;

  while(1)
    {
     do{
       a = poll(fdsProc,DSM_NODE_NUM,-1);
     }while(a == -1 && errno == EINTR);
     if (a == -1) {
       perror("erreur poll thread\n");
     }

     for (j = 0; j < DSM_NODE_NUM+1 ; j++){
       if(fdsProc[j].revents == POLLHUP || fdsProc[j].revents == POLLERR || fdsProc[j].revents == POLLNVAL){

         close(fdsProc[j].fd);
         fdsProc[j].fd = -1;
         fdsProc[j].events = -1;
         fdsProc[j].revents = -1;

       }
       else if (fdsProc[j].revents == POLLIN){

         /* Reception de la requete */
         printf("[%i] J'ai une requete du processus %i sur la socket %i \n",DSM_NODE_ID,j,fdsProc[j].fd );
         buffer = dsm_recv(fdsProc[j].fd,buffer,sizeof(buffer));

         /* Recuperation du numero de la page en question */
         char* page_addr  = (char *)(((unsigned long) buffer) & ~(PAGE_SIZE-1));
         int num_page = address2num( page_addr );
         printf("[%i] Numéro page qu'il faut que je libère : %i\n",DSM_NODE_ID, num_page );

         /* Je change les informations du tableau */
         dsm_change_info( num_page, NO_CHANGE , j);
         printf("[%i] J'ai changé les informations \n",DSM_NODE_ID );

         /* Je libère cette page de mon espace d'adressage */
         dsm_free_page( num_page );
         printf("[%i] J'ai libéré cette page de mon espace d'adressage\n",DSM_NODE_ID );

         /* Je previens l'autre processus qu'il peut faire le reste */
         int sent = dsm_send(fdsProc[j].fd,buffer,sizeof(buffer));
         printf("[%i] J'ai prévenu le processus en envoyant %i bits\n",DSM_NODE_ID,sent);

         fdsProc[j].revents = 0;
         fdsProc[j].events = 0;

         break;
       }

     }

  	sleep(2);

   }

  return;
}

static void dsm_handler( void  *addr)
{

  printf("[%i] FAULTY  ACCESS !!! \n",DSM_NODE_ID);

   /* Récuperation de adresse de la page dont fait partie l'adresse qui a provoque la faute */
   char* page_addr  = (char *)(((unsigned long) addr) & ~(PAGE_SIZE-1));

   /* Récupration du numéro de page concernée */
   int num_page = address2num( page_addr );
   printf("[%i] numéro page %i\n",DSM_NODE_ID, num_page );

   /* Récupération du processus actuellement propriétaire */
   dsm_page_owner_t owner = get_owner(num_page);
   printf("[%i] je vais l'envoyer au processus %i via la socket %i \n",DSM_NODE_ID, owner, fd_array[owner] );

   /* Envoie de la requête à ce processus */
   int sent = dsm_send(fd_array[owner],addr,sizeof(addr));

   printf("[%i] envoyé : %i\n", DSM_NODE_ID, sent);

   /* Je recois la page de la part du proprietaire */
   void *buf;
   buf = dsm_recv(fd_array[owner],buf,sizeof(buf));
   printf("[%i] J'ai reçu la page \n", DSM_NODE_ID);

   /* J'alloue cette page à mon espace d'adressage */
   dsm_alloc_page( num_page );
   printf("[%i] J'ai alloué la page \n", DSM_NODE_ID);

   /* Je met à jour les informations concernant cette page */
   dsm_change_info( num_page, NO_CHANGE , DSM_NODE_ID);
   printf("[%i] J'ai changé les infos de la page \n", DSM_NODE_ID);

   /* Je previens maintenant tous les autres que le propiétaire de la page à bien changé */

   abort();
}

/* Traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context)
{

  /* Récuperation adresse qui a provoque une erreur */
  void *addr = info->si_addr;

  /* Si ceci ne fonctionne pas, utiliser a la place :*/
  /*
  #ifdef __x86_64__
  void *addr = (void *)(context->uc_mcontext.gregs[REG_CR2]);
  #elif __i386__
  void *addr = (void *)(context->uc_mcontext.cr2);
  #else
  void  addr = info->si_addr;
  #endif
  */
  /*
  pour plus tard (question ++):
  dsm_access_t access  = (((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & 2) ? WRITE_ACCESS : READ_ACCESS;
  */

  if ((addr >= (void *)BASE_ADDR) && (addr < (void *)TOP_ADDR))
   {
    dsm_handler(addr);
   }
  else
   {
    /* SIGSEGV normal : ne rien faire*/
    return ;
   }
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char **argv)
{
  struct sigaction act;
  int index;

  sleep(1);

  /* Reception du nombre de processus dsm envoye */
  /* par le lanceur de programmes (DSM_NODE_NUM)*/
  DSM_NODE_NUM = atoi(argv[1]);

  /* Reception de mon numero de processus dsm envoye */
  /* par le lanceur de programmes (DSM_NODE_ID)*/
  DSM_NODE_ID = atoi(argv[2]);

  printf("\n",atoi(argv[2]) );

  /* Reception des informations de connexion des autres */
  /* processus envoyees par le lanceur : */
  /* nom de machine, numero de port, etc. */

  int premiere_info = argc-DSM_NODE_NUM*5;
  dsm_proc_t proc_array[DSM_NODE_NUM];
  int j = 0;
  int i = 0;

  for(j = argc-DSM_NODE_NUM*5 ; j < argc ; j = j+5){
    proc_array[i].pid =atoi(argv[j]);
    proc_array[i].connect_info.rank = atoi(argv[j+1]);
    strcpy(proc_array[i].connect_info.host,(argv[j+2]));
    proc_array[i].connect_info.port = atoi(argv[j+3]);
    proc_array[i].connect_info.fd_sock = atoi(argv[j+4]);
    i++;
  }

  /* Creation des tableau et structure pour les interconnexion */
  int fd[DSM_NODE_NUM];
  int sock2[DSM_NODE_NUM];
  memset(sock2,0,sizeof(int)*DSM_NODE_NUM);

  struct addrinfo hints;
  struct addrinfo *result;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  for(j=0;j<DSM_NODE_NUM;j++){
    /*Création de la socket d'écoute */
    fd[proc_array[j].connect_info.rank] = creer_socket();
    if (fd[proc_array[j].connect_info.rank]==-1) {
      printf("erreur a la creation de la socket\n");
    }
  }

  /* initialisation des connexions */
  /* avec les autres processus : connect/accept */
  int sock_proc[DSM_NODE_NUM];
  int cpt=0;
  /* Parcours de tous les processus pour s'interconnecter */
  for(j=0;j<DSM_NODE_NUM;j++){

    int co = 0 ;
    char port_char[32];
    memset(port_char,0,32);

    /* Le processus accepte les connections de tous les processus ayant un rang superieur à lui*/
    if (DSM_NODE_ID < proc_array[j].connect_info.rank) {

      struct sockaddr res;
      socklen_t res_size;

      /* Stockage des socket accpetées dans un tableau */
      do{
       sock2[proc_array[j].connect_info.rank]= accept(proc_array[proc_array[j].connect_info.rank].connect_info.fd_sock,&res,&res_size);
      }while ((sock2[proc_array[j].connect_info.rank] == -1));

      sock_proc[cpt] = sock2[proc_array[j].connect_info.rank];
      cpt++;

      /* Vérification des accepts */
      if (sock2[proc_array[j].connect_info.rank] == -1){
       (void)fprintf(stdout, "============ ACCEPT FAILED =========== : %s : %i\n", strerror(errno), proc_array[proc_array[j].connect_info.rank].connect_info.fd_sock);
      }else{
       fprintf(stdout,"============ ACCEPT SUCCED =========== %i / %i\n",(j-DSM_NODE_ID),(DSM_NODE_NUM-DSM_NODE_ID-1));
      }

    }

    /* Le processus se connecte avec tous les processus ayant un rang inférieur à lui*/
    if(DSM_NODE_ID > proc_array[j].connect_info.rank ){

      /* Récupération du port et de hostname du processus distant */
      sprintf(port_char,"%i",proc_array[j].connect_info.port);
      getaddrinfo(proc_array[j].connect_info.host,port_char,&hints,&result);

      /* Connexion avec le processus */
      do{
        co = connect(fd[proc_array[j].connect_info.rank],result->ai_addr,result->ai_addrlen);
      }while ((co == -1) && (errno == ECONNREFUSED));

      /* Stockage des socket dans un tableau */
      sock_proc[cpt]=fd[proc_array[j].connect_info.rank];
      cpt++;

      /* Vérification des connect */
      if (co==-1) {
        fprintf(stdout,"============ CONNEXION FAILED =========== : %s.\n", strerror(errno));
      }else{
        fprintf(stdout,"============ CONNEXION SUCCED =========== %i / %i\n",j,DSM_NODE_ID-1);
      }

    }

  }

  sleep(2);

  /* Rangement du tableau de socket par ordre de rang */
  int monid = DSM_NODE_ID;
  for ( i = 0; i < DSM_NODE_NUM; i++) {
    send(sock_proc[i], &monid, sizeof(monid), 0);
  }

  int id_autre_proc;
  for ( i = 0; i < DSM_NODE_NUM-1; i++) {
    recv(sock_proc[i], &id_autre_proc, sizeof(id_autre_proc), 0);
    if (id_autre_proc == DSM_NODE_ID) {
      proc_array[id_autre_proc].connect_info.fd_sock=-100;
    }
    else{
      printf("je communique avec id_autre_proc %i via la socket %i\n",id_autre_proc,sock_proc[i] );
      proc_array[id_autre_proc].connect_info.fd_sock=sock_proc[i];
    }
  }

  /* Creation et initialisation du fdsProc pour le poll du thread */
  struct pollfd *fdsProc = malloc(DSM_NODE_NUM*sizeof(struct pollfd));
  memset(fdsProc,0,sizeof(struct pollfd));

  for (j = 0; j < DSM_NODE_NUM ; j++){
   fdsProc[j].fd = proc_array[j].connect_info.fd_sock;
   fdsProc[j].events = POLLIN;
   fdsProc[j].revents = 0;
   fd_array[j] = fdsProc[j].fd;
  }

  /* Allocation des pages en tourniquet */
  for(index = 0; index < PAGE_NUMBER; index ++){

    if ((index % DSM_NODE_NUM) == DSM_NODE_ID)
      dsm_alloc_page(index);

    dsm_change_info(index, WRITE, index % DSM_NODE_NUM);
  }

  /* Mise en place du traitant de SIGSEGV */
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = segv_handler;
  sigaction(SIGSEGV, &act, NULL);

  /* Creation du thread de communication */
  /* ce thread va attendre et traiter les requetes */
  /* des autres processus */
  pthread_create(&comm_daemon, NULL, dsm_comm_daemon, (void *)fdsProc);

  /* Adresse de début de la zone de mémoire partagée */
  return ((char *)BASE_ADDR);

}

void dsm_finalize( void )
{
   /* fermer proprement les connexions avec les autres processus */

   /* terminer correctement le thread de communication */
   /* pour le moment, on peut faire : */

   pthread_cancel(comm_daemon);

  return;
}
