/*
|| Author      : Nikhil Selvaraj
||
|| Created     : 20-Nov-2015
||
|| File Name   : nikhil.c
||
|| Version     : 2.5
||
|| Description : Distance Vector
||
|| Usage       : This program (Distance vector protocol) is used to implement
||               UDP server and send routing table to neighbor servers.
||               Also executes various commands on a simulated shell.
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
#define LSH_TOK_DELIM1 "|"
#define LENGTH 512
#define MAX_VALUE 99

struct addrdb
  {
    int srvrid;
    char ipaddr[50];
    char theport[10];
  };

struct costdb
  {
    int srvrid;
    int nbrid;
    int cost;
    int orig_cost;
    int pktrcvd;
    int disabled;
  };

// declare functions
double now(void);
char **lsh_split_line(char *line, int delim);
void *get_in_addr(struct sockaddr *sa);
void makepack(int srvrcnt,int edgecnt,int costcnt,char *msg1,
         char *fromport,char *fromip,struct costdb *cost_detl,struct addrdb *addr_detl);
void BellmanFordEval(int distances[],struct costdb *cost_detl,
                   int srvrcnt,int costcnt,int adjmatrix[][6]);


int main(int argc,char *argv[])
 {
  int sockfd,addrlen;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_in *addr_v4,sa,si_other;
  struct sockaddr_storage their_addr;
  struct timeval timeout;
  socklen_t sin_size;
  int res,berr,yes=1,srno,distances[6],node,sourcenode,destinationnode;
  void *addr;
  char *ipver,pkttoip[35],fromip[35],fromport[10];
  char s[INET6_ADDRSTRLEN],msg[2000],msg1[2000],**args,**fileval,*line,cbuff[100];
  unsigned char *sdata;
  fd_set master;    // master file descriptor list
  fd_set read_fds;  // temp file descriptor list for select()
  int fdmax;        // maximum file descriptor number
  int i,j,k,l,rv,nbytes,kbcount,arrcount,srvrcnt,edgecnt,costcnt,updfreq,retval,nbrdisable;
  int adjmatrix[6][6],updnos,pktfromsrv,isnbr,pkttosrv,pktrcvd = 0,timeoutcnt,iscrash;
  // structures
  struct addrdb addr_detl[10]; // hold addresses
  struct costdb cost_detl[10];
  struct tm * time_info;
  char timeString[9];
  time_t begin, end;
  //clock_t begin, end;
  double time_spent;
  double start=now();

  size_t bufsize = 0; // getline buffer allocation
  char getfile[200],*sdbuf = NULL,chostname[1023],ctemp[100];
  int write_sz,fs_block_sz,n,m,linecnt,pktinf;
  FILE *fr,*fs;

  struct hostent *host_ent;
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  //
  printf("***** U D P  S E R V E R *****\n");
  //
  iscrash = 0;
  srvrcnt = 0;
  timeoutcnt = 0;
  arrcount = 0;
  updfreq = 0;
  FD_ZERO(&master);   // clear master set
  FD_ZERO(&read_fds);
  FD_SET(fileno(stdin), &master); // for keyboard
  fdmax = fileno(stdin); // start with keyboard only
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  //begin = now();
  //
  // *** Main loop ***
  for(;;)
    {
    printf(">\n");

    read_fds = master;
    if (updfreq == 0) {
     if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
       {perror("select error\n");
        exit(4);
       }
     }
    else {
     retval = select(fdmax+1, &read_fds, NULL, NULL, &timeout);
     timeout.tv_sec = updfreq;
     timeout.tv_usec = 0;
     if (retval == -1)
       {perror("select error\n");
        exit(4);
       }
     else if (retval == 0 && updfreq != 0) // timeout
       {
       // send packets on timeout
       timeoutcnt += 1;
       // No packet received from nbr for 3 timeouts
      /* if (timeoutcnt >= 3) {
         timeoutcnt = 0;
         pktinf = 0;
         for (k=0; k<costcnt; k++) {
           if (cost_detl[k].pktrcvd == 0) {
             cost_detl[k].cost = MAX_VALUE; // set to infinity
             adjmatrix[cost_detl[k].srvrid][cost_detl[k].nbrid] = MAX_VALUE;
             pktinf = 1;
             }
           else if (cost_detl[k].pktrcvd != MAX_VALUE) // not at startup
             cost_detl[k].pktrcvd = 0;
           } // for k
         if (pktinf == 1) {
           BellmanFordEval(distances,cost_detl,srvrcnt,costcnt,adjmatrix);
            }
         } */ //timeoutcnt
       // pack values
       makepack(srvrcnt,edgecnt,costcnt,msg1,fromport,fromip,cost_detl,addr_detl);
       // send rt table packet to nbr servers
       for (j=0; j<=srvrcnt; j++) {
         isnbr = 0;
         for (k=0; k<costcnt; k++) {
           if (addr_detl[j].srvrid == cost_detl[k].nbrid && 
                  cost_detl[k].cost != MAX_VALUE && cost_detl[k].disabled == 0)
             {isnbr = 1;
              break;}
           }
         if (isnbr != 0) {
           si_other.sin_family = AF_INET;
           si_other.sin_port = htons(atoi(addr_detl[j].theport));
           if (inet_aton(addr_detl[j].ipaddr, &si_other.sin_addr)==0) {
              fprintf(stderr, "inet_aton() failed\n");
              exit(1);}
           if ((nbytes=sendto(sockfd, msg1, strlen(msg1), 0,
                     (struct sockaddr *)&si_other, sizeof si_other)) == -1) {
               printf("send to failed %s\n",inet_ntoa(si_other.sin_addr));
              }
           printf("sent to %s\n", inet_ntoa(si_other.sin_addr));
           } // is nbr
         } // for j
         start=now(); // reset timer
       } // retval == 0
     } // timeout.tv_sec == 0
    // run through looking for data to read
    for(i = 0; i <= fdmax; i++)
      {
      if (FD_ISSET(i, &read_fds)) // connection detected
        {// handle data
        if (!FD_ISSET(fileno(stdin), &read_fds)) // Not keyboard
          {// handle data
          if ((nbytes = recvfrom(i, msg, sizeof msg, 0,(struct sockaddr *)&their_addr
                        , &sin_size)) > 0)
            {// got some data from a server
            if (strlen(msg) != 0)
              {
            //printf("%s\n",msg);
              args = lsh_split_line(msg,1);
              if (strcmp(args[0],"RTBL") == 0)
                {
                updnos = atoi(args[1]);
                pktfromsrv = MAX_VALUE;
                for (k=0; k<arrcount; k++) {
                  if (strcmp(addr_detl[k].ipaddr,args[3]) == 0 && strcmp(addr_detl[k].theport,args[2]) == 0)
                    pktfromsrv = addr_detl[k].srvrid;
                  }
                if (pktfromsrv == MAX_VALUE) {
                  printf("%s:%s\n",args[0],"Invalid From Server IP!");
                  break;
                 }
                nbrdisable = 0;
                for (k=0; k<costcnt; k++) {
                  if (cost_detl[k].nbrid == pktfromsrv &&
                         cost_detl[k].disabled == 1)
                    {
                    nbrdisable = 1;
                    break;
                    }
                  }
                if (nbrdisable == 0)
                 {
                 printf("RECEIVED A MESSAGE FROM SERVER %d\n", pktfromsrv);
                 //
                 for (k=0; k<costcnt; k++) {
                  if (cost_detl[k].nbrid == pktfromsrv) {
                    if (cost_detl[k].pktrcvd == MAX_VALUE) {
                      cost_detl[k].pktrcvd = 1;
                      break;
                      }
                    else {
                     cost_detl[k].pktrcvd += 1;
                     break;}
                    }
                   }
                 adjmatrix[pktfromsrv][pktfromsrv] = 0; // cost to server itself
                //printf("args4 %s\n",args[4]);
                 fileval = lsh_split_line(args[4],2); // split by delim "|"
                 for (j=0; j<=50; j++) {
                   if (strcmp(fileval[j],"END") == 0)
                     break;
                   else if (strcmp(fileval[j],"IP") == 0)
                     strcpy(pkttoip, fileval[j+1]);
                   else if (strcmp(fileval[j],"CST") == 0)
                    {
                     pkttosrv = MAX_VALUE;
                     for (k=0; k<arrcount; k++) {
                       if (strcmp(addr_detl[k].ipaddr,pkttoip) == 0)
                         pkttosrv = addr_detl[k].srvrid;
                       }
                     if (pkttosrv == MAX_VALUE) {
                       printf("%s:%s\n",args[0],"Invalid To Server IP!");
                       break;
                       }
                     adjmatrix[pktfromsrv][atoi(fileval[j-1])] = atoi(fileval[j+1]);

                     if (atoi(fileval[j-1]) == cost_detl[0].srvrid) {
                       for (k=0; k<costcnt; k++) {
                         if (cost_detl[k].nbrid == pktfromsrv) {
                           cost_detl[k].cost = atoi(fileval[j+1]);
                           adjmatrix[atoi(fileval[j-1])][pktfromsrv] = atoi(fileval[j+1]);
                           break;
                          }
                        } // k
                       }
                     }
                   } // for j
                  /*** Apply Bellman Ford Logic ***/
                  BellmanFordEval(distances,cost_detl,srvrcnt,costcnt,adjmatrix);
                  /*** End Bellman Ford Logic ***/
                  pktrcvd += 1;
                 } // nbrdisable
                }
                // send packet on timeout here also
                //end = time(NULL);
                time_spent = now() - start;
                if (updfreq != 0 && time_spent >= updfreq)
                  {
               //printf("%f\n",time_spent);
                  timeoutcnt += 1;
                  // No packet received from nbr for 3 timeouts
              /* if (timeoutcnt >= 3) {
                   timeoutcnt = 0;
                   pktinf = 0;
                   for (k=0; k<costcnt; k++) {
                     if (cost_detl[k].pktrcvd == 0) {
                      cost_detl[k].cost = MAX_VALUE; // set to infinity
                      adjmatrix[cost_detl[k].srvrid][cost_detl[k].nbrid] = MAX_VALUE;
                      pktinf = 1;
                     }
                     else if (cost_detl[k].pktrcvd != MAX_VALUE) // not at startup
                       cost_detl[k].pktrcvd = 0;
                    } // for k
                  if (pktinf == 1) {
                      BellmanFordEval(distances,cost_detl,srvrcnt,costcnt,adjmatrix);
                     }
                   } */  //timeoutcnt
                  // pack values
                  makepack(srvrcnt,edgecnt,costcnt,msg1,fromport,fromip,cost_detl,addr_detl);
                  // send rt table packet to nbr servers
                  for (j=0; j<=srvrcnt; j++) {
                   isnbr = 0;
                   for (k=0; k<costcnt; k++) {
                    if (addr_detl[j].srvrid == cost_detl[k].nbrid && 
                           cost_detl[k].cost != MAX_VALUE && cost_detl[k].disabled == 0)
                     {isnbr = 1;
                      break;}
                    }
                   if (isnbr != 0) {
                     si_other.sin_family = AF_INET;
                     si_other.sin_port = htons(atoi(addr_detl[j].theport));
                     if (inet_aton(addr_detl[j].ipaddr, &si_other.sin_addr)==0) {
                       fprintf(stderr, "inet_aton() failed\n");
                       exit(1);}
                     if ((nbytes=sendto(sockfd, msg1, strlen(msg1), 0,
                        (struct sockaddr *)&si_other, sizeof si_other)) == -1) {
                       printf("send to failed %s\n",inet_ntoa(si_other.sin_addr));
                       }
                     printf("sent to %s\n", inet_ntoa(si_other.sin_addr));
                    } // is nbr
                   } // for j
                  start=now();
                  break;
                } // time_spent
              } // msg != 0
            } // rcv data
          } // kbd
        else
          { // ***** Handle keyboard
          line = NULL;
          if (!getline(&line, &bufsize, stdin))
            {
            if (ferror(stdin)) {
              perror("stdin");
              exit(1);
              }
            } //getline
            if (strlen(line) != 0) {
              kbcount = strlen(line);
              args = lsh_split_line(line,1);
              }
            if (kbcount <= 2) // empty enter key
              printf("Invalid Input!\n");
            else if (strcmp(args[0],"server") == 0 || strcmp(args[0],"SERVER") == 0)
              {
               if (iscrash == 1) {
                 printf("%s:%s\n",args[0],"Server Crashed\n");
                 break;}
               if (srvrcnt != 0) {
                 printf("%s:%s\n",args[0],"Topology file has already been loaded\n");
                 timeout.tv_sec = updfreq;
                 break;}
              if (strcmp(args[1],"-t") != 0)
                {printf("%s:%s\n",args[0],"Invalid topology Command");
                 continue;
                }
              if (strcmp(args[3],"-i") != 0)
                {printf("%s:%s\n",args[0],"Invalid routing update interval Command");
                 continue;
                }
              updfreq = atoi(args[4]);
              timeout.tv_sec = updfreq;
              timeout.tv_usec = 0;
              strcpy(getfile,args[2]);
              fs = fopen(getfile, "r");
              if(fs == NULL) {
                printf("File %s not found.\n", getfile);
                return(1);}
              // initialize R table
              for (j=0; j<=6; j++)
                for (k=0; k<=6; k++)
                  adjmatrix[j][k] = MAX_VALUE;
              costcnt = 0;
              arrcount = 0;
              srvrcnt = 0;
              edgecnt = 0;
              linecnt = 0;
              while ((fs_block_sz = getline(&sdbuf, &bufsize, fs)) != -1)
                {
                linecnt += 1;
                //printf("Retrieved line of length %d :\n", fs_block_sz);
                //printf("%s", sdbuf);
                //
                if (strlen(sdbuf) != 0) {
                   fileval = lsh_split_line(sdbuf,1);
                   }
                if (linecnt == 1)
                  srvrcnt = atoi(fileval[0]);
                else if (linecnt == 2)
                  edgecnt = atoi(fileval[0]);
                else if (linecnt <= srvrcnt+2)
                  {
                  addr_detl[arrcount].srvrid = atoi(fileval[0]);
                  strcpy(addr_detl[arrcount].ipaddr,fileval[1]);
                  strcpy(addr_detl[arrcount].theport,fileval[2]);
                  arrcount += 1;
                  }
                else // costs
                  {
                  cost_detl[costcnt].srvrid = atoi(fileval[0]);
                  cost_detl[costcnt].nbrid = atoi(fileval[1]);
                  if (strcmp(fileval[2],"inf") == 0 || strcmp(fileval[2],"INF") == 0)
                    {
                    adjmatrix[atoi(fileval[0])][atoi(fileval[1])] = MAX_VALUE;
                    cost_detl[costcnt].cost = MAX_VALUE;
                    cost_detl[costcnt].orig_cost = MAX_VALUE;
                    }
                  else
                    {
                    adjmatrix[atoi(fileval[0])][atoi(fileval[1])] = atoi(fileval[2]);
                    cost_detl[costcnt].cost = atoi(fileval[2]);
                    cost_detl[costcnt].orig_cost = atoi(fileval[2]);
                    }
                  cost_detl[costcnt].pktrcvd = MAX_VALUE; // initial value
                  cost_detl[costcnt].disabled = 0;
                  costcnt += 1;
                  }
                } // while
              free(sdbuf);
              fclose(fs);
              // store cost of server to itself
              adjmatrix[cost_detl[0].srvrid][cost_detl[0].srvrid] = 0;
              strcpy(fromip,"999.0.0.1");
              for (k=0; k<arrcount; k++) {
                if (addr_detl[k].srvrid == cost_detl[0].srvrid) {
                   strcpy(fromip, addr_detl[k].ipaddr);
                   strcpy(fromport, addr_detl[k].theport);
                  }
                }
              if (strcmp(fromip,"999.0.0.1") == 0) {
                printf("%s:%s\n",args[0],"Invalid From Server IP!");
                break;
                }
              //
              memset(&hints, 0, sizeof hints);
              hints.ai_family = AF_UNSPEC;
              hints.ai_socktype = SOCK_DGRAM;
              hints.ai_flags = AI_PASSIVE;
              //
              if((res=getaddrinfo(fromip, fromport, &hints, &servinfo))!=0)
                {
                fprintf(stderr,"getaddrinfo:%s\n", gai_strerror(res));
                break;
                }
              // loop through all the results and bind to the first we can
              for(p=servinfo; p!=NULL; p=p->ai_next)
                {
                if(p->ai_family==AF_INET)
                 {
                  struct sockaddr_in *ipv4=(struct sockaddr_in *)p->ai_addr;
                  addr = &(ipv4->sin_addr);
                  ipver = "IPv4";
                  inet_ntop(p->ai_family,addr,s,sizeof s);
                  if((sockfd=socket(p->ai_family,p->ai_socktype,p->ai_protocol))<0)
                    {printf("Searching Socket...\n");
                    continue;
                    }
                  else
                    {
                    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(int))==-1)
                      {printf("setsockopt...\n");
                       close(sockfd);
                       continue;
                      }
                    if((berr=bind(sockfd,p->ai_addr,p->ai_addrlen))<0)
                       {printf("Binding...\n");
                        close(sockfd);
                        continue;
                       }
                    } // sockfd
                 } //AF_INET
              } // for p=servinfo
              if(sockfd < 0)
                {printf("%s:%s\n",args[0],"Socket creation error!");
                 exit(0);
                }
              FD_SET(sockfd, &master);
              // keep track of the biggest file descriptor
              fdmax = sockfd;
              printf("Server Up at %s Port %s\n",fromip,fromport);
              printf("%s: SUCCESS\n",args[0]);
              break;
              }
            else if (strcmp(args[0],"update") == 0 || strcmp(args[0],"UPDATE") == 0)
                {
                if (iscrash == 1) {
                  printf("%s:%s\n",args[0],"Server Crashed\n");
                  break;}
                if (srvrcnt == 0) {
                  printf("%s:%s\n",args[0],"Topology file not loaded");
                  break;}
                if (cost_detl[0].srvrid != atoi(args[1])) {
                  printf("%s:%s\n",args[0],"Invalid Source ID");
                  break;}
                isnbr = 0;
                for (k=0; k<costcnt; k++) {
                  if (atoi(args[2]) == cost_detl[k].nbrid)
                    {isnbr = 1;
                     if (strcmp(args[3],"inf") == 0 || strcmp(args[3],"INF") == 0)
                       cost_detl[k].cost = MAX_VALUE;
                     else
                       cost_detl[k].cost = atoi(args[3]);
                     break;}
                  }
                if (isnbr == 0) {
                  printf("%s:%s\n",args[0],"Invalid Neighbor ID");
                  break;}
                if (strcmp(args[3],"inf") == 0 || strcmp(args[3],"INF") == 0)
                  adjmatrix[atoi(args[1])][atoi(args[2])] = MAX_VALUE;
                else
                  adjmatrix[atoi(args[1])][atoi(args[2])] = atoi(args[3]);
                /*** Apply Bellman Ford Logic ***/
                BellmanFordEval(distances,cost_detl,srvrcnt,costcnt,adjmatrix);
                /*** End Bellman Ford Logic ***/
                //
                // pack values
                makepack(srvrcnt,edgecnt,costcnt,msg1,fromport,fromip,cost_detl,addr_detl);
                // send rt table packet to nbr server
                for (j=0; j<=srvrcnt; j++) {
                  if (addr_detl[j].srvrid == atoi(args[2]) && 
                                  adjmatrix[atoi(args[1])][atoi(args[2])] != MAX_VALUE)
                                  // && cost_detl[k].disabled == 0)
                    {
                    si_other.sin_family = AF_INET;
                    si_other.sin_port = htons(atoi(addr_detl[j].theport));
                    if (inet_aton(addr_detl[j].ipaddr, &si_other.sin_addr)==0) {
                         fprintf(stderr, "inet_aton() failed\n");
                       exit(1);}
                    if ((nbytes=sendto(sockfd, msg1, strlen(msg1), 0,
                           (struct sockaddr *)&si_other, sizeof si_other)) == -1) {
                      printf("send to failed %s\n",inet_ntoa(si_other.sin_addr));
                      }
                    printf("sent %d bytes to %s\n", nbytes, inet_ntoa(si_other.sin_addr));
                    printf("%s: SUCCESS\n",args[0]);
                    } // to nbr
                  } // for j
                } // update
            else if (strcmp(args[0],"step") == 0 || strcmp(args[0],"STEP") == 0)
                {
                if (iscrash == 1) {
                  printf("%s:%s\n",args[0],"Server Crashed\n");
                  break;}
                if (srvrcnt == 0) {
                  printf("%s:%s\n",args[0],"Topology file not loaded");
                  break;}
                // pack values
                makepack(srvrcnt,edgecnt,costcnt,msg1,fromport,fromip,cost_detl,addr_detl);
                // send rt table packet to nbr servers
                for (j=0; j<=srvrcnt; j++) {
                  isnbr = 0;
                  for (k=0; k<costcnt; k++) {
                    if (addr_detl[j].srvrid == cost_detl[k].nbrid && 
                          cost_detl[k].cost != MAX_VALUE && cost_detl[k].disabled == 0)
                      {isnbr = 1;
                       break;}
                    }
                  if (isnbr != 0) {
                   si_other.sin_family = AF_INET;
                   si_other.sin_port = htons(atoi(addr_detl[j].theport));
                   if (inet_aton(addr_detl[j].ipaddr, &si_other.sin_addr)==0) {
                        fprintf(stderr, "inet_aton() failed\n");
                      exit(1);}
                   if ((nbytes=sendto(sockfd, msg1, strlen(msg1), 0,
                          (struct sockaddr *)&si_other, sizeof si_other)) == -1) {
                     printf("send to failed %s\n",inet_ntoa(si_other.sin_addr));
                     }
                   printf("sent %d bytes to %s\n", nbytes, inet_ntoa(si_other.sin_addr));
                   } // is nbr
                 } // for j
                printf("%s: SUCCESS\n",args[0]);
                } // step
            else if (strcmp(args[0],"packets") == 0 || strcmp(args[0],"PACKETS") == 0)
               {
               if (iscrash == 1) {
                 printf("%s:%s\n",args[0],"Server Crashed\n");
                 break;}
               if (srvrcnt == 0) {
                  printf("%s:%s\n",args[0],"Topology file not loaded");
                  break;}
               printf("Packets Received %d\n", pktrcvd);
               pktrcvd = 0;
               printf("%s: SUCCESS\n",args[0]);
               }
            else if (strcmp(args[0],"display") == 0 || strcmp(args[0],"DISPLAY") == 0)
               {
               if (iscrash == 1) {
                 printf("%s:%s\n",args[0],"Server Crashed\n");
                 break;}
               if (srvrcnt == 0) {
                 printf("%s:%s\n",args[0],"Topology file not loaded\n");
                 break;}
               printf("source-ID  destination-server-ID  next-hop-server-ID  cost-of-path\n");
               printf("=========  =====================  ==================  ============\n");
               for (j=1; j<=srvrcnt; j++)
                 {
                  printf("%8d ",cost_detl[0].srvrid);
                  printf("%10d ",j);
                  if ((j-cost_detl[0].srvrid) > 1)
                    {
                    isnbr = 0;
                    for (k=cost_detl[0].srvrid; k<j; k++) {
                     for (m=k+1; m<j; m++) {
                      if (adjmatrix[k][m] != MAX_VALUE && adjmatrix[k][m] != 0)
                        {
                         if (k > cost_detl[0].srvrid) {
                           printf("%25d ",k);
                           isnbr = 1;
                           break;
                           }
                         else
                          {
                           printf("%25d ",m);
                           isnbr = 1;
                           break;
                          }
                        }
                      } // m
                      if (isnbr == 1)
                        break;
                     } // k
                    if (isnbr == 0)
                      printf("%25s "," ");
                    }
                  else
                    {printf("%25s "," ");
                    }
                  if (adjmatrix[cost_detl[0].srvrid][j] == MAX_VALUE)
                    printf("%15s\n","INF");
                  else
                    printf("%15d\n",adjmatrix[cost_detl[0].srvrid][j]);
                 } 
               printf("%s: SUCCESS\n",args[0]);
               }
            else if (strcmp(args[0],"disable") == 0 || strcmp(args[0],"DISABLE") == 0)
               {
               if (iscrash == 1) {
                 printf("%s:%s\n",args[0],"Server Crashed\n");
                 break;}
               if (srvrcnt == 0) {
                 printf("%s:%s\n",args[0],"Topology file not loaded");
                 break;}
               isnbr = 0;
               for (k=0; k<costcnt; k++) {
                 if (atoi(args[1]) == cost_detl[k].nbrid)
                   {isnbr = 1;
                    cost_detl[k].cost = MAX_VALUE; 
                    adjmatrix[cost_detl[0].srvrid][cost_detl[k].nbrid] = MAX_VALUE;
                    adjmatrix[cost_detl[k].nbrid][cost_detl[0].srvrid] = MAX_VALUE;
                    cost_detl[k].disabled = 1; // set to stop sending
                    break;}
                 }
               if (isnbr == 0) {
                 printf("%s:%s\n",args[0],"Invalid Neighbor ID");
                 break;}
               printf("%s: SUCCESS\n",args[0]);
               }
            else if (strcmp(args[0],"crash") == 0 || strcmp(args[0],"CRASH") == 0)
               {
               if (srvrcnt == 0) {
                 printf("%s:%s\n",args[0],"Topology file not loaded");
                 break;}
               close(sockfd);
               FD_CLR(sockfd, &master); // remove from master set
               for (k=0; k<costcnt; k++) {
                    cost_detl[k].cost = MAX_VALUE; // set to INF to stop sending
                    adjmatrix[cost_detl[0].srvrid][cost_detl[k].nbrid] = MAX_VALUE;
                 }
               iscrash = 1;
               timeout.tv_sec = 0;
               printf("%s: SUCCESS\n",args[0]);
               }
            else if (strcmp(args[0],"quit") == 0 || strcmp(args[0],"QUIT") == 0)
               {
               close(sockfd);
               return 0;
               } // quit
             } // kbd
           } // END handle data from client
      } // END looping through file descriptors i
    } // END for(;;)
 return 0;
 } // END main


