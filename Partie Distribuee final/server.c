/* T. Grandpierre - Application distribue'e pour TP IF4-DIST 2004-2005

But : 

fournir un squelette d'application capable de recevoir des messages en 
mode non bloquant provenant de sites connus. L'objectif est de fournir
une base pour implementer les horloges logique/vectorielle/scalaire, ou
bien pour implementer l'algorithme d'exclusion mutuelle distribue'

Syntaxe :
         arg 1 : Numero du 1er port
	 arg 2 et suivant : nom de chaque machine

--------------------------------
Exemple pour 3 site :

Dans 3 shells lances sur 3 machines executer la meme application:

pc5201a>./dist 5000 pc5201a.esiee.fr pc5201b.esiee.fr pc5201c.esiee.fr
pc5201b>./dist 5000 pc5201a.esiee.fr pc5201b.esiee.fr pc5201c.esiee.fr
pc5201c>./dist 5000 pc5201a.esiee.fr pc5201b.esiee.fr pc5201c.esiee.fr

pc5201a commence par attendre que les autres applications (sur autres
sites) soient lance's

Chaque autre site (pc5201b, pc5201c) attend que le 1er site de la
liste (pc5201a) envoi un message indiquant que tous les sites sont lance's


Chaque Site passe ensuite en attente de connexion non bloquante (connect)
sur son port d'ecoute (respectivement 5000, 5001, 5002).
On fournit ensuite un exemple permettant 
1) d'accepter la connexion 
2) lire le message envoye' sur cette socket
3) il est alors possible de renvoyer un message a l'envoyeur ou autre si
necessaire 

*/


#include <stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<netdb.h>
#include<string.h>
#include <time.h>


#define bool unsigned char 
#define VRAI 1
#define FAUX 0

#define MAX(x, y) (((x) > (y)) ? (x) : (y))


#define MAX_PILE 10 

typedef struct Message* value ;

struct Message{
  char* type ;
  char* message ;
  time_t timestamp ; 
  int port ; 
  int nb_reponse_valide ;
  bool has_been_sent ;
};
void initMessage(struct Message* message, char* type, char* str, time_t time, int port, int NSites){
  message->type = type ;
  message->message = str ;
  message->timestamp = time ;
  message->port = port ;
  message->nb_reponse_valide = 1 ;
  message->has_been_sent = FAUX ;
}

void PrintMessage(struct Message* msg){
  if(msg == NULL) return ;
  printf("{[type=%s] msg=%s time=%ld port=%d nb_confirmation_sites=%d}\n",msg->type, msg->message, msg->timestamp, msg->port, msg->nb_reponse_valide);
}

void MessageToString(struct Message* message, char* buffer){
  strcpy(buffer, message->type) ; // 0
  strcat(buffer, ":") ;
  strcat(buffer, message->message) ; // 1
  strcat(buffer, ":") ;
  char str[64] ;
  sprintf(str, "%ld", message->timestamp) ; // 2
  strcat(buffer, str) ;
  strcat(buffer, ":") ;
  sprintf(str, "%d", message->port); // 3
  strcat(buffer, str) ;
}

void MessageInitString(struct Message* message, char* buffer){

  char *ptr = strtok(buffer, ":");
  int i = 0 ; 

	while(ptr != NULL) {
    if(i == 0) message->type = ptr ;
    if(i == 1) message->message = ptr ;
    if(i == 2) message->timestamp = atol(ptr);
    if(i == 3) message->port = atoi(ptr) ;
		ptr = strtok(NULL, ":");
    i++ ;
	}

}

struct Node {
    struct Message* data;
    struct Node* next;
};

struct Queue {
    struct Node* first;
};

struct Queue* createQueue() {
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
    q->first = NULL;
    return q;
}

void PrintQueue(struct Queue* q){
  struct Node* first = q->first ;
  printf("PrintQueue : \n") ;
  if(first == NULL) return ;

  printf("- ") ;
  PrintMessage(first->data) ;
  struct Node* next = first->next ;
  while(next != NULL) {
    printf("- ") ;
    PrintMessage(next->data) ;
    next = next->next ;
  }
}

void enqueue(struct Queue* q, struct Message* data) {
    struct Node* temp = (struct Node*)malloc(sizeof(struct Node));
    temp->data = data;
    temp->next = NULL;

    struct Node* first = q->first ;

    if(first == NULL){
      q->first = temp ;
      return ;
    }

    struct Node* next = first->next ;

    while(next != NULL){
      if(next->data->timestamp > data->timestamp || 
        (next->data->timestamp == data->timestamp && next->data->port > data->port) ){
        first->next = temp ;
        temp->next = next ;
        return ;
      }
      first = next ;
      next = next->next ;
    }
    first->next = temp ;
}

struct Message* dequeue(struct Queue* q, struct Message* val) {
    if (q->first == NULL)
        return NULL;
    
