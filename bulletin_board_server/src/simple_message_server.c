/**
 * @file simple_message_server.c
 * Verteilte Systeme
 * TCP/IP Programmieruebung
 *
 * TCP/IP Server
 *
 * @author Andrea Maierhofer    1410258024  <andrea.maierhofer@technikum-wien.at>
 * @author Thomas Schmid        1410258013  <thomas.schmid@technikum-wien.at>
 * @date 2015/12/07
 *
 *
 */

/* === includes ============================================================= */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> /* getopt, fork, close, execvp, STDIN_FILENO, STDOUT_FILENO*/
#include <assert.h> /* assert */
#include <sys/socket.h> /* socket, bind, listen, accept */
#include <sys/wait.h> /* waitpid */
#include <netdb.h> /* INADDR_ANY, kinds of sockaddr */
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <stdarg.h>

/* === defines ============================================================== */

#define INPUT_NUM_BASE 10 /* decimal format base for strtol */

#define LOWER_PORT_RANGE 0
#define UPPER_PORT_RANGE 65536
#define LOCALHOST "127.0.0.1"
#define CONN_COUNT 10
#define LOGIC_NAME "simple_message_server_logic"
#define LOGIC_PATH "/usr/local/bin/simple_message_server_logic"
/* === static ============================================================== */
/** Current program arguments. */
static const char* sprogram_arg0 = NULL;

/* === prototypes =========================================================== */

char* param_check(int argc, char **argv);
void killZombies(int signal);
int regSigHandler(void);
int handleConnections(int sockfd);
static void print_error(const char* message, ...);
static void print_usage(FILE* file, const char* message, int exit_code);
static void cleanup(bool exit);
static int setUpConnection(const char* portnumber);


/* === globals ============================================================== */

/* === functions ============================================================ */

/**
 * \brief the main method for the server
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * \return success or failure
 * \retval 0 if the function call was successful, 1 otherwise
 *
 */
int main(int argc, char** argv) {
    /* server port with type short int which is needed by the htons function */
    char* server_port = 0;
    int sockfd = 0;


    /* calling the getopt function to get portnum*/
    server_port = param_check(argc, argv);
    if(server_port == NULL){
        return EXIT_FAILURE;
    }

    if ((sockfd = setUpConnection(server_port)) < 0) {
        return EXIT_FAILURE;
    }
    if (regSigHandler() < 0) {
        return EXIT_FAILURE;
    }
    if (handleConnections(sockfd) < 0) {
        return EXIT_FAILURE;
    }

    /*
     * ### FB_CF: Dead Code
     */

    return EXIT_SUCCESS;
}


/**
 *
 * \brief Prints error message to stderr.
 *
 * A new line is printed after the message text automatically.
 * Printout can be formatted like printf.
 *
 * \param message output on stderr.
 *
 * \return void
 */
static void print_error(const char* message, ...)
{
    va_list args;

    /* do not handle return value of fprintf, because it makes no sense here */
    (void) fprintf(stderr, "%s: ", sprogram_arg0);
    va_start(args, message);
    (void) vfprintf(stderr, message, args);
    va_end(args);
    (void) fprintf(stderr, "\n");
}

/**
 *
 * \brief Print the usage and exits.
 *
 * Print the usage and exits with the given exit code.
 *
 * \param stream where to put the usage output.
 * \param command name of this executable.
 * \param exit_code to be set on exit.
 *
 * \return void
 */
static void print_usage(FILE* stream, const char* command, int exit_code)
{
    int written;

    written = fprintf(stream, "usage: %s options\n", command);
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = fprintf(stream,
      "  -s, --server <server>   fully qualified domain name or IP address of the server\n"
      "  -p, --port <port>       well-known port of the server [0..65535]\n"
      "  -u, --user <name>       name of the posting user\n"
      "  -i, --image <URL>       URL pointing to an image of the posting user\n"
      "  -m, --message <message> message to be added to the bulletin board\n"
      "  -v, --verbose           verbose output\n"
      "  -h, --help\n");
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    fflush(stream);
    fflush(stderr);

    exit(exit_code);
}
/**
 * \brief Cleanup the program.
 *
 * \param exit_program when set to TRUE exit program immediately with EXIT_FAILURE.
 *
 * \return void
 */
static void cleanup(bool exit_program)
{

    (void) fflush(stderr); /* do not handle erros here */
    (void) fflush(stdout);

    if (exit_program)
    {
        exit(EXIT_FAILURE);
    }

}
/**
 * \brief the method for checking the passed parameters
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * \return the portnumber
 * \retval -1 if wrong arguments are passed, otherwise the portnumber which is given with the -p argument
 *
 */