void BellmanFordEval(int distances[],struct costdb *cost_detl,
                     int srvrcnt,int costcnt,int adjmatrix[][6])
  {
  int node,sourcenode,destinationnode,j,k;

  for (node = 1; node <= srvrcnt; node++)
    {distances[node] = MAX_VALUE; //Initialize
       }
  for (sourcenode = 1; sourcenode <= srvrcnt; sourcenode++)
    {
    for (destinationnode = 1; destinationnode <= srvrcnt; destinationnode++)
      {
      if (sourcenode == destinationnode)
        {
        adjmatrix[sourcenode][destinationnode] = 0;
        continue;
          }
      if (adjmatrix[sourcenode][destinationnode] == 0)
        {
        adjmatrix[sourcenode][destinationnode] = MAX_VALUE;
        }
      }
    }
    //
    distances[cost_detl[0].srvrid] = 0; // source
    for (node = 1; node <= srvrcnt - 1; node++)
      {
      for (sourcenode = 1; sourcenode <= srvrcnt; sourcenode++)
        {
        for (destinationnode = 1; destinationnode <= srvrcnt; destinationnode++)
          {
          if (adjmatrix[sourcenode][destinationnode] != MAX_VALUE)
            {
            if (distances[destinationnode] > distances[sourcenode] 
                                             + adjmatrix[sourcenode][destinationnode])
              distances[destinationnode] = distances[sourcenode]
                                           + adjmatrix[sourcenode][destinationnode];
            }  // != maxvalue
          } // destinationnode
        } // sourcenode
      } // node
  // store computed new distances for this source
  for (j=1; j<=5; j++) {
    adjmatrix[cost_detl[0].srvrid][j] = distances[j];
    for (k=0; k<costcnt; k++) {
      if (cost_detl[k].nbrid == j)
        cost_detl[k].cost = distances[j];
        }
     }
  } // END BellmanFordEval

