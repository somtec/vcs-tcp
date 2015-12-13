/**
 * @file simple_message_server.c
 * Verteilte Systeme
 * TCP/IP Programmieruebung
 *
 * TCP/IP Server
 *
 * @author Andrea Maierhofer    1410258024  <andrea.maierhofer@technikum-wien.at>
 * @author Thomas Schmid        1410258013  <thomas.schmid@technikum-wien.at>
 * @date 2015/12/12
 *
 */

/*
 * --------------------------------------------------------------- includes --
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <stdarg.h>
#include <getopt.h>

/*
 * ---------------------------------------------------------------- defines --
 */

/* decimal format base for strtol */
#define INPUT_NUM_BASE 10

#define BUSINESS_LOGIC "simple_message_server_logic"
#define BUSINESS_LOGIC_PATH "/usr/local/bin/simple_message_server_logic"

#define LOWER_PORT_RANGE 0
#define UPPER_PORT_RANGE 65535
/* handle up to max connections */
#define MAX_CONNECTION 15

/*
 * ---------------------------------------------------------------- globals --
 */

/*
 * ----------------------------------------------------------------- static --
 */
static const char* sprogram_arg0 = NULL;

/*
 * ------------------------------------------------------------- prototypes --
 */
static void print_error(const char* message, ...);
static void print_usage(FILE* file, const char* message, int exit_code);
static void param_check(int argc, const char* const argv[], uint16_t* port_nr);
static int register_signal_handler(void);
static void kill_child_handler(int signal);
static int setup_connection(uint16_t port_nr);
static int do_connection(int socket_fd);
/*
 * -------------------------------------------------------------- functions --
 */

/**
 * \brief the main method for the server
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * \return success or failure.
 * \retval EXIT_SUCCESS if the function call was successful.
 * \retval EXIT_FAILURE on failure.
 *
 */
int main(int argc, const char* const argv[])
{
    /* server port with type short int which is needed by the htons function */
    uint16_t server_port;
    int socket_fd;

    sprogram_arg0 = argv[0];  /* must contain the filename anyway */

    /* calling the getopt function to get server_port*/
    param_check(argc, argv, &server_port);

    if ((socket_fd = setup_connection(server_port)) < 0)
    {
        return EXIT_FAILURE;
    }
    if (register_signal_handler() < 0)
    {
        (void) close(socket_fd);
        return EXIT_FAILURE;
    }
    if (do_connection(socket_fd) < 0)
    {
        return EXIT_FAILURE;
    }

    /* should never come here, assert due to our c rules */
    assert(0);
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

    written = fprintf(stream, "usage: %s option\noptions:\n", command);
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    written = fprintf(stream,
            "  -p, --port <port>       well-known port of the server [%d..%d]\n"
            "  -h, --help\n", LOWER_PORT_RANGE, UPPER_PORT_RANGE);
    if (written < 0)
    {
        print_error(strerror(errno));
    }
    fflush(stream);
    fflush(stderr);

    exit(exit_code);
}

/**
 * \brief Method for checking the passed parameters.
 *
 * This functions calls exit indirectly when passed parameters are invalid.
 *
 * \param argc the number of arguments.
 * \param argv the arguments itself (including the program name in argv[0]).
 * \param port_nr resulting port number for further usage.
 *
 */
static void param_check(int argc, const char* const argv[], uint16_t* port_nr)
{
    char* end_ptr;
    long int port_nr_convert;
    int c;
    const char* port = NULL;

    struct option long_options[] =
    {
        {"port", 1, NULL, 'p'},
        {"help", 0, NULL, 'h'},
        {0, 0, 0, 0}
    };

    opterr = 0;
    if (argc < 2)
    {
        print_usage(stderr, argv[0], EXIT_FAILURE);
    }

    while ((c = getopt_long(argc, (char** const) argv, "p:h", long_options,
            NULL)) != EOF)
    {
        switch (c)
        {
        case 'p':
            port = optarg;
            if (optarg != NULL)
            {
                port_nr_convert = strtol(optarg, &end_ptr, INPUT_NUM_BASE);
                if ((errno == ERANGE &&
                        (port_nr_convert == LONG_MAX || port_nr_convert == LONG_MIN))
                    || (errno != 0 && port_nr_convert == 0))
                {
                    print_error("Can not convert port number (%s).", strerror(errno));
                    print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
                }

                if (end_ptr == optarg)
                {
                    print_error("No digits were found.");
                    print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
                }

                if (port_nr_convert < LOWER_PORT_RANGE || port_nr_convert > UPPER_PORT_RANGE)
                {
                    print_error("Port number out of range.");
                    print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
                }
                /* set resulting port number */
                *port_nr = (uint16_t) port_nr_convert;
            }
            break;
        case 'h':
            /* when the usage message is requested, program will exit afterwards */
            print_usage(stdout, sprogram_arg0, EXIT_SUCCESS);
            break;
        case '?':
        default:
            /* occurs, when other arguments than -p or -h are passed */
            print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
            break;
        }
    }

    /* if user added extra arguments */
    if ((optind != argc) || (port == NULL))
    {
        print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
    }
    return;
}