    struct Node* temp = q->first;
    struct Message* data = temp->data;
    q->first = q->first->next ;
    free(temp);
    val->message = data->message;
    val->nb_reponse_valide = data->nb_reponse_valide;
    val->port = data->port ;
    val->timestamp = data->timestamp ;
    val->has_been_sent = data->has_been_sent ;
    val->type = data->type ;
    return val ;
}

bool queueIsEmpty(struct Queue* q){
  if(q->first == NULL) return VRAI ;
  return FAUX ;
}

value queueFront(struct Queue* q, value val) {
    if (q->first == NULL)
        return NULL;
    //printf("DEBUG QueueFront\n") ;
    value d = q->first->data ;
    val->message = d->message;
    val->nb_reponse_valide = d->nb_reponse_valide;
    val->port = d->port ;
    val->timestamp = d->timestamp ;
    val->has_been_sent = d->has_been_sent ;
    val->type = d->type ;
    return d ;
}

/*****************************************/

bool prefix(const char *pre, const char *str){
  return strncmp(pre, str, strlen(pre)) == 0;
}

int GetSitePos(int Nbsites, char *argv[]) ;
void WaitSync(int socket);
void SendSync(char *site, int Port);

/*Identification de ma position dans la liste */
int GetSitePos(int NbSites, char *argv[]) {
  char MySiteName[20]; 
  int MySitePos=-1;
  int i;
  gethostname(MySiteName, 20);
  for (i=0;i<NbSites;i++) 
    if (strcmp(MySiteName,argv[i+2])==0) {
      MySitePos=i;
      //printf("L'indice de %s est %d\n",MySiteName,MySitePos);
      return MySitePos;
    }
  if (MySitePos == -1) {
    printf("Indice du Site courant non trouve' dans la liste\n");
    exit(-1);
  }
  return (-1);
}


/*Attente bloquante d'un msg de synchro sur la socket donne'e*/
void WaitSync(int s_ecoute) {
  char texte[40];
  int l;
  int s_service;
  struct sockaddr_in sock_add_dist;
  int size_sock;
  size_sock=sizeof(struct sockaddr_in);
  printf("WaitSync : ");fflush(0);
  s_service=accept(s_ecoute,(struct sockaddr*) &sock_add_dist,&size_sock);
  l=read(s_service,texte,39);
  texte[l] ='\0';
  printf("%s\n",texte); fflush(0);
  close (s_service);
} 

/*Envoie d'un msg de synchro a la machine Site/Port*/
void SendSync(char *Site, int Port) {
  struct hostent* hp;
  int s_emis;
  char chaine[15];
  int longtxt;
  struct sockaddr_in sock_add_emis;
  int size_sock;
  int l;
  
  if ( (s_emis=socket(AF_INET, SOCK_STREAM,0))==-1) {
    perror("SendSync : Creation socket");
    exit(-1);
  }
    
  hp = gethostbyname(Site);
  if (hp == NULL) {
    perror("SendSync: Gethostbyname");
    exit(-1);
  }

  size_sock=sizeof(struct sockaddr_in);
  sock_add_emis.sin_family = AF_INET;
  sock_add_emis.sin_port = htons(Port);
  memcpy(&sock_add_emis.sin_addr.s_addr, hp->h_addr, hp->h_length);
  
  if (connect(s_emis, (struct sockaddr*) &sock_add_emis,size_sock)==-1) {
    perror("SendSync : Connect");
    exit(-1);
  }
     
  sprintf(chaine,"**SYNCHRO**");
  longtxt =strlen(chaine);
  /*Emission d'un message de synchro*/
  l=write(s_emis,chaine,longtxt);
  close (s_emis); 
}

int SendSocket(char* Site, int Port, char* text){
  struct hostent* hp;
  int s_emis;
  char chaine[15];
  int longtxt;
  struct sockaddr_in sock_add_emis;
  int size_sock;
  int l;
  
  if ((s_emis=socket(AF_INET, SOCK_STREAM,0))==-1) {
    perror("SendSocket : Creation socket");
    exit(-1);
  }
    
  hp = gethostbyname(Site);
  if (hp == NULL) {
    perror("SendSocket: Gethostbyname");
    exit(-1);
  }

  size_sock=sizeof(struct sockaddr_in);
  sock_add_emis.sin_family = AF_INET;
  sock_add_emis.sin_port = htons(Port);
  memcpy(&sock_add_emis.sin_addr.s_addr, hp->h_addr, hp->h_length);
  
  if (connect(s_emis, (struct sockaddr*) &sock_add_emis,size_sock)==-1) {
    perror("SendSocket : Connect");
    exit(-1);
  }
     
  printf("Written : %s\n", text) ;
  longtxt =strlen(text);
  /*Emission d'un message de synchro*/
  l=write(s_emis,text,longtxt);
  return s_emis ;
}