void *get_in_addr(struct sockaddr *sa)
 {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
 }

char **lsh_split_line(char *line, int delim)
 {
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
   }
  if (delim == 1)
    token = strtok(line, LSH_TOK_DELIM);
  else
    token = strtok(line, LSH_TOK_DELIM1);
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
    if (delim == 1)
      token = strtok(NULL, LSH_TOK_DELIM);
    else
      token = strtok(NULL, LSH_TOK_DELIM1);
  }
  tokens[position] = NULL;
  return tokens;
 }

void makepack(int srvrcnt,int edgecnt,int costcnt,char *msg1,
         char *fromport,char *fromip,struct costdb *cost_detl,struct addrdb *addr_detl)
  {
  int updnos,j,k,isnbr;
  char cbuff[100];

  updnos = 2+(edgecnt*4);
  strcpy(msg1,"RTBL ");
  sprintf(cbuff,"%1d",updnos);
  strcat(msg1,cbuff);
  strcat(msg1," ");
  strcat(msg1,fromport);
  strcat(msg1," ");
  strcat(msg1,fromip);
  strcat(msg1," ");
  for (j=0; j<srvrcnt; j++) {
    isnbr = 0;
    for (k=0; k<costcnt; k++) {
      if (addr_detl[j].srvrid == cost_detl[k].nbrid)
        {isnbr = 1;
         break;}
        }
    if (isnbr != 0)
      {
      strcat(msg1,"IP|");
      strcat(msg1,addr_detl[j].ipaddr);
      strcat(msg1,"|PORT|");
      strcat(msg1,addr_detl[j].theport);
      strcat(msg1,"|ID|");
      sprintf(cbuff,"%1d",addr_detl[j].srvrid);
      strcat(msg1,cbuff);
      strcat(msg1,"|CST|");
      sprintf(cbuff,"%1d",cost_detl[k].cost);
      strcat(msg1, cbuff);
      strcat(msg1, "|");
      } // is nbr
    } // for j
    strcat(msg1,"END|99\0");
  } //makepack

double now(void)
{
   struct timeval tv;
   double retval=0;
   gettimeofday(&tv, NULL);
   retval=tv.tv_usec;
   //retval+= (double)tv.tv_usec / 1000000.;
   return retval;
}
