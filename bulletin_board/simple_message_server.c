/**
*@file simple_message_server.c
*Verteilte Systeme server file.
*Bsp4
*
*@author: Abbas AlHaj
*@author: Stefan Hasel
*
*@date 30.11.2013
*
*@version 1.0
*
*/

/*
*
*-----------------------------------------------------------includes--
*/
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
/*#include <seqev.h>*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>
#include "simple_message_client_commandline_handling.h"
#include <sys/wait.h>
#include <signal.h>
#include <getopt.h>
/*
*
*------------------------------------------------------------defines--
*/
#define C_ERROR -99
#define MAX_BUF 1024
#define LOGICPATH "/usr/local/bin/simple_message_server_logic" /* business_logic Path */
#define LISTENQ 24 		/*	Backlog for Listen	*/
/*
*
*--------------------------------------------------------------enums--
*/

/*
*
*-----------------------------------------------------------typedefs--
*/

/*
*
*------------------------------------------------------------globals--
*/
const char *progname = "<no name defined>";

/*
*
*----------------------------------------------------------functions--
*/
void sig_chld(int s);
/*void parse_commandline(int argc, const char * const * argv , smc_usagefunc_t usagefunc);*/
void usage(FILE* stream, const char *  cmd, int exitcode);
void smc_parsecommandline_server(int argc,const char * const argv[],smc_usagefunc_t usagefunc,const char **port,int *verbose);