char* param_check(int argc, char **argv) {
    char* end_ptr;
    long int port_nr = 0;
    char* portstr = NULL;
    int c;
    int berror = 0;

    opterr = 0;

    if(argc<3){
        print_usage(stderr,sprogram_arg0,EXIT_FAILURE);
    }

    while ((c = getopt(argc, argv, "p:h::")) != EOF) {
        switch (c) {
        case 'p':
            port_nr = strtol(optarg, &end_ptr, INPUT_NUM_BASE);
               if ((errno == ERANGE && (port_nr == LONG_MAX || port_nr == LONG_MIN))
                   || (errno != 0 && port_nr == 0))
               {
                   print_error("Can not convert port number (%s).", strerror(errno));
                   return EXIT_FAILURE;
               }

               if (end_ptr == optarg)
               {
                   print_error("No digits were found.");
                   return EXIT_FAILURE;
               }

               if (port_nr < LOWER_PORT_RANGE && port_nr > UPPER_PORT_RANGE)
               {
                   print_error("Port number out of range.");
                   print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
               }
            portstr = optarg;
            break;
        case 'h':
            /* when the usage message is requested, program will exit afterwards */
            print_usage(stdout,sprogram_arg0,EXIT_SUCCESS);
            break;
        case '?':
            /* occurs, when other arguments than -p or -h are passed */
            print_usage(stderr,sprogram_arg0,EXIT_FAILURE);
            break;
        default:
            assert(0);
        }
    }
    /* if user somehow messed other things up :) */
    if (argc - optind >= 1) {
        berror = 1;
    }

    if (berror == 1) {
        print_usage(stderr,sprogram_arg0,EXIT_FAILURE);
        return NULL;
    }
    return portstr;
}

int regSigHandler(void) {
    struct sigaction sig;
    sig.sa_handler = killZombies;
    /*
     * excludes all signals from the signal handler mask
     * returns always 0, so return value can be ignored.
     */
    (void) sigemptyset(&sig.sa_mask);
    sig.sa_flags = SA_RESTART;

    return sigaction(SIGCHLD, &sig, NULL);
}

/**
 * \brief knifes all the zombies
 */
void killZombies(int signal) {
    /*
     * waitpid waits for information about child-processes
     * (pid_t)(-1) means, status is requested for any child process
     * argument 2 (0) can store termination status of the terminated process,
     * but we don't care about
     * WNOHANG makes this function non-blocking
     */
    (void) signal;
    while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) {
    }
}


static int setUpConnection(const char* portnumber) {
    int sockfd = 0;
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    /* resetting the structs */
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC; /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE; /* For my IP */


    /*
     * ### FB_CF:  Also hier getaddrinfo() zu verwenden bringt's nicht so
     *             wirklich. - Außerdem wissen Sie dadurch nicht, ob Ihr Server
     *             nun auf einer IPV4 oder einer IPV6 Adresse lauscht ...
     */

    /*
     * getting a linked list of available adresses.
     * stores the first one in &result
     */
    if (getaddrinfo(NULL, portnumber, &hints, &result) != 0) {

        print_error("error getaddrinfo\n");
        return -1;
    }

    /*
     * trys to bind to the available adresses.
     * breaks, if binding to one was successful.
     */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
    }

    /* no address succeeded */
    if (rp == NULL) {
        print_error("error bin\n");
        return -1;
    }

    freeaddrinfo(result);

    if (listen(sockfd, CONN_COUNT) < 0) {

        print_error("error listen\n");
        (void) close(sockfd);
        return -1;
    }

    return sockfd;
}

int handleConnections(int sockfd) {
    int pid = 0;
    struct sockaddr_storage addr_inf;
    socklen_t socklen = sizeof(addr_inf);
    int connfd = 0;

    while (1) {
        if ((connfd = accept(sockfd, (struct sockaddr*) &addr_inf, &socklen)) < 0) {
            print_error("error when trying to accept client connection\n");
            continue;
        }
        (void) fprintf(stdout, "accepting connection worked\n");
        (void) fprintf(stdout, "listening socket: %d\n", sockfd);
        (void) fprintf(stdout, "connected socket: %d\n", connfd);

        if ((pid = fork()) < 0) {
            print_error("error when forking\n");
            (void) close(sockfd);
            (void) close(connfd);
            return -1;
        }
        (void) fprintf(stdout, "forking worked\n");
        /* code, executed by the child process */
        if (!pid) {
            /* child process doesn't need listening socket */
            (void) close(sockfd);

            /* this should redirect the standard i/o to the socket
             if (dup2(connfd, STDIN_FILENO) < 0
             || dup2(connfd, STDOUT_FILENO) < 0) {
             fprintf(stderr,
             "error when trying to redirect the std streams\n");
             (void) close(connfd);
             return -1;
             }
             */

            /*
             * ### FB_CF: Warum doch nicht dup2()?
             */
            /*
             * ### FB_CF: stderr auch aufs socket umzuleiten macht nicht wirklich Sinn.
             *            Dann koennen Sie keine Fehlermeldungen mehr ausgeben. [-1]
             */

            close(0); /* close standard input  */
            close(1); /* close standard output */
            close(2); /* close standard error  */

            if (dup(connfd) != 0 || dup(connfd) != 1 || dup(connfd) != 2) {
                /*
                 * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0]
                 */
                (void) fprintf(stdout,
                        "error duplicating socket for stdin/stdout/stderr");
                (void) close(connfd);
                return -1;
            }

            /*
             * this should overlay the simple_message_server_logic
             * over the child process
             */
            if (execl(LOGIC_PATH, LOGIC_NAME, (char*) NULL) < 0) {
                /*
                 * can't print error message, because stdout is already the socket fd.
                 * so the client would get the error message.
                 * (void) fprintf(stderr, "execvp command threw an error");
                 */
                (void) close(connfd);
                return -1;
            }
            (void) close(connfd);
            return 0;

            /* code, executed by the server process */
        } else {
            /*
             * if an error occurs when trying to close the listening socket in the parent process,
             * it can be ignored
             */
            (void) close(connfd);
        }
    }

    /*
     * ### FB_CF: Dead Code
     */

    return 0;
}
/* === EOF ================================================================== */
