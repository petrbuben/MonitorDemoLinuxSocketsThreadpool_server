/*
 zadání:
 simulovaná řešení jsou na jednom (či více) počítačích, ale naše monitorovací aplikace je jiném stroji
 (např. si představme, že naše aplikace bude počítat počet měření teploty od jednotlivých zařízení –
 pro každé zařízení zvlášť)

Analýza
 Důležité pro správnou volbu řešení je určit/znát parametry systému, detailní požadavky na zpracování -
 tedy například požadovanou četnost měření, časový rámec, nároky, způsob zpracování

 Vyberme tedy jeden příklad, pro jednoduchou demonstraci kódování jedné verze systému

 Monitorovací aplikace bude soustavně hlídat teplotu systému.
 Pokud dojde ke změně, aplikace ji zaznamená.
 Pokud dojde ke změně teploty páté, aplikace odešle informaci serveru řešení, který informaci zapíše do souboru

 Budeme předpokládat, že změny teplot jsou spíše vyjímkou, která není častá a není to vlastní funkce systému, a
 systém čidel k monitorování není rozsáhlý
*/

/*
Detail design
    Monitorovací aplikace monitoruje teplotu, pro každé zařízení v jiném thread.
    Při změně, tuto zapíše, do operační paměti.
    Až počet dosáhne 5, aplikace jako client naváže spojení se serverem zpracování.
	Pošle název souboru a odečet teplot, které server do daného souboru zapíše.
    Server naslouchá na clientu známé adrese, a je připraven informaci přijmout a zpracovat - zapsat dzaslaná data teplot do určeného souboru
*/

#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h> 
#include <unistd.h>
#include <pthread.h>

#include "myqueue.hpp"

using namespace std;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
//lets threads wait until some condition happens - when not needed, they are suspended
//conditional variables are stateless

void * handle_connection(void * vp, void * varg) {
  char client_message[2000];
  char server_message[2000];

  //memset(client_message,0,sizeof(client_message));
  //memset(server_message,0,sizeof(server_message));

  int clientSocket = *(int*)vp;
    //free(vp); causes dump
  //read - receive recv slightly better, win compatible
  //read/write bez 0
  int r = recv(clientSocket, client_message , 2000 , 0);
  if ( r<0 ){
    //if(read(clientSocket, client_message, 2000) < 0){
    perror("from server: read error");
    exit(-1);
    }

  //print received
  cout<<"client_message received: "<<client_message <<endl;

  //send to client
  //strcpy(server_message,"Message from server");
  sprintf(server_message, "Message from server thread %d", *(int *)varg);

  //send(*(int*)vp, server_message,sizeof(server_message),0);

  int w = write(clientSocket, server_message, sizeof(server_message));
  if(w<0)
    perror("from server: send failed\n");

  //close(*clientSocket);
  //delete clientSocket;

  return nullptr;
}//handle



//server, handle moni
void * handle_moni(void * vp, void * varg) {
  const int size_msg_from_client = 300;
  const int size_msg_to_client = 1000;

  char msg_from_client[size_msg_from_client];
  char msg_to_client[size_msg_to_client];

  int clientSocket = *(int*)vp;
  
  //read file name
  int r = recv(clientSocket, msg_from_client, sizeof(msg_from_client) , 0);
  if ( r<0 ){
    perror("from server: read error");
    exit(-1);
  }

  //temp store file name
  string ofname = msg_from_client;
  //print received
  cout<<"filename msg from client received: "<<msg_from_client <<endl;

  //message to client
  sprintf(msg_to_client, "Server thread %d, received filename %s", 
  *(int *)varg, msg_from_client);

  int w = write(clientSocket, msg_to_client, sizeof(msg_to_client));
  if(w < 0){
    perror("from server: write failed\n");
    exit(-1);
  }

  //read 5 temp data reading from client
   r = recv(clientSocket, msg_from_client, sizeof(msg_from_client), 0);
  if (r < 0){
    perror("from server: read error");
    exit(-1);
  }

  //print received
  cout<<"data msg from client received: "<<msg_from_client <<endl;

  //message to client
  sprintf(msg_to_client, "Server thread %d, received %s as temperature data reading", 
  *(int *)varg, msg_from_client);

  //copy data back to client
  w = write(clientSocket, msg_to_client, sizeof(msg_to_client));
  if(w < 0){
    perror("from server: write failed\n");
    exit(-1);
  }
  

  //write a file on sever machine
  ofstream ofile(ofname, ios::out);
  ofile<<msg_from_client;
  ofile.close();
 
  close(clientSocket);
  return nullptr;
}//serve handle moni



void* thread_function(void* vid){
  int * pclient = new int;
  
  while(1){
    pthread_mutex_lock(&mutex);
    //not wait here
    if( (pclient = dequeue()) == nullptr){//only wait if no new work
          pthread_cond_wait(&cond_var,&mutex); //suspend thread until signaled, releases the lock
          //suspended threads are no using cpu
          //try again
          pclient = dequeue();
    }
    pthread_mutex_unlock(&mutex);

    //denial of service
    //can ring server to halt by long connections
    //remedy - event driven programming model and
    //asyncronous i/o

    if(pclient != nullptr){
      //there is a connection
      printf("thread %d will handle new connection\n", *(int*)vid);
      //handle_connection(pclient, vid);
      handle_moni(pclient, vid);
    }
  }//while

  return nullptr;
}//thread f


int main(int argc, char ** argv){

  cout << "server\n";

  int serverSocket, clientSocket;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size;

  #define THREAD_POOL_SIZE 5
  pthread_t thread_pool[THREAD_POOL_SIZE];

  int *p[THREAD_POOL_SIZE];
  //tpool create threads just to hang here, handle future connections
 for (int s = 0; s < THREAD_POOL_SIZE; s++)
  {
    p[s] = new int(s); // must be on heap
    pthread_create(&thread_pool[s],NULL, thread_function, p[s]);
    printf("thread %d created\n", s);
  }

  serverSocket = create_server_socket();

  while(1){
    //Accept call creates a new socket for the incoming connection
    addr_size = sizeof (serverStorage);

    //waits here till client request
    clientSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);
    if(clientSocket < 0){
      perror("server client socket creation error");
      exit(-1);
    }
    cout<<"\nserver connected - accept() - new client request\n";

    //thread safe passing arg
    //int * pclient = (int*)malloc(sizeof(int));
    //pclient = &clientSocket;

    //tpool - put connections into a datastructure - queue
    pthread_mutex_lock(&mutex);
    enqueue(&clientSocket); 
    pthread_cond_signal(&cond_var); //signal to dequeue to other threads
    pthread_mutex_unlock(&mutex);
  }//while

  for (size_t s = 0; s < THREAD_POOL_SIZE; s++){
     pthread_join(thread_pool[s], nullptr);
     delete p[s];
  }

  return 0;
}//main