#define TEST
#define SLEEP_TIME 1
//#define TEST_QUEUE

/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/

int main (int argc, char* argv[]) {

  bool actived = FAUX ;

  

  #ifdef TEST_QUEUE
    struct Queue* q = createQueue() ;
    PrintQueue(q) ;
    struct Message val ;

    struct Message msg1 ;
    initMessage(&msg1, "requete", "msg1", 1231, 4444, 3) ;
    enqueue(q, &msg1) ;
    PrintQueue(q) ;

    struct Message msg2 ;
    initMessage(&msg2, "requete", "msg2", 1233, 4444, 3) ;
    enqueue(q, &msg2) ;
    PrintQueue(q) ;

    struct Message msg3 ;
    initMessage(&msg3, "requete", "msg3", 1234, 4444, 3) ;
    enqueue(q, &msg3) ;
    PrintQueue(q) ;

    struct Message msg4 ;
    initMessage(&msg4, "requete", "msg4", 1232, 4444, 3) ;
    enqueue(q, &msg4) ;
    PrintQueue(q) ;

    queueFront(q, &val) ;
    printf("> Front : ") ;
    PrintMessage(&val) ;
    PrintQueue(q) ;

    struct Message msg5 ;
    initMessage(&msg5, "requete", "msg5", 1233, 4443, 3) ;
    enqueue(q, &msg5) ;
    PrintQueue(q) ;

    dequeue(q, &val) ;
    printf("> Dequeue : ") ;
    PrintMessage(&val) ;
    PrintQueue(q) ;
    
    dequeue(q, &val) ;
    printf("> Dequeue : ") ;
    PrintMessage(&val) ;
    PrintQueue(q) ;

    queueFront(q, &val) ;
    printf("> Front : ") ;
    PrintMessage(&val) ;
    PrintQueue(q) ;

    return 0 ;
  #endif



  struct sockaddr_in sock_add, sock_add_dist;
  int size_sock;
  int s_ecoute, s_service;
  char texte[40];
  int i,l;
  float t;

  int PortBase=-1; /*Numero du port de la socket a` creer*/
  int NSites=-1; /*Nb total de sites*/


  if (argc!=4) {
    printf("Il faut donner le numero de port du site et le nombre total de sites");
    exit(-1);
  }

  srand(time(NULL));

  /*----Nombre de sites (adresses de machines)---- */
  //NSites=argc-2;
  NSites=atoi(argv[3]); //-2;



  /*CREATION&BINDING DE LA SOCKET DE CE SITE*/
  PortBase=atoi(argv[2]) ;//+GetSitePos(NSites, argv);
  printf("Numero de port de ce site %d\n",PortBase);

  sock_add.sin_family = AF_INET;
  sock_add.sin_addr.s_addr= htons(INADDR_ANY);  
  sock_add.sin_port = htons(PortBase);

  if ( (s_ecoute=socket(AF_INET, SOCK_STREAM,0))==-1) {
    perror("Creation socket");
    exit(-1);
  }

  if ( bind(s_ecoute,(struct sockaddr*) &sock_add, \
	    sizeof(struct sockaddr_in))==-1) {
    perror("Bind socket");
    exit(-1);
  }
  
  listen(s_ecoute,30);
  /*----La socket est maintenant cre'e'e, binde'e et listen----*/

  if (strcmp(argv[1], argv[2])==0) { 
    /*Le site 0 attend une connexion de chaque site : */
    for(i=0;i<NSites-1;i++) 
      WaitSync(s_ecoute);
    printf("Site 0 : toutes les synchros des autres sites recues \n");fflush(0);
    /*et envoie un msg a chaque autre site pour les synchroniser */
    for(i=0;i<NSites-1;i++) 
      SendSync("localhost", atoi(argv[1])+i+1);
    } else {
      /* Chaque autre site envoie un message au site0 
	 (1er  dans la liste) pour dire qu'il est lance'*/
      SendSync("localhost", atoi(argv[1]));
      /*et attend un message du Site 0 envoye' quand tous seront lance's*/
      printf("Wait Synchro du Site 0\n");fflush(0);
      WaitSync(s_ecoute);
      printf("Synchro recue de  Site 0\n");fflush(0);
  }

  
  /* Passage en mode non bloquant du accept pour tous*/
  /*---------------------------------------*/
  fcntl(s_ecoute,F_SETFL,O_NONBLOCK);
  size_sock=sizeof(struct sockaddr_in);
  
  int Port_Site = atoi(argv[1]) ;
  struct Queue* queue = createQueue() ;
  
  printf("===============================\n\n") ;
  printf("NSites = %d\n", NSites) ;
  printf("Mon port (PortBase) = %d\n", PortBase) ;
  printf("Port defaut (Port_Site) = %d\n\n", Port_Site) ;
  printf("Mon PID = %d\n", getpid()) ;




  printf("===============================\n\n") ;

  /* Boucle infini*/
  while(1) {
  
    /* On commence par tester l'arrivée d'un message */
    s_service=accept(s_ecoute,(struct sockaddr*) &sock_add_dist,&size_sock);

    int random = rand() % 10 ; 

    if(queueIsEmpty(queue) == FAUX){
        
        struct Message message ; 
        struct Message* msg = queueFront(queue, &message) ;

        
        if(msg->port == PortBase){

          if(msg->nb_reponse_valide != NSites){
            printf("[Attente] En attente des validations des autres ports. (%d/%d)\n", message.nb_reponse_valide, NSites) ;
          }
          else{
            dequeue(queue, msg) ;
            printf("[MA SECTION CRITIQUE] /!\\ Exécution de ma SC.\n") ;

            int t = 15 ;
            for(int k=0 ; k<10000000 ; k++){
              t = k*3 ;
              t = k/3 ;
            }

            for(int k=0 ; k < NSites ; k++){
              // Envoie "requete" à tous les autres sites
              
              int socket_fd = -1 ;
              int Port_A_Envoyer = Port_Site + k ;

              if(PortBase == Port_A_Envoyer) continue ;
              else {
                struct Message msg_reponse ;
                initMessage(&msg_reponse, "liberation", "-", time(NULL), PortBase, NSites) ;
                char buffer_reponse[500] ;
                MessageToString(&msg_reponse, buffer_reponse) ;
                SendSocket("localhost", Port_A_Envoyer, buffer_reponse) ;
                printf("[FIN SC] Libération du jeton SC pour le port %d.\n", Port_A_Envoyer) ;
                printf("> %s\n", buffer_reponse) ;
              }
            }

          }

        }

        else{
          if(msg->has_been_sent == FAUX){
            struct Message msg_reponse ;
            initMessage(&msg_reponse, "reponse", "-", time(NULL), PortBase, NSites) ;
            char buffer_reponse[500] ;
            MessageToString(&msg_reponse, buffer_reponse) ;
            SendSocket("localhost", msg->port, buffer_reponse) ;
            msg->has_been_sent = VRAI ;
          }
          printf("[SC] En cours d'execution de la SC du port %d (%s)\n", msg->port, msg->message) ;
        }

    }
    else printf(".\n") ;

    #ifdef TEST
      if(PortBase == 7000 && actived == FAUX) {
        actived = VRAI ;
    #else
      if(random == 0){
    #endif

      int Port_Site = atoi(argv[1]) ;
      printf("[Requete] Envoie des requetes pour ma SC.\n") ;

      struct Message msg ;
      char buffer[500] ;

      char str_requete[500] ;
      char integer_string[32];
      int integer = (int) getpid() ;
      sprintf(integer_string, "%d", integer);
      strcpy(str_requete, "[PID = ") ;
      strcat(str_requete, integer_string);
      strcat(str_requete, "] Section critique");

      initMessage(&msg, "requete", str_requete, time(NULL), PortBase, NSites) ;
      enqueue(queue, &msg) ;
      MessageToString(&msg, buffer) ;

      for(int k=0 ; k < NSites ; k++){

        int socket_fd = -1 ;
        int Port_A_Envoyer = Port_Site + k ;

        if(PortBase == Port_A_Envoyer) continue ;
        else socket_fd = SendSocket("localhost", Port_A_Envoyer, buffer) ;
        
      }
    
    }

    printf("1.0\n") ;
    
    if (s_service>0) {
      /*Extraction et affichage du message */
      printf("2.0\n") ;
      l=read(s_service,texte,500);
      texte[l] ='\0';

      printf("3.0\n") ;

      char text_copy[500];
      strcpy(text_copy, texte) ;

      struct Message message ;
      MessageInitString(&message, text_copy) ;

      if(prefix("requete", message.type)){
        enqueue(queue, &message) ;
        printf("[Requete] SC demandée par %d\n", message.port) ;
      }

      else if(prefix("reponse", message.type)){
        struct Message message_buff ; 
        struct Message* msg = queueFront(queue, &message_buff) ;
        msg->nb_reponse_valide++ ;
        printf("[Reponse] Le port %d a répondu (%d/%d)\n", message.port, msg->nb_reponse_valide, NSites) ;
      }

      else if(prefix("liberation", message.type)){
        struct Message msg ; 
        dequeue(queue, &msg) ;
        printf("[Libération] Fin de la section critique du port %d.\n", message.port) ;
      }

      fflush(0);
      close (s_service);
    }

    sleep(SLEEP_TIME) ;
    //printf(" ");
    close (s_service);
    fflush(0); /* pour montrer que le serveur est actif*/
  }


  close (s_ecoute);  
  return 0;
}
