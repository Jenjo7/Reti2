#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>

/* ******************************************************************
   This code should be used for unidirectional data transfer protocols
   (from A to B)
   Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for Pipelined ARQ), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
  int seqnum;
  int acknum;
  int checksum;
  char payload[20];
};


/*- Your Definitions
  ---------------------------------------------------------------------------*/

#define MSG_SIZE 20
#define NEW_LIST NULL

struct pkt last_pkt;
struct pkt last_ack;
struct pkt last_ack_recived;

int seqnum;
int ack;				// valore 0 o 1 in base all'alternanza
int data_sent;			// contatore dei pacchetti inviati
int ack_sent;
int pkt_corrupted;		// sia ack che dati
int pkt_retrasmitted;	// numero di pacchetti ritrasmessi
int buff_lock;			// riempimento del buffer
int last_seq_ordered;
int last_of_window;
int pkt_recived;
int tot_pkt_recived;

double RTT;				//tempo per trasmissione+ricezione di ogni messaggio+ack

/* Please use the following values in your program */

#define   A    0
#define   B    1
#define   FIRST_SEQNO   0

/*- Declarations ------------------------------------------------------------*/
void	restart_rxmt_timer(void);
void	tolayer3(int AorB, struct pkt packet);
void	tolayer5(char datasent[20]);

void	starttimer(int AorB, double increment);
void	stoptimer(int AorB);

/* WINDOW_SIZE, RXMT_TIMEOUT and TRACE are inputs to the program;
   We have set an appropriate value for LIMIT_SEQNO.298
   You do not have to concern but you have to use these variables in your
   routines --------------------------------------------------------------*/

extern int WINDOW_SIZE;      // size of the window
extern int LIMIT_SEQNO;      // when sequence number reaches this value,// it wraps around
extern double RXMT_TIMEOUT;  // retransmission timeout
extern int TRACE;            // trace level, for your debug purpose
extern double time_now;      // simulation time, for your debug purpose

/********* YOU MAY ADD SOME ROUTINES HERE ********/

struct node {
	struct pkt p;
	struct node *next;
	int attend_akc;
};

typedef struct node *list;
struct node *tail;
list L;

struct pkt buff[50]; // buffer del receiver
int attualmente_in_finestra;

int bufferSize(){
	return sizeof(buff)/sizeof(struct pkt);
}


int is_empty(list *L) {
  return (L == NULL) || (*L == NULL);
}

void list_print(list L) { 
  if (L == NULL){
     printf("lista vuota, nessun elemento da stampare");
  }
  while(L!=NULL) {
    printf("payload: %s attend_akc = %d ",L->p.payload, L->attend_akc);
    L = L->next;
  }
  printf("\n");
}

//allocazione dinamica di un nodo
struct node* node_alloc(struct pkt e) {
	struct node *tmp = (struct node *)malloc(sizeof(struct node));
	if(tmp != NULL) {
		tmp->p = e;
		tmp->next = NULL;
		tmp->attend_akc = 0;
	}
	else{
		printf("errore nella creazione del nodo\n");
	}
	return tmp;
}

int node_insert(struct node *L, struct pkt e) {
	if(L != NULL) {
		struct node *tmp = node_alloc(e);
		if(tmp != NULL) {
			tmp->next = L->next;
			L->next = tmp;
		}
		return tmp == NULL;
	} else {
		printf("lista non esiste, creazione elemento in testa\n");
	}
}

//eliminazione nodo alla posizione iesima
static int node_delete(struct node *L) {
	if(L == NULL || L->next == NULL) {
		return 1;
	} else {
		struct node *tmp = L->next;
		L->next = tmp->next;
		free(tmp);
		return 0;
	}
}

//eliminazione testa
int head_delete(list *L) {
	if(is_empty(L)) {
		return 1;
	} else {
		struct node *tmp = *L;
		if(tmp->next==NULL) {
			tail=NULL;
		}
		*L = (*L)->next;
		free(tmp);
		return 0;
	}
}

//inserimento in testa
int head_insert(list *L, struct pkt e) {
	if(L == NULL) {
		return 1;
	} else {
		struct node *tmp = node_alloc(e);

		if(tmp != NULL) {
			tmp->next = *L;
			*L = tmp;
		}
		return tmp == NULL;
	}
}

