/**
 * @file simple_message_client.c
 * VCS TCP/IP Client
 *
 * @author Andrea Maierhofer
 * @author Thomas Schmid
 * @date 2015/11/30
 *
 * @version $Revision: 45 $
 *
 * Last Modified: $Author: Thomas Schmid $
 */

 /*
 * -------------------------------------------------------------- includes --
 */

#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <simple_message_client_commandline_handling.h>

/*
 * --------------------------------------------------------------- defines --
 */

#define MAX_SIZE 256

/*
 * --------------------------------------------------------------- globals --
 */

/* Globals are bad, but in this case it's better than passing verbose/prg to every function to get the logging-output ;) */
int verbose = FALSE;
const char *prg;

/*
 * ------------------------------------------------------------- functions --
 */

static void usage (FILE *out, const char *command, int exitcode);
void logging(char *message);
void printerr(char *message);
void *get_in_addr(struct sockaddr *sa);
int sendMessage(int socketfd, const char* user, const char* message, const char* image);
int getResponse(int socketfd);
int getValue(char* data, const char *key, char *value);

/**
 * \brief This is the main entry point for any C program.
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * \return      if we had an error somewhere
 * \retval      EXIT_FAILURE if we had at least one error
 * \retval      EXIT_SUCCESS if we had no error
 *
 */
int main(int argc, const char* argv[]) {
  const char* server  = NULL;
    const char* port    = NULL;
    const char* user    = NULL;
    const char* message = NULL;
    const char* image   = NULL;

  int socketfd;
  struct addrinfo hints, *servinfo, *p;
  int result;

  smc_parsecommandline(argc, argv, &usage, &server, &port, &user, &message, &image, &verbose);
  prg = argv[0]; /* get the program name for logging */

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  result = getaddrinfo(server, port, &hints, &servinfo);
  if (result != 0) {
    fprintf(stderr, "%s: getaddrinfo: %s\n", prg, gai_strerror(result));
    return EXIT_FAILURE;
  }

  /* loop through all the results and connect to the first we can */
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      logging("socket");
      continue;
    }
    if (connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(socketfd);
      logging("connect");
      continue;
    }
    break;
  }

  if (p == NULL) {
    /*
     * ### FB_CF: Ressourcenleak (freeaddrinfo fehlt) [-2]
     */
    fprintf(stderr, "%s: failed to connect\n",prg);
    return EXIT_FAILURE;
  }

  freeaddrinfo(servinfo); /* all done with this structure */

  /* Send the message */
  result = sendMessage(socketfd, user, message, image);

  /* If something went wrong --> exit */
  if (result == FALSE) {
    close(socketfd);
      printerr("Error in sending the message");
    return EXIT_FAILURE;
    }

  /* Handle the response */
  result = getResponse(socketfd);

  /* If something went wrong --> exit */
  if (result == FALSE) {
    close(socketfd);
      printerr("Error in receiving the response");
     /*
      * ### FB_CF: Fehlerhafter exit code beim Client (nicht Status des Servers) [-2]
      */
    return EXIT_FAILURE;
    }

  /* We are done with the socket */
  close(socketfd);


  return EXIT_SUCCESS;
}

/**
 * \brief a function pointer to a function  that  is  called  upon  a
 *        failure  in  command line parsing by smc_parsecommandline()
 *        The type of  this  function pointer is: typedef void (* smc_usagefunc_t) (FILE *, const char *, int);
 *
 * \param out      - specifies the output (STDOUT/STDIN - depends on the -h parameter or in case of an error)
 * \param command  - a string containing the name of the executable  (i.e., the contents of argv[0]).
 * \param exitcode - the exit code to be used in the call to exit(exitcode) for terminating the program.
 */
