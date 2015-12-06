/**
 * @file simple_message_client.c
 *
 *
 * @version $Revision: 1.0 $
 *
 * @TODO store received stream on disk
 * @TODO overwork read in of stream blockwise
 * @TODO error handling in execute on read in
 *
 */

/*
 * --------------------------------------------------------------- includes --
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <simple_message_client_commandline_handling.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * ---------------------------------------------------------------- defines --
 */

/* macro used for printing source line etc. in verbose function */
#define VERBOSE(...) verbose(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Defines for the request entries */
#define SET_USER "user="
#define SET_IMAGE "img="

/* Define for the request field terminator */
#define FIELD_TERMINATOR '\n'

/*
 * ---------------------------------------------------------------- globals --
 */

/*
 * --------------------------------------------------------------- static --
 */

/** Maximum path length of file system. */
static long int smax_path = 0;

/** Current program arguments. */
static const char* sprogram_arg0 = NULL;

/** Controls the verbose output. */
static int sverbose = 0;

/*
 * ------------------------------------------------------------- prototypes --
 */
static void print_usage(FILE* file, const char* message, int exit_code);
static void print_error(const char* message, ...);
static int init(const char** program_args);
static void cleanup(bool exit);
static void verbose(const char* file_name, const char* function_name, int line, const char* message,
        ...);
static int execute(const char* server, const char* port, const char* user, const char* message,
        const char* image_url);
static int send_request(const char* user, const char* message, const char* image_url, int sfd);


/*
 * -------------------------------------------------------------- functions --
 */

/**
 * @brief       Main function
 *
 * This function is the main entry point of the program. 
 * it opens a connection to specified server and sends and receives data
 * This is the main entry point for any C program.
 *
 * \param argc the number of arguments.
 * \param argv the arguments itself (including the program name in argv[0]).
 *
 * \return EXIT_SUCCESS on success  EXIT_FAILURE on error.
 * \retval EXIT_SUCCESS Program ended successfully.
 * \retval EXIT_FAILURE Program ended with failure.
 */
int main(int argc, const char* argv[])
{
    int result;
    const char* server;
    const char* port;
    const char* user;
    const char* message;
    const char* img_url;
    const char* img_url_text;
    char* end_ptr;
    long int port_nr;

    sprogram_arg0 = argv[0];

    smc_parsecommandline(argc, argv, print_usage, &server, &port, &user, &message, &img_url,
            &sverbose);

    errno = 0;
    port_nr = strtol(port, &end_ptr, 10);
    if ((errno == ERANGE && (port_nr == LONG_MAX || port_nr == LONG_MIN))
            || (errno != 0 && port_nr == 0))
    {
        print_error("Can not convert port number (%s).", strerror(errno));
        return EXIT_FAILURE;
    }

    if (end_ptr == port)
    {
        print_error("No digits were found.");
        return EXIT_FAILURE;
    }

    if (port_nr < 0 && port_nr > 65535)
    {
        print_error("Port number out of range.");
        print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
    }

    img_url_text = img_url == NULL ? "<no image>" : img_url;
    VERBOSE("Got parameter server %s, port %s, user %s, message %s, "
            "image %s", server, port, user, message, img_url_text);

    /* so_REuseaddr mit setsockopt im server */

    result = init(argv);
    if (EXIT_SUCCESS != result)
    {
        cleanup(true);
    }

    result = execute(server, port, user, message, img_url);
    cleanup(false);

    return result;
}

/**
 * \brief Initializes the program.
 *
 * \param program_args is the program argument vector.
 *
 * \return EXIT_SUCCESS the program was successfully initialized,
 *  otherwise program startup failed.
 * \retval EXIT_SUCCESS everything is fine.
 * \retval EXIT_FAILURE No data available for maximum path length.
 */