//inserisci in tail
int tail_insert(list *L, struct pkt e) {
	int result; 
	if(L == NULL) {
		printf("lista non esiste\n");
		return 1;
	} else if(is_empty(L)) {
		result = head_insert(L, e);
		tail = *L;
		return result;
	} else	{
		struct node *tmp = *L;

		while(tmp->next != NULL)
			tmp = tmp->next;
		result = node_insert(tmp, e);
		tail = tmp->next;
		return result;
	}
}

struct pkt nullPacket(struct pkt null_pkt){
	null_pkt.seqnum = 0;
	null_pkt.acknum = 0;
	null_pkt.checksum = 0;
	null_pkt.payload[0] = '\0';
	return null_pkt;
}
int is_pkt_null(struct pkt packet) {
	return (packet.acknum == 0 && packet.seqnum == 0 && packet.checksum == 0 && packet.payload == "") ? 0 : 1;
}


//seqDaCercare indica il primo pacchetto fuori ordine
void next_pkt(int seqDaCercare){
	struct pkt pacchettoInOrdine;
	struct msg message;

	for(int i = 0; i < buff_lock; i++) {//scorre tutti gli elementi della lista
		if(is_pkt_null(buff[i])) {//controlla che non sia vuoto
			if(buff[i].seqnum == seqDaCercare) {//controlla che sia il pacchetto da cercare
				pacchettoInOrdine = buff[i];
				buff[i] = buff[buff_lock - 1];
				buff[buff_lock - 1] = nullPacket(buff[buff_lock - 1]);
				buff_lock--; 
				printf("pacchetto trovato %s\n",pacchettoInOrdine.payload );
        		strcpy(message.data,pacchettoInOrdine.payload);
        		tolayer5(message.data);
        		last_seq_ordered++;
        		next_pkt(last_seq_ordered + 1);
			}
		}
	 	printf("non ho trovato altri pacchetti\n" );
	}
}


int generate_checksum(struct pkt packet){
    int i;
    int sum = (packet.seqnum + packet.acknum);
    for(i = 0; i < MSG_SIZE; i++){
        sum += (int)packet.payload[i];
    }
    return sum;
}