static void usage(FILE *out, const char *prg, int exitcode) {
  /* Error handling of fprintf isn't necessary because of the exit-function at the end */
  fprintf(out,"usage: %s options\n",prg);
    fprintf(out,"options:\n");
    fprintf(out,"\t-s, --server <server>   full qualified domain name or IP address of the server\n");
    fprintf(out,"\t-p, --port <port>       well-known port of the server [0..65535]\n");
    fprintf(out,"\t-u, --user <name>       name of the posting user\n");
    fprintf(out,"\t-i, --image <URL>       URL pointing to an image of the posting user\n");
    fprintf(out,"\t-m, --message <message> message to be added to the bulletin board\n");
    fprintf(out,"\t-v, --verbose           verbose output (for debugging purpose)\n");
    fprintf(out,"\t-h, --help\n");

    exit(exitcode);
}

/**
 * \brief Logging function
 *
 * \param message - The message that should be printed out
 */
void logging(char *message) {
  int result = 0;
  if(verbose == TRUE) {
    result = fprintf(stdout,"%s: %s\n",prg , message);
    /* fptintf: If an output error is encountered, a negative value is returned. */
    if(result < 0) {
      /* Error on printing on stdout - Try to write this on stderr
       * - if this also fails it does not make sense to exit the program. It's just the logging function. So there is no error handling.
       */
      fprintf(stderr,"Cannot print on stdout\n");
    }
  }
}

/**
 * \brief Prints errors
 *
 * \param message - The error message
 */
void printerr(char *message) {
  /* Try to write the message on stderr
   * - if this fails it does not make really sense to exit the program because this logic is handled in other ways.
   *   So there is no error handling for fprintf.
   */
  fprintf(stderr,"%s: %s\n", prg, message);
}

/**
 * \brief get sockaddr, IPv4 or IPv6:
 *        Thanks to Beej's ;)
 *
 * \param sockaddr - The socket address that will be converted to an IPv4 or IPv6 address (input & output parameter)
 */
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * \brief Sends a message to the server
 *
 * \param socketfd the socket file descriptor
 * \param user     the user
 * \param message  the messgae
 * \param image    the URL of the image
 *
 * \return      if we had an error somewhere
 */
int sendMessage(int socketfd, const char* user, const char* message, const char* image) {
  /* An alternative approach would be to use the send-syscall (and the sendall function in Beej's Guide).
   * But in the verbose output of the originial simple_message_client it seems to be handled this way.
   * --> Trying to flush file pointer associated with socket ...
   * And there should be no difference because Linux handles sockets like files
   */
    FILE *openClientSocket;
  char verboseString[MAX_SIZE];

  logging("Trying to open file descriptor writing");
  openClientSocket = fdopen(socketfd,"w");

    if (openClientSocket == NULL) {
        printerr("Failed to open socket for writing");
        return FALSE;
    }

  /* After every field there is a newline (0x0a) as a field seperator! */

  /* Just 4 logging (verbose) */
  if(sprintf(verboseString, "Sent request user=\"%s\"", user) < 0) {
    printerr("Failed to write verboseString");
  }

  /* Print the user with the user indicator */
  logging("Trying to send the user field ...");
    if (fprintf(openClientSocket,"user=%s\n",user) < 0) {
        /* fptintf: If an output error is encountered, a negative value is returned. */
        printerr("Failed to write the user to the stream");
        /*
         * ### FB_CF: Resourceleak (z.B., fehlendes free() oder close()) [-2]
         */
        return FALSE;
    }

  /* Only print the image, if there is an image ;) */
    if (image != NULL) {
    logging("Trying to send the image field ...");

    /* Just 4 logging (verbose) */
    if(sprintf(verboseString, "%s, img=\"%s\"", verboseString, image) < 0) {
      printerr("Failed to write verboseString");
    }

    /* Print the image with the image indicator */
        if (fprintf(openClientSocket,"img=%s\n",image) < 0) {
            printerr("Failed to write the image url to the stream");
            /*
             * ### FB_CF: Resourceleak (z.B., fehlendes free() oder close())
             */
            return FALSE;
        }
    }

  logging("Trying to send the message field ...");
  /* Just 4 logging (verbose) */
  if(sprintf(verboseString, "%s, message=\"%s\"", verboseString, message) < 0) {
    printerr("Failed to write verboseString");
  }
  /* Just print the message to the stream without a field indicator */
    if (fprintf(openClientSocket,"%s\n",message) < 0)     {
        printerr("Failed to write the message to the stream");
        /*
         * ### FB_CF: Resourceleak (z.B., fehlendes free() oder close())
         */
        return FALSE;
    }

  logging("Trying to flush file pointer associated with socket ...");
    if (fflush(openClientSocket)!=0) {
        printerr("Failed to flush the stream after writing");
        /*
         * ### FB_CF: Resourceleak (z.B., fehlendes free() oder close())
         */
        return FALSE;
    }

  logging("Trying to shutdown socket associated with file pointer ...");
  /* Shutdown the write direction. The server get's an EOF */
    if (shutdown(socketfd, SHUT_WR) != 0) {
        printerr("Failed to close writing direction of the stream");
        /*
         * ### FB_CF: Resourceleak (z.B., fehlendes free() oder close())
         */
        return FALSE;
    }

  /* Don't close the file descriptor here because the socket cannot be opened again in getResponse. It seems that the socket gets also closed when the file descriptor is closed. */

  logging(verboseString);

    return TRUE;
}