static int init(const char** program_args)
{
    sprogram_arg0 = program_args[0];
    VERBOSE("Initialize program.");

    /* get maximum directory size */
    smax_path = pathconf(".", _PC_PATH_MAX);
    if (-1 == smax_path)
    {
        smax_path = 0;
        print_error("pathconf() failed: %s.", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
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

    fflush(stderr);
    fflush(stdout);

    if (exit_program)
    {
        exit(EXIT_FAILURE);
    }

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
    (void)fprintf(stderr, "%s: ", sprogram_arg0);
    va_start(args, message);
    (void)vfprintf(stderr, message, args);
    va_end(args);
    (void)fprintf(stderr, "\n");
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
    written = fprintf(stream, "  -s, --server <server>   full qualified domain "
            "name or IP address of the server\n"
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
 * \brief Gives messages out when verbose option is set.
 *
 * Printout can be formatted like printf.
 *
 * \param file_name of c source to be print.
 * \param function_name to be print without ().
 * \param line source code line number.
 * \param message to be print.
 *
 * \return void
 */
static void verbose(const char* file_name, const char* function_name, int line,
        const char* message, ...)
{
    int written;
    va_list args;

    if (sverbose > 0)
    {
        written = fprintf(stdout, "%s [%s, %s(), line %d]: ", sprogram_arg0, file_name,
                function_name, line);
        if (written < 0)
        {
            print_error(strerror(errno));
        }
        va_start(args, message);
        written = vfprintf(stdout, message, args);
        if (written < 0)
        {
            print_error(strerror(errno));
        }
        va_end(args);
        written = fprintf(stdout, "\n");
        if (written < 0)
        {
            print_error(strerror(errno));
        }
    }
}


/**
 * \brief Executes the request and get the response from server.
 *
 * /param server address.
 * /param port of server.
 * /param user which wrote the message.
 * /param message to be shown in bulletin board.
 * /param image_url URL of image or NULL.
 *
 * /return EXIT_SUCCESS on success, else EXIT_FAILURE.
 */
static int execute(const char* server, const char* port, const char* user,
        const char* message, const char* image_url)
{
    struct addrinfo hints;
    struct addrinfo* result;
    struct addrinfo* rp;
    int info;
    int sfd;
    ssize_t nread;
    char buf[1000];
    void* in_addr = NULL;
    char straddr[INET6_ADDRSTRLEN];
    struct sockaddr_in* s4;
    struct sockaddr_in6* s6;
    fd_set set;
    struct timeval timeout;
    int ready;

    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP; 

    info = getaddrinfo(server, port, &hints, &result);
    if (info != 0)
    {
        print_error("getaddrinfo: %s", gai_strerror(info));
        return EXIT_FAILURE;
    }

    /* getaddrinfo() returns a list of address structures.
     Try each address until we successfully connect(2).
     If socket(2) (or connect(2)) fails, we (close the socket
     and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */

        (void)close(sfd); /* error handling is not necessary, not interested */
                          /* in this socket */
    }

    if (rp == NULL)
    {
        /* No address succeeded */
        print_error("Could not connect %s:%s.", server, port);
        freeaddrinfo(result);
        return EXIT_FAILURE;
    }

    /* Determine IP family of found address */
    switch (rp->ai_family)
    {
        case AF_INET:
            /* IPv4 found */
            s4 = (struct sockaddr_in *) rp->ai_addr;
            in_addr = &s4->sin_addr;
            break;
        case AF_INET6:
            /* IPv6 found */
            s6 = (struct sockaddr_in6 *) rp->ai_addr;
            in_addr = &s6->sin6_addr;
            break;
        default:
            /* oh no, this should not happen */
            print_error("Unknown address family: %d.", rp->ai_family);
            freeaddrinfo(result);
            return EXIT_FAILURE;

    }
    /* Convert IP address into printable format */
    if (NULL == inet_ntop(rp->ai_family, in_addr, straddr, sizeof(straddr)))
    {
        print_error("Could not convert server address: %s.", strerror(errno));
    }
    else
    {
        VERBOSE("Connection to %s (%s) on port %s established!", server, straddr,
                port);
    }

    freeaddrinfo(result); /* No longer needed */

    if (send_request(user, message, image_url, sfd) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    /* Initialize the file descriptor set. */
    FD_ZERO(&set);
    FD_SET(sfd, &set);

    /* Initialize the timeout data structure. */
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    /* select returns 0 if timeout, 1 if input available, -1 if error. */
    ready = select(sfd + 1, &set, NULL, NULL, &timeout);
    if (ready < 0)
    {
        /* can not handle errors or signals */
        print_error(strerror(errno));
    }

    nread = read(sfd, buf, 1000);
    if (nread == -1)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    VERBOSE("Received %ld bytes.", (long ) nread);

    return EXIT_SUCCESS;
}

/**
 * /brief Send message request to server.
 *
 * /param user which wrote the message.
 * /param message to be shown in bulletin board.
 * /param image_url URL of image or NULL.
 * /param sfd open socket file descriptor.
 *
 * /return EXIT_SUCCESS on succuss, else EXIT_FAILURE.
 */
static int send_request(const char* user, const char* message,
        const char* image_url, int sfd)
{
    size_t len_user;
    size_t len_user_data;
    size_t len_message;
    size_t len_image;
    size_t len_image_url;
    size_t len;
    char* send_buf;
    char* destination;

    len_user = strlen(SET_USER);
    len_user_data = strlen(user) + 1; /* + 1 for 0xa terminator */
    len_message = strlen(message);
    if (image_url == NULL)
    {
        len_image = 0;
        len_image_url = 0;
    }
    else
    {
        len_image = strlen(SET_IMAGE);
        len_image_url = strlen(image_url) + 1; /* +1 for 0xa terminator */
    }
    len = len_user + len_user_data + len_image + len_image_url + len_message;
    VERBOSE("Send request of %ld bytes.", (long ) len);

    send_buf = malloc(len * sizeof(char));
    if (send_buf == NULL)
    {
        print_error(strerror(ENOMEM));
        return EXIT_FAILURE;
    }
    destination = send_buf;
    strncpy(destination, SET_USER, len_user);
    destination += len_user;
    strncpy(destination, user, len_user_data - 1);
    destination += len_user_data - 1;
    *destination = FIELD_TERMINATOR;
    ++destination;
    if (image_url != NULL)
    {
        strncpy(destination, SET_IMAGE, len_image);
        destination += len_image;
        strncpy(destination, image_url, len_image_url - 1);
        destination += len_image_url - 1;
        *destination = FIELD_TERMINATOR;
        ++destination;
    }
    strncpy(destination, message, len_message);
    /** @TODO error handling */
    if (write(sfd, send_buf, len) != (ssize_t) len)
    {
        print_error("partial/failed write.");
        free(send_buf);
        return EXIT_FAILURE;
    }
    if (0 != shutdown(sfd, SHUT_WR)) /* no more writes */
    {
        print_error("Could not shutdown write connection: %s", strerror(errno));
        free(send_buf);
        return EXIT_FAILURE;
    }
    free(send_buf);
    return EXIT_SUCCESS;
}