int check_checksum(struct pkt packet){
    int sum = generate_checksum(packet);
    printf("checkchecksum:  %d = %d\n", sum, packet.checksum);
    return sum == packet.checksum;
}
/********* STUDENTS WRITE THE NEXT SIX ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void A_output (message)
  struct msg message;
{
	//per prima cosa inseriamo in lista. se possibile inviamo il pkt(in attesa di ack) altrimenti attendiamo l'ingresso in finestra
	struct pkt new_list;
	strcpy(new_list.payload,message.data);
	new_list.seqnum = seqnum;
	new_list.acknum = ack;
	new_list.checksum  = generate_checksum(new_list);
	//aggiungi messaggio alla lista di attesa
	tail_insert(&L, new_list);
	printf("nuovo messaggio da inviare: %s seq: %d elementi in lista: ", new_list.payload, new_list.seqnum);
	list_print(L);
 	if(attualmente_in_finestra < WINDOW_SIZE && bufferSize() - buff_lock >= WINDOW_SIZE) {	//si invia subito il pacchetto
		//si controlla la possibilità del ricevitore di inserire pacchetti nel buffer
		printf("A sta inviando un messaggio: %s\n", new_list.payload);
		tail->attend_akc = 1;
        printf("messaggio inviato: %s in attesa di ack = %d\n", new_list.payload, tail->attend_akc);
        printf("attualmente in finestra %d", attualmente_in_finestra);
		data_sent++;
		attualmente_in_finestra++;
        tolayer3(A, new_list);
	}
	ack++;//preparo per il prossimo messaggio
	seqnum++;
	if(attualmente_in_finestra == WINDOW_SIZE){
		starttimer(A, RTT);
		printf("ho inviato tutti i messaggi in finestra, parte il timer \n");
    }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  //printf("A ha ricevuto un ack da B ack ricevuto = %d ack corrente %d, messaggio ricevuto : %s \n",packet.acknum,ack,packet.payload);
	if(check_checksum(packet)){ //pacchettoOK
		if(packet.seqnum > packet.acknum) { //probabile 2ack
			if(last_ack_recived.seqnum == packet.seqnum && last_ack_recived.acknum == packet.acknum){
	           //doppio ack quindi reinvio il pacchetto richiesto
				printf("A riceve un 2ack per il messaggio seqnum = %d \n", packet.seqnum);
				struct node *resended_pkt=L;
				while(L!=NULL) { //scorro la lista per cercare il pacchetto
					if(resended_pkt->p.seqnum==packet.seqnum){
						tolayer3(A,resended_pkt->p);
						pkt_retrasmitted++;
                        //attualmente_in_finestra++; // perchè NON teneva conto del reinvio del messaggio, il che equivale ad un messaggio in  più in finestra
						printf("pacchetto da reinviare %s seq %d, numero pkt ritrasmessi: %d, attualmente in finestra: %d\n",resended_pkt->p.payload,packet.seqnum,pkt_retrasmitted,attualmente_in_finestra);
                        break;
					} else {
                    	if(resended_pkt->next != NULL){
                          resended_pkt = resended_pkt->next;
                      	} else {
                          printf("Pacchetto non trovato\n" );
                          break;
                      	}
					}
				}
	        } else {
            //potrebbe essere il primo di un doppio ack quindi aggiorno i dati
		    	printf("ack 1 di 2 ricevuto s: %d a: %d \n",packet.seqnum,packet.acknum);
		    	last_ack_recived = packet;
          	}
      	} else {
	    //nel caso dell'ack cumulativo elimina tutti i paccketti in attesa di ack dalla lista
    		stoptimer(A);
    		ack = packet.seqnum;
    		seqnum = packet.acknum-1;
    		printf("ack cumulativo ricevuto %d, ack corrente = %d\n",packet.acknum,ack);
		    attualmente_in_finestra = 0;//azzera il contatore dei messaggi in finestra
        //cancella i messaggi inviato della lista
		    if(L!=NULL) {//in lista d'attesa ci sono messaggi
            	struct node *tmp = L;
                while(L!=NULL) {
                    printf("controllo il pacchetto %s ", L->p.payload);
                    if(tmp->attend_akc == 1) {
                        printf("elimino il messaggio %s dalla lista ",L->p.payload);
                        head_delete(&L);
                    } else {//il messaggio non è stato ancora inviato && attualmente_in_finestra<WINDOW_SIZE
                            tolayer3(A,tmp->p);
                            tmp->attend_akc=1;
                            attualmente_in_finestra++;
                            data_sent++;
                            printf("invio il messaggio dalla lista %s, in finestra %d",tmp->p.payload,attualmente_in_finestra);
                            if(attualmente_in_finestra == WINDOW_SIZE)break;//controllo di non superari il limite di messaggi
                    }
                    printf("\n");
		            if(tmp->next != NULL) 
						tmp = tmp->next;
                    else break;
				}
   	    	}
	    	last_pkt = packet;
    	}
	} else {//ack corrotto
    	pkt_corrupted++;
      	printf("E' arrivato un ack corrotto, pacchetti corrotti: %d\n", pkt_corrupted);
  }
}

/* called when A's timer goes off */
void
A_timerinterrupt (void)
{
	//RTT scaduto quindi rimandiamo last_pkt
	printf("TIMEOUT: ack non ricevuto, ritrasmissione finestra\n");
	attualmente_in_finestra = 0;
	struct node *tmp = L;
	while(tmp->attend_akc == 1 && attualmente_in_finestra < WINDOW_SIZE) {
		tolayer3(A, tmp->p);
		pkt_retrasmitted++;
    	attualmente_in_finestra++;
		printf("ritrasmetto p: %s, npritrasmessi: %d\n",tmp->p.payload,pkt_retrasmitted);
		tmp = tmp->next;
  	}
	printf("ho ritrasmesso tutta la finestra, faccio ripartire il timer \n");
 	starttimer(A,RTT);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void
A_init (void)
{
	int numPacchetti = 100;
	seqnum = FIRST_SEQNO;
	ack = 0;
	attualmente_in_finestra=0; //non potrà superare il valore 8
	L = NEW_LIST;//lista di pacchetti da inviare
	tail = NULL;
	data_sent = 0;
	ack_sent = 0;
	pkt_corrupted = 0;
	pkt_retrasmitted = 0;
	last_of_window = 0;
	RTT = RXMT_TIMEOUT;
}

/* called from layer 3, when a packet arrives for layer 4 at B*/
void
B_input (packet)
  struct pkt packet;
{
  	printf("B ha ricevuto un messaggio da A %s seq %d , pacchetti ricevuti %d\n",packet.payload,packet.seqnum, pkt_recived);
	if(check_checksum(packet)){

		if(packet.seqnum != last_seq_ordered+1 ) {//pacchetto NON in ordine e non gia arrivato
    		if(packet.seqnum > last_seq_ordered) {
      			pkt_recived++;
				printf("pacchetto arrivato fuori ordine %d > %d\n",packet.seqnum,last_seq_ordered);
				buff[buff_lock].seqnum=packet.seqnum;
				buff[buff_lock].acknum=packet.acknum;
				strcpy(buff[buff_lock].payload,packet.payload);
				buff_lock++;
				printf("inserito packet nel buffer payload :%s,buffer occupato: %d\n",buff[buff_lock].payload,buff_lock);
       		}
		} else {
			if(packet.seqnum > last_seq_ordered) {
				pkt_recived++;
				printf("pacchetto arrivato in ordine %d > %d\n",packet.seqnum, last_seq_ordered);
				//salvo l'ultimo seqnum arrivato in ordine
				struct msg messaggio;
      			strcpy(messaggio.data,packet.payload);
	     		tolayer5(messaggio.data);
				last_seq_ordered++;
       		}
		}


		if(buff_lock!=0) {  //ricerco nel buffer
			printf("controllo nel buffer se ci sono dei pacchetti fuori ordine\n" );
			next_pkt(last_seq_ordered + 1);
    	}
		if(pkt_recived == WINDOW_SIZE) {//ack comulativo
 			struct pkt packet_ack;
			packet_ack.seqnum = packet.seqnum;
    		packet_ack.acknum = seqnum + 1;
     		packet_ack.checksum = generate_checksum(packet_ack);
			printf("B invia un ack cumulativo ad A %d \n",last_seq_ordered+1);
			printf("packet_ack: %s\n",packet_ack.payload);
        	tolayer3(B,packet_ack);
			ack_sent++;
			pkt_recived = 0;
		}
		//far stampare il buffer
		printf("stampa buffer : ");
		for(int i =0;i<buff_lock;i++){
			printf("%s, ",buff[i].payload);
		}
		printf("\n");

	} else {
		//pacchetto corrotto
		//2ack
		pkt_corrupted++;
		struct pkt packet_ack;
        packet_ack.acknum = last_seq_ordered;
		packet_ack.seqnum = packet.seqnum;
       	packet_ack.checksum = generate_checksum(packet_ack);
		printf("B invia un DOPPIO ack per il pacchetto corrotto %s, numero pacchetti corrotti : %d \n", packet.payload,pkt_corrupted);
		tolayer3(B, packet_ack);
		ack_sent++;
        printf("B invia ack 1 di 2, ack inviati : %d\n",ack_sent);
		tolayer3(B, packet_ack);
		ack_sent++;
        printf("B invia ack 2 di 2, ack inviati : %d \n",ack_sent);
	}

}
/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void
B_init (void)
{
	last_seq_ordered = -1;
   	buff_lock = 0;
   	pkt_recived = 0;
    tot_pkt_recived = 0;
}

/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/


struct event {
  double evtime;           /* event time */
  int evtype;             /* event type code */
  int eventity;           /* entity where event occurs */
  struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
  struct event *prev;
  struct event *next;
};
struct event *evlist = NULL;   /* the event list */

/* Advance declarations. */
void init(void);
void generate_next_arrival(void);
void insertevent(struct event *p);


/* possible events: */
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1


int TRACE = 0;              /* for debugging purpose */
int fileoutput;
double time_now = 0.000;
int WINDOW_SIZE;
int LIMIT_SEQNO;
double RXMT_TIMEOUT;
double lossprob;            /* probability that a packet is dropped  */
double corruptprob;         /* probability that one bit is packet is flipped */
double lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/
int nsim = 0;
int nsimmax = 0;
unsigned int seed[5];         /* seed used in the pseudo-random generator */

int
main(int argc, char **argv)
{
  struct event *eventptr;
  struct msg  msg2give;
  struct pkt  pkt2give;

  int i,j;

  init();
  A_init();
  B_init();

  while (1) {
    eventptr = evlist;            /* get next event to simulate */
    if (eventptr==NULL)
      goto terminate;
    evlist = evlist->next;        /* remove this event from event list */
    if (evlist!=NULL)
      evlist->prev=NULL;
    if (TRACE>=2) {
      printf("\nEVENT time: %f,",eventptr->evtime);
      printf("  type: %d",eventptr->evtype);
      if (eventptr->evtype==0)
        printf(", timerinterrupt  ");
      else if (eventptr->evtype==1)
        printf(", fromlayer5 ");
      else
        printf(", fromlayer3 ");
      printf(" entity: %d\n",eventptr->eventity);
    }
    time_now = eventptr->evtime;    /* update time to next event time */
    if (eventptr->evtype == FROM_LAYER5 ) {
      generate_next_arrival();   /* set up future arrival */
      /* fill in msg to give with string of same letter */
      j = nsim % 26;
      for (i=0;i<20;i++)
        msg2give.data[i]=97+j;
      msg2give.data[19]='\n';
      nsim++;
      if (nsim==nsimmax+1)
        break;
      A_output(msg2give);
    } else if (eventptr->evtype ==  FROM_LAYER3) {
      pkt2give.seqnum = eventptr->pktptr->seqnum;
      pkt2give.acknum = eventptr->pktptr->acknum;
      pkt2give.checksum = eventptr->pktptr->checksum;
      for (i=0;i<20;i++)
        pkt2give.payload[i]=eventptr->pktptr->payload[i];
      if (eventptr->eventity == A)      /* deliver packet by calling */
        A_input(pkt2give);            /* appropriate entity */
      else
        B_input(pkt2give);
      free(eventptr->pktptr);          /* free the memory for packet */
    } else if (eventptr->evtype ==  TIMER_INTERRUPT) {
      A_timerinterrupt();
    } else  {
      printf("INTERNAL PANIC: unknown event type \n");
    }
    free(eventptr);
  }
  terminate:
    printf("Simulator terminated at time %.12f\n",time_now);
    return (0);
}


void init(void)                         /* initialize the simulator */
{
  int i=0;
  printf("----- * ARQ Network Simulator Version 1.1 * ------ \n\n");
  printf("Enter number of messages to simulate: ");
  scanf("%d",&nsimmax);
  printf("Enter packet loss probability [enter 0.0 for no loss]:");
  scanf("%lf",&lossprob);
  printf("Enter packet corruption probability [0.0 for no corruption]:");
  scanf("%lf",&corruptprob);
  printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
  scanf("%lf",&lambda);
  printf("Enter window size [>0]:");
  scanf("%d",&WINDOW_SIZE);
  LIMIT_SEQNO = 2*WINDOW_SIZE;
  printf("Enter retransmission timeout [> 0.0]:");
  scanf("%lf",&RXMT_TIMEOUT);
  printf("Enter trace level:");
  scanf("%d",&TRACE);
  printf("Enter random seed: [>0]:");
  scanf("%d",&seed[0]);
  for (i=1;i<5;i++)
    seed[i]=seed[0]+i;
  fileoutput = open("OutputFile.txt", O_CREAT|O_WRONLY|O_TRUNC,0644);
  if (fileoutput<0)
    exit(1);
  ntolayer3 = 0;
  nlost = 0;
  ncorrupt = 0;
  time_now=0.0;                /* initialize time to 0.0 */
  generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* mrand(): return a double in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
int nextrand(int i)
{
  seed[i] = seed[i]*1103515245+12345;
  return (unsigned int)(seed[i]/65536)%32768;
}

double mrand(int i)
{
  double mmm = 32767;
  double x;                   /* individual students may need to change mmm */
  x = nextrand(i)/mmm;            /* x should be uniform in [0,1] */
  if (TRACE==0)
    printf("%.16f\n",x);
  return(x);
}


/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
void
generate_next_arrival(void)
{
  double x,log(),ceil();
  struct event *evptr;


  if (TRACE>2)
    printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

  x = lambda*mrand(0)*2;  /* x is uniform on [0,2*lambda] */
  /* having mean of lambda        */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime =  time_now + x;
  evptr->evtype =  FROM_LAYER5;
  evptr->eventity = A;
  insertevent(evptr);
}

void
insertevent(p)
  struct event *p;
{
  struct event *q,*qold;

  if (TRACE>2) {
    printf("            INSERTEVENT: time is %f\n",time_now);
    printf("            INSERTEVENT: future time will be %f\n",p->evtime);
  }
  q = evlist;     /* q points to header of list in which p struct inserted */
  if (q==NULL) {   /* list is empty */
    evlist=p;
    p->next=NULL;
    p->prev=NULL;
  } else {
    for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
      qold=q;
    if (q==NULL) {   /* end of list */
      qold->next = p;
      p->prev = qold;
      p->next = NULL;
    } else if (q==evlist) { /* front of list */
      p->next=evlist;
      p->prev=NULL;
      p->next->prev=p;
      evlist = p;
    } else {     /* middle of list */
      p->next=q;
      p->prev=q->prev;
      q->prev->next=p;
      q->prev=p;
    }
  }
}

void
printfevlist(void)
{
  struct event *q;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
  }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
  struct event *q /* ,*qold */;
  if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time_now);
  for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
      /* remove this event */
      if (q->next==NULL && q->prev==NULL)
        evlist=NULL;         /* remove first and only event on list */
      else if (q->next==NULL) /* end of list - there is one in front */
        q->prev->next = NULL;
      else if (q==evlist) { /* front of list - there must be event after */
        q->next->prev=NULL;
        evlist = q->next;
      } else {     /* middle of list */
        q->next->prev = q->prev;
        q->prev->next =  q->next;
      }
      free(q);
      return;
    }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


void starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
double increment;
{
  struct event *q;
  struct event *evptr;

  if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",time_now);
  /* be nice: check to see if timer is already started, if so, then  warn */
  /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
  for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
    }
  /* create future event for when timer goes off */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime =  time_now + increment;
  evptr->evtype =  TIMER_INTERRUPT;
  evptr->eventity = AorB;
  insertevent(evptr);
}


/************************** TOLAYER3 ***************/
void
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
  struct pkt *mypktptr;
  struct event *evptr,*q;
  double lastime, x;
  int i;


  ntolayer3++;

  /* simulate losses: */
  if (mrand(1) < lossprob)  {
    nlost++;
    if (TRACE>0)
      printf("          TOLAYER3: packet being lost\n");
    return;
  }

  /* make a copy of the packet student just gave me since he/she may decide */
  /* to do something with the packet after we return back to him/her */
  mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
  mypktptr->seqnum = packet.seqnum;
  mypktptr->acknum = packet.acknum;
  mypktptr->checksum = packet.checksum;
  for (i=0;i<20;i++)
    mypktptr->payload[i]=packet.payload[i];
  if (TRACE>2)  {
    printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
    mypktptr->acknum,  mypktptr->checksum);
  }

  /* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
  /* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
  lastime = time_now;
  for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) )
      lastime = q->evtime;
  evptr->evtime =  lastime + 1 + 9*mrand(2);



  /* simulate corruption: */
  if (mrand(3) < corruptprob)  {
    ncorrupt++;
    if ( (x = mrand(4)) < 0.75)
      mypktptr->payload[0]='?';   /* corrupt payload */
    else if (x < 0.875)
      mypktptr->seqnum = 999999;
    else
      mypktptr->acknum = 999999;
    if (TRACE>0)
      printf("          TOLAYER3: packet being corrupted\n");
  }

  if (TRACE>2)
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
}

void
tolayer5(datasent)
  char datasent[20];
{
  write(fileoutput,datasent,20);
}