/**
 * \brief Handles the response
 *
 * \param socketfd the socket file descriptor
 *
 * \return      if we had an error somewhere (TRUE/FALSE)
 */
int getResponse(int socketfd) {
  FILE *openClientSocket;
  FILE *fileToWrite = NULL;
  char buffer[MAX_SIZE];
  char value[MAX_SIZE];
  char verboseString[MAX_SIZE];
  char *endptr;
  int mode = 0;       /* 0=Status lesen, 1=filename lesen, 2=length lesen, 3=data lesen */
  int status = -1;
  int filelen = 0;
  int received = 0;
  int max_buf_size = 0;
  int bytesread = 0;
  int i_i = 0;

  logging("Trying to open file descriptor reading");
  openClientSocket = fdopen(socketfd,"r");
  if (openClientSocket == NULL) {
        printerr("Failed to open socket for reading");
        return FALSE;
    }
/*
 * ### FB_CF: Eine Zeile kann aber auch länger sein [-2]
 */
  while(fgets(buffer,MAX_SIZE,openClientSocket) != NULL) {
    if(mode == 0) {
      /* First read the status */
      if(getValue(buffer,"status=",value) == 0) {
        getValue(buffer,"status=",value);
        status = strtol(value, &endptr, 10);
        if (value == endptr){
          printerr("Error converting status to integer");
        /*
         * ### FB_CF: Resourceleak (openClientSocket) [-2]
         */
          return FALSE;
        }

        if(status == 0) {
          /* Just 4 logging */
          if(sprintf(verboseString, "Status okay: %d",status) < 0) {
            printerr("Failed to write verboseString");
          }
          logging(verboseString);

          mode = 1;
        } else {
          /* Just 4 logging */
          if(sprintf(verboseString, "Status not okay: %d",status) < 0) {
            printerr("Failed to write verboseString");
          }
          printerr(verboseString);
        /*
         * ### FB_CF: Resourceleak (openClientSocket)
         */
          return FALSE;
        }
      }
    }

    if(mode == 1) {
      /* Read the filename */
      if(getValue(buffer,"file=",value) == 0) {
        fileToWrite = fopen(value,"w");
        if (fileToWrite==NULL) {
          printerr("Unable to open file");
        /*
         * ### FB_CF: Resourceleak (openClientSocket)
         */
          return FALSE;
        }

        mode = 2;
      }
    }

    if(mode == 2) {
      /* Read the length */
      if(getValue(buffer,"len=",value) == 0) {
        filelen = strtol(value, &endptr, 10);
        if (value == endptr){
          printerr("Error converting len to integer");
          /*
           * ### FB_CF: Ressourceleak (fileToWrite) [-2]
           */
          /*
           * ### FB_CF: Resourceleak (openClientSocket)
           */
          return FALSE;
        }

        mode = 3;
        received = 0;
      }
    }

    if(mode == 3) {
      /* Read the data */
      if(fileToWrite == NULL) {
        printerr("Something went wrong with the modes... No file opened!");
          /*
           * ### FB_CF: Resourceleak (openClientSocket)
           */
        return FALSE;
      }

      do {
        i_i++;

        /* If the file data is still bigger than the MAX_SIZE then the buffer is set to MAX_Size */
        if(MAX_SIZE < (filelen - received)) {
          max_buf_size = MAX_SIZE;
        } else {
          max_buf_size = filelen - received;
        }

        /* Just 4 logging */
        if(sprintf(verboseString, "Read chunk: %d @%d bytes",i_i,max_buf_size) < 0) {
          printerr("Failed to write verboseString");
        }
        logging(verboseString);

        /* read the data from the socket */
        bytesread = fread(buffer,sizeof(char),max_buf_size,openClientSocket);
        received += max_buf_size;

        /* if the written bytes are not equivalent to the read bytes then a write error occured */
        if((int) fwrite(buffer,sizeof(char),bytesread,fileToWrite) != bytesread) {
          (void)fclose(fileToWrite); /* close file and ignore errors */
          printerr("Error while writing to file");
          /*
           * ### FB_CF: Resourceleak (openClientSocket)
           */
          return FALSE;
        }

        /* if the whole file is received then we can close the actual written file and try to find out if there is another file-record on the tcp-stream */
        if(received == filelen) {
          if(fclose(fileToWrite)!=0) {
            printerr("Error while closing to file");
          /*
           * ### FB_CF: Resourceleak (openClientSocket)
           */
            return FALSE;
          }

          logging("File read done - search for next record");
          mode = 1; /* Next record begins with filename */
          break;
        }

        /* If there was an error on reading from stream break this */
        if(ferror(openClientSocket) != 0) {
          fclose(fileToWrite);
          printerr("Error while reading from stream");
          /*
           * ### FB_CF: Resourceleak (openClientSocket)
           */
          return FALSE;
        }
      } while(1);
    }
  }
  logging("EOF received ...");

  logging("Trying to close file pointer associated with socket ...");
  /* Close the file pointer associated with socket */
    if (fclose(openClientSocket) != 0) {
        printerr("Failed to close file pointer associated with socket");
        return FALSE;
    }

  logging("Closed read part of socket");

  return TRUE;
}

