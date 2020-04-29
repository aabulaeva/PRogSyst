#include "common_impl.h"


/* Fonction de creation et d'attachement */
/* d'une nouvelle socket */
/* renvoie le numero de descripteur */
/* et modifie le parametre port_num */
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

/* Vous pouvez ecrire ici toutes les fonctions */
/* qui pourraient etre utilisees par le lanceur */
/* et le processus intermediaire. N'oubliez pas */
/* de declarer le prototype de ces nouvelles */
/* fonctions dans common_impl.h */
char** lecturemachine_file(char *name){

  int fp = open(name,O_RDONLY);
  int nb = nombreMachines(name);
  int j = 0;
  int i = 0;

  char *buffer = malloc(sizeof(char)*LEN_NAME*100);
  char *buf = malloc(sizeof(char)*2);

  char **tab = malloc(nb*sizeof(char*));

  for(i = 0 ; i < nb; i++){
    tab[i] = malloc(LEN_NAME);
  }

  char *res = malloc(sizeof(char)*LEN_NAME);

  int to_send = read(fp,buffer,LEN_NAME*100);

  memset(res,0,LEN_NAME);
  memset(buf,0,2);

  for (i = 0; i < to_send; i++) {
    if (buffer[i] == '\n' && strlen(res) >0) {
      strcpy(tab[j],res);
      memset(res,0,LEN_NAME);
      memset(buf,0,2);
      j++;
      }
    else if (buffer[i] != ' ') {
      buf[0]=buffer[i];
      strcat(res,buf);
    }
    else
      break;
    }
  return tab;
}

/* Fonciton qui permet de retourner le nombre de processus à créer*/
int nombreMachines(char *name){

  int fp = open(name,O_RDONLY);
  char *buffer = malloc(sizeof(char)*LEN_NAME*100);

  int nbMachine = 0;
  int i = 0;
  int to_send = read(fp,buffer,LEN_NAME*100);

  for (i = 0; i < to_send; i++) {
    if (buffer[i] == '\n') {
      nbMachine++;
      }
  }
  return nbMachine;
}

int ecoutesocket(int sock){

  int file = 512;
  int ecoute = listen(sock,file);

  if (ecoute == -1) {
    perror("ecoute de la socket\n");
    return -1;
  }

  return ecoute;
}