/**
 * \brief Install signal handler for waiting on child processes.
 *
 * If the functions fails then errno is set to EINVAL, see sigaction.
 *
 * \return 0 if sighandler was installed, else -1 on failure.
 */
int register_signal_handler(void)
{
    struct sigaction sig;
    sig.sa_handler = kill_child_handler;
    /*
     * excludes all signals from the signal handler mask.
     * Return value can be ignored, is always 0.
     */
    (void) sigemptyset(&sig.sa_mask);
    sig.sa_flags = SA_RESTART; /* see beej, Restart syscall on signal return. */

    errno = 0;
    return sigaction(SIGCHLD, &sig, NULL);
}

/**
 * \brief Wait for all my children to be killed otherwise they will be zombies.
 *
 * \param signal of child will be ignored.
 */
static void kill_child_handler(int signal)
{
    /*
     * waitpid waits for information about child-processes
     * status is requested for any child process
     * not interested in status of child process.
     * WNOHANG makes this function non-blocking
     */
    (void) signal; /* pedantic */
    while (waitpid((pid_t) (WAIT_ANY), NULL, WNOHANG) > 0)
    {
    }
}

/**
 * \brief Setup the connection for a tcp socket.
 *
 * \param port_nr where this server listens.
 * \return On success a valid socket descriptor or -1 in case of failure.
 */
static int setup_connection(uint16_t port_nr)
{
    int socket_fd;
    /* set SO_REUSEADDR on a socket to true (1) */
    int optval = 1;
    struct sockaddr_in serveraddr;

    /* support only IP V4 */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
       print_error("socket() IPV4 failed.");
       return -1;
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval,
            sizeof(optval)) < 0)
    {
       print_error("setsockopt(SO_REUSEADDR) failed.");
       (void) close(socket_fd);
       return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    /* port number must be converted to network byte order */
    serveraddr.sin_port   = htons(port_nr);
    /* IPV 4 address to accept any incoming messages */
    serveraddr.sin_addr.s_addr   = htonl(INADDR_ANY);

    if (bind(socket_fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
    {
         print_error("bind() failed.");
         (void) close(socket_fd);
         return -1;
      }

    if (listen(socket_fd, MAX_CONNECTION) < 0)
    {
        print_error("listen() failed.");
        (void) close(socket_fd);
        return -1;
    }
    return socket_fd;
}

/**
 * \brief handle the connections of socket_fd.
 *
 * This function serves client request in a loop, so it should never exit.
 *
 * \param socket_fd listening socket.
 * \return -1 in case of a 'weird' program execution.
 */
static int do_connection(int socket_fd)
{
    int pid;
    struct sockaddr_storage addr_inf;
    socklen_t socklen = sizeof(addr_inf);
    int connection_fd;
    int written;

    while (1)
    {
        if ((connection_fd = accept(socket_fd, (struct sockaddr*) &addr_inf, &socklen)) < 0)
        {
            print_error("accept() failed.\n");
            continue;
        }

        if ((pid = fork()) < 0)
        {
            print_error("fork() failed.");
            (void) close(socket_fd);
            (void) close(connection_fd);
            return -1;
        }
        /* code, executed by the child process */
        if (pid == 0)
        {
            written = fprintf(stdout, "fork() successful.");
            if (written < 0)
            {
                print_error(strerror(errno));
            }

            /* child process doesn't need listening socket */
            if (close(socket_fd) != 0)
            {
                print_error("Child process could not close listening socket.");
                (void) close(connection_fd);
                exit(EXIT_FAILURE);
            }

            /* redirect stdin and stdout to connect socket */
            if ((dup2(connection_fd, STDIN_FILENO) == -1) || (
                    dup2(connection_fd, STDOUT_FILENO) == -1))
            {
                print_error("Child process dup failed.\n");
                (void) close(connection_fd); /* in case of error no handling */
                exit(EXIT_FAILURE);
            }

            /* After dup, connection_fd is no longer needed */
            if (close(connection_fd) != 0)
            {
                print_error("Child process could not close connect socket.\n");
                exit(EXIT_FAILURE);
            }

            /*
             * this should overlay the simple_message_server_logic
             * over the child process
             */
            if (execl(BUSINESS_LOGIC_PATH, BUSINESS_LOGIC, NULL) < 0)
            {
                print_error("Could not start server business logic.\n");
                exit(EXIT_FAILURE);
            }
            assert(0);  /*never come here after successful execl */
            exit(EXIT_FAILURE);
        }
        else
        {
            /* code, executed by the server process */
            /* in the parent */
            /*
             * if an error occurs when trying to close the listening socket in
             * the parent process, it can be ignored
             */
            (void) close(connection_fd);
        }
    }

    /* never come here */
    assert(0);
    return EXIT_FAILURE;
}
/* === EOF ================================================================== */