/**
*\brief server, connects to clients
*
*This is the main entry point
*
*\param argc the number of arguments
*\param argv the arguments itselves
*
*\return EXIT_FAILURE error in program
	progname = argv[0];
*\return EXIT_SUCCESS no error occured
*
*/
int main (int argc, const char* argv[]){
	/* parameter check  */	
	/*server*/
    struct addrinfo hints, *servinfo;  	/*		*/
    struct sockaddr_storage address;	/*		*/
	socklen_t addlen;					/*		*/
	int yes , childpid;			/*		*/
	int sockfd , new_fd;	/*	Listen on socket sockdf, new connection on new_fd */
	const char* port;		/*	Port Number	*/
	int verbose;			
	/*struct sigaction sa;	*/
	int rv;
	progname = argv[0];
	/*argc1 = argc;*/
	yes = 1;
	/*	Check parameters */
	smc_parsecommandline_server(argc , argv , usage ,&port , &verbose );

	memset(&hints, 0, sizeof hints); 		
 	hints.ai_family = AF_UNSPEC;    		/*	Use both IPv4 & IPv6	*/
	hints.ai_socktype = SOCK_STREAM;		/*	TCP Socket	*/
	hints.ai_flags = AI_PASSIVE;			/*	Fill my IP	*/

    /*
     * ### FB_CF:  Hier getaddrinfo() zu verwenden bringt's nicht so
     *             wirklich. - Außerdem wissen Sie dadurch nicht, ob Ihr Server
     *             nun auf einer IPV4 oder einer IPV6 Adresse lauscht ...
     */

	/**/
	if((rv=getaddrinfo(NULL, port , &hints, &servinfo)) != 0){
		fprintf(stderr, "%s: error getaddrinfo: %s\n", progname, gai_strerror(rv)); 		
		exit(EXIT_FAILURE); 
	}	
	/**/
	if((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 
						servinfo->ai_protocol)) == -1){
		fprintf(stderr, "%s: error socket: %s\n", progname, strerror(errno)); 		
		/*
		 * ### FB_CF: Ressourceleak (freeaddrinfo() fehlt) [-2]
		 */
		exit(EXIT_FAILURE); 
	}
	/**/
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
		fprintf(stderr, "%s: error setsockopt %s\n", progname, strerror(errno)); 
		close(sockfd);
		/*
		 * ### FB_CF: Ressourceleak (freeaddrinfo() fehlt)
		 */
		exit(EXIT_FAILURE); 
	}
	/**/
	if(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1){
		fprintf(stderr, "%s: error bind %s\n", progname, strerror(errno)); 
		close(sockfd);
		/*
		 * ### FB_CF: Ressourceleak (freeaddrinfo() fehlt) [-2]
		 */
		exit(EXIT_FAILURE); 
	}
	
	freeaddrinfo(servinfo);

	/*	Listen on Socket  */
	if (listen(sockfd , LISTENQ) == -1 ){
		fprintf(stderr, "%s: error too many connections %s\n", progname, strerror(errno));
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	
	/*sa.sa_handler = sig_chld; 
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;*/
	/**/
	/*if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		fprintf(stderr, "%s: error signation %s\n", progname, strerror(errno));
		close(sockfd);
		exit(EXIT_FAILURE);
	}*/

	signal (SIGCHLD , sig_chld);
	/* Waiting till success accept */
	while(1){
		addlen = sizeof (address);
		new_fd = accept(sockfd, (struct sockaddr *) &address , &addlen ); 	/* Accept new Connection*/
		
		if ( new_fd ==  -1 ){	
				if (errno == EINTR) { /* The system call was interrupted by a signal that was caught before a valid connection arrived	*/
				continue;			/* in case of EINTR we do not need to end */
				}else{
				fprintf(stderr, "%s: error accept %s\n", progname, strerror(errno));
				close(sockfd);				
				exit(EXIT_FAILURE);
				}
			}
		
		childpid = fork();		/* Child Process */
		
		switch (childpid){
		case -1: 
			close(new_fd);
			fprintf(stderr, "%s: error fork %s\n", progname, strerror(errno));
			/*
			 * ### FB_CF: Ressourceleak (close(sockfd) fehlt) [-2]
			 */
			exit(EXIT_FAILURE);		
			
		case 0:{
			close(sockfd); /* Close listing socket */
			if (dup2(new_fd,0) == -1){ /* Change to stdin */
				close(new_fd);
				exit(EXIT_FAILURE);
			}
			if (dup2(new_fd,1) == -1){ /* Change to stdout */
				close(new_fd);
				exit(EXIT_FAILURE);
			}			
			close(new_fd); /* done with this client */
			/*	Start simple_message_server_logic	*/
			if (execlp(LOGICPATH , "simple_message_server_logic" , NULL )==-1){			
			fprintf(stderr, "execlp() failed: %s\n", strerror(errno));						
		    exit(EXIT_FAILURE);
			}			 
			break;
			}		
		}
	close(new_fd); /* parent closes connected socket */  
	}	/*End While*/
	
	/*
	 * ### FB_CF: Dead Code
	 */
	
	/*	close(sockfd);	*/
	exit(EXIT_SUCCESS);
}
/*	Handler for Childprocesses	*/
void sig_chld(int signo)
{
	signo = signo+1; /* No meaning, Avoids the annoying warning :)*/
    while(waitpid(-1, NULL, WNOHANG) > 0);
}
/*	Print Usage of the program*/
void usage(FILE* stream, const char *  cmd, int exitcode){
	fprintf(stream, "usage:%s <options>\n", cmd);
        fprintf(stream, "\t-p,\t--port <port>\t\twell-known port for the server [0..65535]\n");
        fprintf(stream, "\t-h,\t--help\n");
	exit(exitcode);
}
/* Modified version of the smc_parsecommandline */
void smc_parsecommandline_server(
    int argc,
    const char * const argv[],
    smc_usagefunc_t usagefunc,
    const char **port,
    int *verbose
    )
{
    int c;
    struct option    long_options[] =
    {
        {"port", 1, NULL, 'p'},
        {"verbose", 0, NULL, 'v'},
        {"help", 0, NULL, 'h'},
        {0, 0, 0, 0}
    };

    *port = NULL;
    *verbose = FALSE;
	
    while (
        (c = getopt_long(
             argc,
             (char ** const) argv,
             "p:hv",
             long_options,
             NULL
             )
            ) != -1
        )
    {
        switch (c)
        {
            case 'p':
                *port = optarg;
                break;

            case 'v':
                *verbose = TRUE;
                break;

            case 'h':
	      usagefunc(stdout, argv[0], EXIT_SUCCESS);
                break;

            case '?':
            default:
                usagefunc(stderr, argv[0], EXIT_FAILURE);
                break;
        }
    }

    if ((optind != argc) ||(port == NULL))
    {
        usagefunc(stderr, argv[0], EXIT_FAILURE);
    }
}