/**
 * \brief Searches for key-fields and gives the value back if there was something found
 *
 * \param data  the acutal data of the tcp stream
 * \param key   the key which is searched for
 * \param value the value of the key (return value!)
 *
 * \return      if we had an error somewhere (0,-1)
 */
int getValue(char *data, const char *key, char *value) {
  char *newlinepos;
  char *pos;
  char verboseString[MAX_SIZE];

    if (data != NULL)    {
    /* Search for the key */
    pos = strstr(data, key);

    if (pos != NULL){
      /* set the pointer after the '=' character */
      pos = pos + strlen(key);
      newlinepos = strchr(pos, '\n');

      if (newlinepos == NULL) {
        if(sprintf(verboseString, "getValue() - New line character not found for key: %s", key) < 0) {
          printerr("Failed to write verboseString");
        }
        printerr(verboseString);
        return -1;
      }

      /* copy the found value into the given value variable */

      memset(value, 0, strlen(value));
      strncpy(value, pos, newlinepos - pos);

      /* position the pointer after the value, so we can search the next item */
      data = data + strlen(key) + strlen(value) + 1;

      if(sprintf(verboseString, "getValue() - Key: %s, Value: %s", key, value) < 0) {
        printerr("Failed to write verboseString");
      }
      logging(verboseString);
      return 0;
    }
  }

  /* if there was an error, or no key field found -1 is returned */
  return -1;
}

/*
 * =================================================================== eof ==
 */
