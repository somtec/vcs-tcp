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

/*
 * ---------------------------------------------------------------- defines --
 */

/* decimal format base for strtol */
#define INPUT_NUM_BASE 10

#define BUSINESS_LOGIC "simple_message_server_logic"
#define BUSINESS_LOGIC_PATH "/usr/local/bin/simple_message_server_logic"

#define LOWER_PORT_RANGE 0
#define UPPER_PORT_RANGE 65536
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
static void param_check(int argc, char** argv, char** port_nr);
static void kill_child_zombies(int signal);
static int reg_sig_handler(void);
static int do_connection(int socket_fd);
static void print_error(const char* message, ...);
static void print_usage(FILE* file, const char* message, int exit_code);
static int set_up_connection(const char* port_nr);

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
int main(int argc, char** argv)
{
    /* server port with type short int which is needed by the htons function */
    char* server_port = NULL;
    int socket_fd;

    sprogram_arg0 = argv[0];  /* must contain the filename anyway */

    /* calling the getopt function to get portnum*/
    param_check(argc, argv, &server_port);
    if (server_port == NULL)
    {
        return EXIT_FAILURE;
    }

    if ((socket_fd = set_up_connection(server_port)) < 0)
    {
        return EXIT_FAILURE;
    }
    if (reg_sig_handler() < 0)
    {
        return EXIT_FAILURE;
    }
    if (do_connection(socket_fd) < 0)
    {
        return EXIT_FAILURE;
    }

    /* should never come here */
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
            "  -p, --port <port>       well-known port of the server [0..65535]\n"
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

/**
 * \brief the method for checking the passed parameters
 *
 * This functions calls exit indirectly when passed parameters are invalid.
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 * \param port_str resulting port string for further usage
 *
 */
static void param_check(int argc, char **argv, char** port_str)
{
    char* end_ptr;
    long int port_nr = 0;
    int c;

    opterr = 0;

    *port_str = NULL;
    if (argc < 3)
    {
        print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
    }

    while ((c = getopt(argc, argv, "p:h::")) != EOF)
    {
        switch (c)
        {
        case 'p':
            port_nr = strtol(optarg, &end_ptr, INPUT_NUM_BASE);
            if ((errno == ERANGE && (port_nr == LONG_MAX || port_nr == LONG_MIN))
                    || (errno != 0 && port_nr == 0))
            {
                print_error("Can not convert port number (%s).", strerror(errno));
                print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
            }

            if (end_ptr == optarg)
            {
                print_error("No digits were found.");
                print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
            }

            if (port_nr < LOWER_PORT_RANGE || port_nr > UPPER_PORT_RANGE)
            {
                print_error("Port number out of range.");
                print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
            }
            *port_str = optarg;
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
    if (argc - optind >= 1)
    {
        print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
    }
    return;
}

/**
 * \brief Install signal handler for killing child zombie processes.
 *
 * If the functions fails then errno is set to EINVAL, see sigaction.
 *
 * \return 0 if sighandler was installed, else -1 on failure.
 */
int reg_sig_handler(void)
{
    struct sigaction sig;
    sig.sa_handler = kill_child_zombies;
    /*
     * excludes all signals from the signal handler mask.
     * Return value can be ignored, is always 0.
     */
    (void) sigemptyset(&sig.sa_mask);
    sig.sa_flags = SA_RESTART;

    errno = 0;
    return sigaction(SIGCHLD, &sig, NULL);
}

/**
 * \brief Kill all my children otherwise they will be zombies.
 */
static void kill_child_zombies(int signal)
{
    /*
     * waitpid waits for information about child-processes
     * status is requested for any child process
     * not interested in status of child process.
     * WNOHANG makes this function non-blocking
     */
    (void) signal;
    while (waitpid((pid_t) (WAIT_ANY), NULL, WNOHANG) > 0)
    {
    }
}

static int set_up_connection(const char* port_nr)
{
    int socket_fd = 0;
    struct addrinfo hints;
    struct addrinfo* result;
    struct addrinfo* rp;

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
    if (getaddrinfo(NULL, port_nr, &hints, &result) != 0)
    {

        print_error("error getaddrinfo\n");
        return -1;
    }

    /*
     * trys to bind to the available adresses.
     * breaks, if binding to one was successful.
     */
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
        {
            continue;
        }
        if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            break;
        }
        close(socket_fd);
    }

    /* no address succeeded */
    if (rp == NULL)
    {
        freeaddrinfo(result);
        print_error("error bind\n");
        return -1;
    }

    freeaddrinfo(result);

    if (listen(socket_fd, MAX_CONNECTION) < 0)
    {
        print_error("error listen\n");
        (void) close(socket_fd);
        return EXIT_FAILURE;
    }

    return socket_fd;
}

/**
 * \brief handle the connections of socket_fd.
 */
static int do_connection(int socket_fd)
{
    int pid;
    struct sockaddr_storage addr_inf;
    socklen_t socklen = sizeof(addr_inf);
    int connection_fd = 0;

    while (1)
    {
        if ((connection_fd = accept(socket_fd, (struct sockaddr*) &addr_inf, &socklen)) < 0)
        {
            print_error("Accept failed.\n");
            continue;
        }

        (void) fprintf(stdout, "accepting connection worked\n");
        (void) fprintf(stdout, "listening socket: %d\n", socket_fd);
        (void) fprintf(stdout, "connected socket: %d\n", connection_fd);

        if ((pid = fork()) < 0)
        {
            print_error("error on forking\n");
            (void) close(socket_fd);
            (void) close(connection_fd);
            return EXIT_FAILURE;
        }
        /* code, executed by the child process */
        if (pid == 0)
        {
            (void) fprintf(stdout, "forking worked\n");
            /* child process doesn't need listening socket */
            int debug = 1;
            while (debug == 1)
            {
                debug = 1;
            }
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
                (void) close(connection_fd);
                return EXIT_FAILURE;
            }
            (void) close(connection_fd);
            return EXIT_SUCCESS;

        }
        else
        {
            /* code, executed by the server process */
            /* in the parent */
            /*
             * if an error occurs when trying to close the listening socket in the parent process,
             * it can be ignored
             */
            (void) close(connection_fd);
        }
    }

    /* never come here */
    assert(0);
    return EXIT_FAILURE;
}
/* === EOF ================================================================== */

