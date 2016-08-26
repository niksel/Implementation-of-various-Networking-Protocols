/*
|| Author      : Nikhil Selvaraj
||
|| Created     : 15-Oct-2015
||
|| File Name   : nikhil.c
||
|| Version     : 1.0
||
|| Description : Socket network program with command shell interface
||
|| Usage       : This program is used to connect and establish connection among 
||	         a server and multiple peers. Execute various commands on a simulated shell.
||	         Transfer files between peeers.
||
*/

#include<stdio.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<string.h>
#include<netdb.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/time.h>

#define TRUE 1
#define FALSE 0
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define LENGTH 512 

char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }
  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct filedb
  {char fname[100];
   int filesockfd;
  };
struct conctdb
  {
    char hostname[100];
    char ipaddr[50];
    int connsockfd;
  };
struct addrdb
  {
    char hostname[100];
    char ipaddr[50];
    char theport[10];
    int thesockfd;
    int srvr;
  };

int main(int argc,char *argv[])
{
 int sockfd,new_fd,addrlen,sockfd1,sockfd2,sockfd3,sockfd4,getsockfd,putsockfd;
 struct addrinfo hints, *servinfo, *p, *clientinfo, *peerinfo;
 struct sockaddr_storage their_addr;
 struct sockaddr_in *addr_v4,sa;
 struct timeval timeout;
 socklen_t sin_size;
 int res,berr,yes=1,srno,flcount=0;
 void *addr;
 char *ipver;
 char s[INET6_ADDRSTRLEN],msg[2000],msg1[2000],**args,*line,cbuff[100];
 unsigned char *sdata;
 fd_set master;    // master file descriptor list
 fd_set read_fds;  // temp file descriptor list for select()
 int fdmax;        // maximum file descriptor number
 int i,j,k,rv,nbytes,prcount,kbcount,arrcount;
 // structures
 struct conctdb conct_detl[3]; //max three peer connections
 struct addrdb addr_detl[10], peer_detl[10]; // hold addresses
 struct filedb file_detl[100];
 time_t now;
 time(&now);
 

 if(strcmp(argv[1],"s")==0 || strcmp(argv[1],"S")==0)
 {
  if (!argv[2]) {
    printf("Please input PORT No.\n");
    exit(1); }

  printf("***** S E R V E R *****\n");

  memset(&hints, 0, sizeof hints);
  hints.ai_family=AF_UNSPEC;
  hints.ai_socktype=SOCK_STREAM;
  hints.ai_flags=AI_PASSIVE; 

  if((res=getaddrinfo("timberlake.cse.buffalo.edu",argv[2],&hints,&servinfo))!=0)
   {
    fprintf(stderr,"getaddrinfo:%s\n", gai_strerror(res));
    return 1;
   }
  for(p=servinfo;p!=NULL;p=p->ai_next)
  {
   if(p->ai_family==AF_INET)
   {
    struct sockaddr_in *ipv4=(struct sockaddr_in *)p->ai_addr;
    addr = &(ipv4->sin_addr);
    ipver = "IPv4";
   }
   inet_ntop(p->ai_family,addr,s,sizeof s);
   }

   printf("Server waiting for connection %s\n",s);
  if((sockfd=socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol))<0)
  {
   printf("Socket creation error!\n");
   exit(0);
  }
  arrcount = 0;
  strcpy(addr_detl[0].hostname,"timberlake.cse.buffalo.edu");
  strcpy(addr_detl[0].ipaddr,s);
  strcpy(addr_detl[0].theport,argv[2]);
  addr_detl[0].thesockfd = sockfd;
  addr_detl[0].srvr = 1;

  if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)
  {
   perror("setsockopt error\n");
   exit(1);
  }
  if((berr=bind(sockfd,servinfo->ai_addr,servinfo->ai_addrlen))<0)
  {
   printf("Binding error!\n");
   exit(0);
  }
  if(listen(sockfd,10)<0)
  {
   printf("Listening error!\n");
   exit(0);
  }
  FD_ZERO(&master);    // clear the master and temp sets
  FD_ZERO(&read_fds);

   // add the listener to the master set
    FD_SET(sockfd, &master);

    // keep track of the biggest file descriptor
    fdmax = sockfd;

    // *** Main loop ***
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select error\n");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // connection detected
                if (i == sockfd) {
                    // handle new connections
                    sin_size = sizeof their_addr;
                    new_fd = accept(sockfd,
                        (struct sockaddr *)&their_addr,
                        &sin_size);

                    if (new_fd == -1) {
                        perror("Client connection accept error\n");
                    } else {
                        FD_SET(new_fd, &master); // add to master set
                        if (new_fd > fdmax) // keep track of the max
                            fdmax = new_fd;
                        arrcount += 1;
                        addr_detl[arrcount].srvr = 0;
                        strcpy(addr_detl[arrcount].ipaddr,inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr*)&their_addr),
                                s, INET6_ADDRSTRLEN));
                        addr_detl[arrcount].thesockfd = new_fd;

                        printf("Server got new connection from %s on "
                            "socket %d\n",addr_detl[arrcount].ipaddr,
                             new_fd);
                        break;
                       }
                } 
                else {
                    // handle data from a client
                    if ((nbytes = recv(i, msg, sizeof msg, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("CLIENT: socket %d hung up\n", i);
                        } else {
                            perror("recv\n");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        // refresh all clients
                        for(j = 0; j <= fdmax; j++) {
                          if (j == sockfd) continue;
                          //
                            srno = 0;
                            if (FD_ISSET(j, &master)) {
                             for (k = 0; k <= arrcount; k++) {
                              if (FD_ISSET(addr_detl[k].thesockfd, &master)) {
                               strcpy(msg1,"REG ");
                               sprintf(cbuff,"%1d",srno);
                               strcat(msg1,cbuff);
                               strcat(msg1," HOST ");
                               strcat(msg1,addr_detl[k].hostname);
                               strcat(msg1," IP ");
                               strcat(msg1,addr_detl[k].ipaddr);
                               strcat(msg1," PORT ");
                               strcat(msg1,addr_detl[k].theport);
                               strcat(msg1," SOCK ");
                               sprintf(cbuff,"%1d",addr_detl[k].thesockfd);
                               strcat(msg1,cbuff);
                               srno += 1;
                               if (send(j,msg1, sizeof msg1, 0) == -1)
                                  perror("send error\n");
                              } // fd_isset k
                              else addr_detl[k].thesockfd = 0; //remove
                            } // for k
                          } // fd_isset j
                         }  // for j
                         break;
                    }
                    else {
                       // we got some data from a client
                       if (strlen(msg) != 0)
                         args = lsh_split_line(msg);
                       if (strcmp(args[0],"PORT") == 0) {
                        //
                        for(j = 0; j <= fdmax; j++) {
                          if (j == sockfd) continue;
                          //
                            srno = 0;
                            if (FD_ISSET(j, &master)) {
                             for (k = 0; k <= arrcount; k++) {
                              if (FD_ISSET(addr_detl[k].thesockfd, &master)) {
                               if (addr_detl[k].thesockfd == i) {
                                // store client listening port and name
                                strcpy(addr_detl[k].theport,args[1]);
                                strcpy(addr_detl[k].hostname,args[2]);
                                }
                               strcpy(msg1,"REG ");
                               sprintf(cbuff,"%1d",srno);
                               strcat(msg1,cbuff);
                               strcat(msg1," HOST ");
                               strcat(msg1,addr_detl[k].hostname);
                               strcat(msg1," IP ");
                               strcat(msg1,addr_detl[k].ipaddr);
                               strcat(msg1," PORT ");
                               strcat(msg1,addr_detl[k].theport);
                               strcat(msg1," SOCK ");
                               sprintf(cbuff,"%1d",addr_detl[k].thesockfd);
                               strcat(msg1,cbuff);
                               srno += 1;
                               if (send(j,msg1, sizeof msg1, 0) == -1)
                                  perror("send error\n");
                              } // fd_isset k
                            } // for k
                          } // fd_isset j
                         }  // for j
                         break;
                        } //port
                     else if (strcmp(args[0],"FILE") == 0) // transfer log
                      {
                      strcpy(file_detl[flcount].fname,args[1]);
                      file_detl[flcount].filesockfd = i;
                      flcount += 1;
                      }
                     else if (strcmp(args[0],"SYNC") == 0)
                      {
                      for(j = 0; j <= flcount; j++) {

                        printf("nikhil\n");

                       }
                      }

                    } // rcv data
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)
  }
 else if(strcmp(argv[1],"c")==0 || strcmp(argv[1],"C")==0)
 {
  if (!argv[2]) {
    printf("Please input PORT No.\n");
    exit(1); }
  printf("***** C L I E N T *****\n");

  ssize_t bufsize = 0; // getline buffer allocation
  char getfile[200],sdbuf[LENGTH],chostname[1023],ctemp[100];
  int regrd=0,fstart=0,write_sz,fs_block_sz,peerexist,myerr;
  FILE *fr,*fs;

  struct hostent *host_ent;
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);

  memset(&hints, 0, sizeof hints);
  hints.ai_family=AF_UNSPEC;
  hints.ai_socktype=SOCK_STREAM;
  hints.ai_flags=AI_PASSIVE;

  prcount = 0;
  conct_detl[0].connsockfd = 0;
  conct_detl[1].connsockfd = 0;
  conct_detl[2].connsockfd = 0;


  if((res=getaddrinfo(NULL,argv[2],&hints,&clientinfo))!=0)
   {
    fprintf(stderr,"getaddrinfo:%s\n", gai_strerror(res));
    return 1;
   }
  for(p=clientinfo;p!=NULL;p=p->ai_next)
   {
   if(p->ai_family==AF_INET)
    {
     struct sockaddr_in *ipv4=(struct sockaddr_in *)p->ai_addr;
     addr=&(ipv4->sin_addr);
     ipver="IPv4";
    }
   inet_ntop(p->ai_family,addr,s,sizeof s);
   }
   printf("Client running at %s\n",s);
  if((sockfd=socket(clientinfo->ai_family,clientinfo->ai_socktype,clientinfo->ai_protocol))<0)
  {
   printf("Socket creation error!\n");
   exit(0);
  }
  if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)
  {
   perror("setsockopt");
   exit(1);
  }
  if((berr=bind(sockfd,clientinfo->ai_addr,clientinfo->ai_addrlen))<0)
   {
    printf("Binding error!\n");
    exit(0);
   }

  /*getnameinfo(get_in_addr((struct sockaddr*)&clientinfo),INET6_ADDRSTRLEN,
           chostname,sizeof chostname, NULL, 0,0);*/
   if (strlen(chostname) == 0)
     strcpy(chostname,hostname);
  
  if(listen(sockfd,10)<0)
  {
   printf("Listening error!\n");
   exit(0);
  }
   FD_ZERO(&master);    // clear the master and temp sets
   FD_ZERO(&read_fds);
   // add the listener to the master set
   FD_SET(fileno(stdin), &master); // for keyboard
   FD_SET(sockfd, &master);
   // keep track of the biggest file descriptor
   fdmax = sockfd; // so far, it's this one
    // main loop
    printf("->\n");
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select error\n");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
              if (!FD_ISSET(fileno(stdin), &read_fds)) { // Not keyboard
                if (i == sockfd) {
                    // handle new connections
                    if (prcount == 3) {
                       printf("Max peer connect of 3 exceeded!\n");
                       break;
                      }
                    sin_size = sizeof their_addr;
                    new_fd = accept(sockfd,
                        (struct sockaddr *)&their_addr,
                        &sin_size);

                    if (new_fd == -1) {
                        perror("accept error");
                    } else {
                        FD_SET(new_fd, &master); // add to master set
                        if (new_fd > fdmax) {    // keep track of the max
                            fdmax = new_fd;
                        }
                        printf("New Client connection from %s on "
                            "socket %d\n",
                            inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr*)&their_addr),
                                s, INET6_ADDRSTRLEN),
                            new_fd);
                        //
                        if (conct_detl[0].connsockfd == 0)
                         {
                         strcpy(conct_detl[0].ipaddr,inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr*)&their_addr),
                                s, INET6_ADDRSTRLEN));
                         conct_detl[0].connsockfd = new_fd;
                         prcount += 1;
                         }
                        else if (conct_detl[1].connsockfd == 0)
                         {
                         strcpy(conct_detl[1].ipaddr,inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr*)&their_addr),
                                s, INET6_ADDRSTRLEN));
                         conct_detl[1].connsockfd = new_fd;
                         prcount += 1;
                         }
                        else if (conct_detl[2].connsockfd == 0)
                         {
                         strcpy(conct_detl[2].ipaddr,inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr*)&their_addr),
                                s, INET6_ADDRSTRLEN));
                         conct_detl[2].connsockfd = new_fd;
                         prcount += 1;
                         }
                        break;
                    }
                } else {
                    // handle data from a peer
                    if ((nbytes = recv(i, msg, sizeof msg, 0)) <= 0) {
                        // got error or connection closed by peer
                        if (nbytes == 0) {
                            // connection closed
                            printf("HOST socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        if (conct_detl[0].connsockfd == i) {
                          strcpy(conct_detl[0].hostname,"deleted");
                          conct_detl[0].connsockfd = 0;
                          prcount -= 1;
                          }
                        else if (conct_detl[1].connsockfd == i) {
                          strcpy(conct_detl[1].hostname,"deleted");
                          conct_detl[1].connsockfd = 0;
                          prcount -= 1;
                          }
                        else if (conct_detl[2].connsockfd == i) {
                          strcpy(conct_detl[2].hostname,"deleted");
                          conct_detl[2].connsockfd = 0;
                          prcount -= 1;
                          }
                      } 
                    else {
                        // we got some data from a peer
                       if (fstart==0 || i==peer_detl[0].thesockfd) // master socket
                         {if (strlen(msg) != 0)
                           args = lsh_split_line(msg);}
                       else {
                         strcpy(ctemp,"DATA"); 
                         args = lsh_split_line(ctemp);}
                       //
                      if (strcmp(args[0],"REG") == 0)
                       {
                        arrcount = atoi(args[1]);
                        strcpy(peer_detl[arrcount].hostname,args[3]);
                        strcpy(peer_detl[arrcount].ipaddr,args[5]);
                        strcpy(peer_detl[arrcount].theport,args[7]);
                        peer_detl[arrcount].thesockfd = atoi(args[9]);  //convert to int
                        //
                        if (arrcount == 0) {
                         peer_detl[arrcount].srvr = 1;
                         printf("id:     Hostname            ");
                         printf("      IP address         Port No.\n");
                         }
                        else peer_detl[arrcount].srvr = 0;
                        // print detl
                        printf("%d. ",arrcount+1);
                        printf("%-30s ",peer_detl[arrcount].hostname);
                        printf("%-20s ",peer_detl[arrcount].ipaddr);
                        printf("%-8s\n",peer_detl[arrcount].theport);
                       } // REG
                      else if (strcmp(args[0],"FILE")==0 && fstart == 0)
                       {
                       fstart = 1;
                       //strcpy(getfile,args[1]);
                       strcpy(getfile,"/home/nikhil/Downloads/f.txt");
                       fr = fopen(getfile, "a");
                       if(fr == NULL) {
                         printf("File %s Cannot be opened for write\n", getfile);
                         break;}
                       printf("Start %s at %s\n",getfile,ctime(&now));
                       } // FILE
                      else if (strcmp(args[0],"DATA")==0 && fstart == 1)
                       {
                       write_sz = fwrite(msg, sizeof(char), nbytes, fr);
                       if(write_sz < nbytes)
                         {perror("File write failed.\n");
                          fclose(fr);
                          fstart = 0;
                          break;}
                       if (nbytes < LENGTH)
                        {
                        fclose(fr);
                        printf("End %s at %s\n",getfile,ctime(&now));
                        strcpy(msg1,"FILE ");
                        strcat(msg1,getfile);
                        if (send(sockfd1,msg1, sizeof msg1, 0) == -1) // to server
                              perror("send error\n");
                        memset(getfile,0,sizeof getfile);
                        fstart = 0;
                        }
                       }
                      else if (strcmp(args[0],"GET") == 0) // send file
                       { // send file packet
                       strcpy(getfile,args[1]);
                       fs = fopen(getfile, "r");
                       if(fs == NULL) {
                         printf("ERROR: File %s not found.\n", getfile);
                         break;}
                        strcpy(msg1,"FILE ");
                        strcat(msg1,getfile);
                        if (send(i,msg1, sizeof msg1, 0) == -1)
                            perror("send error\n");
                        memset(sdbuf,0, LENGTH);
                        //
                        while((fs_block_sz = fread(sdbuf, sizeof(char), LENGTH, fs)) > 0)
                         {
                         if(send(i, sdbuf, fs_block_sz, 0) < 0) {
                          printf("ERROR: Failed to send file %s. (errno = %d)\n", getfile, errno);
                          break;}
                         memset(sdbuf,0, LENGTH);
                         } //while
                        fclose(fs);
                       } // get
                      else if (strcmp(args[0],"HOST") == 0) // store peer host
                       {
                       if (conct_detl[0].connsockfd == i)
                         strcpy(conct_detl[0].hostname,args[1]);
                       else if (conct_detl[1].connsockfd == i)
                         strcpy(conct_detl[1].hostname,args[1]);
                       else if (conct_detl[2].connsockfd == i)
                         strcpy(conct_detl[2].hostname,args[1]);
                       } // host

                   }
                } // END handle data from client
              } else { 
                // ******Handle keyboard
                line = NULL;
                if (!getline(&line, &bufsize, stdin)) {
                   if (ferror(stdin)) {
                     perror("stdin");
                     exit(1);
                     }
                   }
                if (strlen(line) != 0) {
                  kbcount = strlen(line);
                  args = lsh_split_line(line);
                  }
              if (kbcount <= 2) // empty enter key
                printf("Invalid Input!\n");
              else if (strcmp(args[0],"HELP") == 0 || strcmp(args[0],"help") == 0)
                {
                printf("CREATOR   Displays user details\n");
                printf("DISPLAY   Shows IP address and Port\n");
                printf("REGISTER  Registers client with the server\n");
                printf("CONNECT   Establish connection with a registered client\n");
                printf("LIST      Display details of all the connections\n");
                printf("TERMINATE Terminates a connection\n");
                printf("QUIT      Close all connections and terminate this process\n");
                printf("GET       Download a file from specified Host\n");
                printf("PUT       Put a file to the specified Host\n");
                printf("SYNC      Synchronises files among hosts\n");
                }
              else if (strcmp(args[0],"CREATOR") == 0 || strcmp(args[0],"creator") == 0)
                {
                printf("Nikhil Selvaraj\n");
                printf("Nikhil Selvaraj\n");
                printf("nikhilse@buffalo.edu\n");
                }
              else if (strcmp(args[0],"DISPLAY") == 0 || strcmp(args[0],"display") == 0)
                {
                printf("Client IP %s ",
                    inet_ntop(clientinfo->ai_family,get_in_addr((struct sockaddr*)&clientinfo),
                              s, sizeof s));
                printf("Port:%d\n",ntohs(((struct sockaddr_in *)clientinfo->ai_addr)->sin_port));
                }
              else if (strcmp(args[0],"REGISTER") == 0 || strcmp(args[0],"register") == 0)
                {
                if (regrd != 0) {
                   printf("Client already registered\n");
                   break;
                    }
                if (!args[1] || !args[2]) {
                  printf("Please enter server name and port\n");
                  break;}

                memset(&hints,0,sizeof hints);
                hints.ai_family=AF_UNSPEC;
                hints.ai_socktype=SOCK_STREAM;

                if((res=getaddrinfo(args[1],args[2],&hints,&servinfo))!=0)
                 {
                  printf("Error\n");
                 }
                 if((sockfd1=socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol))<0)
                   printf("Socket creation error!\n");
                 if((connect(sockfd1,servinfo->ai_addr,servinfo->ai_addrlen))==-1)
                   printf("Connect error!\n");
                 else {
                   FD_SET(sockfd1, &master); // add to master set
                   if (sockfd1 > fdmax)  // keep track of the max
                       fdmax = sockfd1;
                   }
                   regrd = 1;
                   // send client PORT to server and display peer IP and Ports
                   strcpy(msg1,"PORT ");
                   strcat(msg1,argv[2]);
                   strcat(msg1," ");
                   strcat(msg1,chostname);
                   if((nbytes=send(sockfd1,msg1,sizeof msg1,0))==-1)
                       printf("Send error!\n");
                   break;
                }
              else if (strcmp(args[0],"CONNECT") == 0 || strcmp(args[0],"connect") == 0)
               {
                if (regrd == 0) {
                   printf("Client Not registered yet\n");
                   break;
                    }
               if (!args[1] || !args[2]) {
                  printf("Please enter client name and port\n");
                  break;}
               if ((strcmp(conct_detl[0].hostname,args[1]) == 0) ||
                         (strcmp(conct_detl[1].hostname,args[1]) == 0) ||
                            (strcmp(conct_detl[2].hostname,args[1]) == 0)) {
                  printf("Peer already connected!\n");
                  break;}
               if (prcount == 3)
                  printf("Max peer connect of 3 exceeded!\n");
               else {
                  // check the target peer is registered
                  peerexist = 0;
                  for (k=0;k<=arrcount;k++)
                   {
                   if (strcmp(peer_detl[k].hostname,args[1]) == 0)
                     {
                     memset(&hints,0,sizeof hints);
                     hints.ai_family=AF_UNSPEC;
                     hints.ai_socktype=SOCK_STREAM;
                     //if((getaddrinfo(args[1],args[2],&hints,&peerinfo))!=0)
                     if((getaddrinfo(peer_detl[k].ipaddr,args[2],&hints,&peerinfo))!=0)
                       {printf("Error connect getaddrinfo\n");
                        break;}
                     peerexist = 1;
                     }
                   } //for k
                 if (peerexist == 0) {
                   printf("Target connection is not registered!\n");
                   break;}
                 //

                 if (conct_detl[0].connsockfd == 0)
                  {
                  if((sockfd2=socket(peerinfo->ai_family,peerinfo->ai_socktype,peerinfo->ai_protocol))<0)
                    printf("Socket creation error!\n");
                  if((connect(sockfd2,peerinfo->ai_addr,peerinfo->ai_addrlen))==-1)
                      printf("Peer Connect error!\n");
                  else {
                   FD_SET(sockfd2, &master); // add to master set
                   if (sockfd2 > fdmax)  // keep track of the max
                      fdmax = sockfd2;
                   // maintain peer connections
                   strcpy(conct_detl[0].hostname,args[1]);
                   strcpy(conct_detl[0].ipaddr,inet_ntop(peerinfo->ai_family,
                               get_in_addr((struct sockaddr*)&peerinfo),s, sizeof s));
                   conct_detl[0].connsockfd = sockfd2;
                   prcount += 1;
                   printf("Peer Connected\n");
                   // send client hostname to peer
                   strcpy(msg1,"HOST ");
                   strcat(msg1,args[1]);
                   if((nbytes=send(sockfd2,msg1,sizeof msg1,0))==-1)
                       printf("Send error!\n");
                   }}
                  else if (conct_detl[1].connsockfd == 0) {
                  if((sockfd3=socket(peerinfo->ai_family,peerinfo->ai_socktype,peerinfo->ai_protocol))<0)
                    printf("Socket creation error!\n");
                  if((connect(sockfd3,peerinfo->ai_addr,peerinfo->ai_addrlen))==-1)
                      printf("Peer Connect error!\n");
                  else {
                   FD_SET(sockfd3, &master); // add to master set
                   if (sockfd3 > fdmax)  // keep track of the max
                      fdmax = sockfd3;
                   strcpy(conct_detl[1].hostname,args[1]);
                   strcpy(conct_detl[1].ipaddr,inet_ntop(peerinfo->ai_family,
                               get_in_addr((struct sockaddr*)&peerinfo),s, sizeof s));
                   conct_detl[1].connsockfd = sockfd3;
                   prcount += 1;
                   printf("Peer Connected\n");
                   // send client hostname to peer
                   strcpy(msg1,"HOST ");
                   strcat(msg1,args[1]);
                   if((nbytes=send(sockfd3,msg1,sizeof msg1,0))==-1)
                       printf("Send error!\n");
                   }}
                  else if (conct_detl[2].connsockfd == 0) {
                  if((sockfd4=socket(peerinfo->ai_family,peerinfo->ai_socktype,peerinfo->ai_protocol))<0)
                    printf("Socket creation error!\n");
                  if((connect(sockfd4,peerinfo->ai_addr,peerinfo->ai_addrlen))==-1)
                      printf("Peer Connect error!\n");
                  else {
                   FD_SET(sockfd4, &master); // add to master set
                   if (sockfd4 > fdmax)  // keep track of the max
                      fdmax = sockfd4;
                   strcpy(conct_detl[2].hostname,args[1]);
                   strcpy(conct_detl[2].ipaddr,inet_ntop(peerinfo->ai_family,
                               get_in_addr((struct sockaddr*)&peerinfo),s, sizeof s));
                   conct_detl[2].connsockfd = sockfd4;
                   prcount += 1;
                   printf("Peer Connected\n");
                   // send client hostname to peer
                   strcpy(msg1,"HOST ");
                   strcat(msg1,args[1]);
                   if((nbytes=send(sockfd4,msg1,sizeof msg1,0))==-1)
                       printf("Send error!\n");
                   }
                  }
                 }
                }
              else if (strcmp(args[0],"LIST") == 0 || strcmp(args[0],"list") == 0)
                {
                if (regrd == 0) {
                   printf("Client Not registered yet\n");
                   break;
                    }
                for(j = 0; j <= arrcount; j++) {
                  if (j == 0) {
                     printf("id:     Hostname            ");
                     printf("      IP address         Port No.\n");}
                       // print detl
                    printf("%d. ",j+1);
                    printf("%-30s ",peer_detl[j].hostname);
                    printf("%-20s ",peer_detl[j].ipaddr);
                    printf("%-8s\n",peer_detl[j].theport);
                  } // for j
                }
              else if (strcmp(args[0],"TERMINATE") == 0 || strcmp(args[0],"terminate") == 0)
                {
                if (regrd == 0) {
                   printf("Client Not registered yet\n");
                   break;
                    }
                if (!args[1]) {
                  printf("Please enter the connection Id\n");
                  break;}
                if (atoi(args[1]) < 2 || atoi(args[1]) > arrcount+1) {
                  printf("Invalid connection Id\n");
                  break;}
                getsockfd = 0;
                peerexist = 0;
                for (k=0;k<=prcount;k++)
                 {
                 if (strcmp(conct_detl[k].hostname,peer_detl[atoi(args[1])-1].hostname) == 0)
                   {
                   peerexist = k;
                   getsockfd = conct_detl[k].connsockfd;
                   }
                 }
                 if (getsockfd == 0) {
                   printf("Connection does not exist!\n");
                   break;}
                close(getsockfd); // bye!
                FD_CLR(getsockfd, &master); // remove from master set
                strcpy(conct_detl[peerexist].hostname,"deleted");
                conct_detl[peerexist].connsockfd = 0;
                prcount -= 1;
                //
                printf("Connection %s has been terminated\n",args[1]);
                break;
                }
              else if (strcmp(args[0],"GET") == 0 || strcmp(args[0],"get") == 0)
                {
                if (regrd == 0) {
                   printf("Client Not registered yet\n");
                   break;
                    }
                if (!args[1] || !args[2]) {
                  printf("Please enter File details\n");
                  break;}
                if (atoi(args[1]) <= 1) {
                  printf("Download from server not permitted\n");
                  break;}
                else if (atoi(args[1]) > arrcount+1) {
                  printf("Invalid host Id\n");
                  break;}
                getsockfd = 0;
                peerexist = 0;
                for (k=0;k<=prcount;k++)
                 {
                 if (strcmp(conct_detl[k].hostname,peer_detl[atoi(args[1])-1].hostname) == 0)
                   {
                   peerexist = 1;
                   getsockfd = conct_detl[k].connsockfd;
                   }
                 }
                 if (peerexist == 0) {
                   printf("Target peer was not connected!\n");
                   break;}
                //
                strcpy(msg1,"GET ");
                strcat(msg1,args[2]);
                if((nbytes=send(getsockfd,msg1,sizeof msg1,0))==-1)
                       printf("Send error!\n");
                }
              else if (strcmp(args[0],"PUT") == 0 || strcmp(args[0],"put") == 0)
                {
                if (regrd == 0) {
                   printf("Client Not registered yet\n");
                   break;
                    }
                if (!args[1] || !args[2]) {
                  printf("Please enter File details\n");
                  break;}
                if (atoi(args[1]) <= 1) {
                  printf("Upload to server not permitted\n");
                  break;}
                else if (atoi(args[1]) > arrcount+1) {
                  printf("Invalid host Id\n");
                  break;}
                putsockfd = 0;
                peerexist = 0;
                for (k=0;k<=prcount;k++)
                 {
                 if (strcmp(conct_detl[k].hostname,peer_detl[atoi(args[1])-1].hostname) == 0)
                   {
                   peerexist = 1;
                   putsockfd = conct_detl[k].connsockfd;
                   }
                 }
                 if (peerexist == 0) {
                   printf("Target peer was not connected!\n");
                   break;}
                //
                strcpy(getfile,args[2]);
                fs = fopen(getfile, "r");
                if(fs == NULL) {
                   printf("ERROR: File %s not found.\n", getfile);
                   break;}
                strcpy(msg1,"FILE ");
                strcat(msg1,getfile);
                if (send(putsockfd,msg1, sizeof msg1, 0) == -1)
                      perror("send error\n");
                printf("File is being sent...%s\n",ctime(&now));
                memset(sdbuf,0, LENGTH);
                //
                while((fs_block_sz = fread(sdbuf, sizeof(char), LENGTH, fs)) > 0)
                   {
                   if(send(putsockfd, sdbuf, fs_block_sz, 0) < 0)
                     {
                     fprintf(stderr, "ERROR: Failed to send file %s. (errno = %d)\n", getfile, errno);
                     break;
                     }
                     memset(sdbuf,0, LENGTH);
                   } //while
                   fclose(fs);
                   strcpy(msg1,"FILE ");
                   strcat(msg1,getfile);
                   if (send(sockfd1,msg1, sizeof msg1, 0) == -1)
                          perror("send error");
                memset(getfile,0,50);
                printf("File has been sent to Host...%s\n",ctime(&now));
                }
              else if (strcmp(args[0],"SYNC") == 0 || strcmp(args[0],"sync") == 0)
                {
                if (regrd == 0) {
                   printf("Client Not registered yet\n");
                   break;
                    }


                }
              else if (strcmp(args[0],"QUIT") == 0 || strcmp(args[0],"quit") == 0)
                {
                exit(1);
                }
              } // END not keyboard
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)

  }
 else
 {
  printf("Enter a valid choice\n");
 }
return 0;
}

