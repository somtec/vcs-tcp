/**
 * @file simple_message_client.c
 * Verteilte Systeme
 * TCP/IP Programmieruebung
 *
 * TCP/IP Client
 *
 * @author Andrea Maierhofer    1410258024  <andrea.maierhofer@technikum-wien.at>
 * @author Thomas Schmid        1410258013  <thomas.schmid@technikum-wien.at>
 * @date 2015/12/07
 *
 *
 * TODO store received stream on disk
 * TODO overwork read in of stream blockwise
 * TODO error handling in execute on read in
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

/* Defines for the request/response entries */
#define SET_USER "user="
#define SET_IMAGE "img="
#define GET_STATUS "status="
#define GET_FILE "file="

/* Define for the request field terminator */
#define FIELD_TERMINATOR '\n'

/* Timeout for waiting on socket to become ready in seconds */
#define SOCKET_TIMEOUT 10

/*
 * ---------------------------------------------------------------- globals --
 */

/*
 * --------------------------------------------------------------- static --
 */

/** Maximum filename length of file system. */
static long int smax_filename = 0;

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
static void verbose(const char* file_name, const char* function_name, int line,
    const char* message, ...);
static int execute(const char* server, const char* port, const char* user,
    const char* message, const char* image_url);
static int send_request(const char* user, const char* message,
    const char* image_url, int socket_fd);
static int read_response(int socket_fd);
static char* search_terminator(char* start, char* end);

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

    smc_parsecommandline(argc, argv, print_usage, &server, &port, &user,
        &message, &img_url, &sverbose);

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

    /* get maximum filename size */
    smax_filename = pathconf(".", _PC_NAME_MAX);
    if (-1 == smax_filename)
    {
        smax_filename = 0;
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

    (void) fflush(stderr); /* do not handle erros here */
    (void) fflush(stdout);

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
        written = fprintf(stdout, "%s [%s, %s(), line %d]: ", sprogram_arg0,
            file_name, function_name, line);
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
    struct addrinfo* addr_result;
    struct addrinfo* info;
    int info_result;
    int socket_fd;
    void* in_addr = NULL;
    char straddr[INET6_ADDRSTRLEN];
    struct sockaddr_in* s4;
    struct sockaddr_in6* s6;
    int read_result;
    int close_result;

    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP;

    info_result = getaddrinfo(server, port, &hints, &addr_result);
    if (info_result != 0)
    {
        print_error("getaddrinfo: %s", gai_strerror(info_result));
        return EXIT_FAILURE;
    }

    /* getaddrinfo() returns a list of address structures.
     Try each address until we successfully connect(2).
     If socket(2) (or connect(2)) fails, we (close the socket
     and) try the next address. */

    for (info = addr_result; info != NULL; info = info->ai_next)
    {
        socket_fd = socket(info->ai_family, info->ai_socktype,
            info->ai_protocol);
        if (socket_fd == -1)
            continue;

        if (connect(socket_fd, info->ai_addr, info->ai_addrlen) != -1)
            break; /* got a filedesriptor, success */

        close_result = close(socket_fd);
        if (close_result < 0)
        {
            print_error("Could not close tested socket: %s", strerror(errno));
        }
    }

    if (info == NULL)
    {
        /* No address succeeded */
        print_error("Could not connect %s:%s.", server, port);
        freeaddrinfo(addr_result);
        return EXIT_FAILURE;
    }

    /* Determine IP family of found address */
    switch (info->ai_family)
    {
    case AF_INET:
        /* IPv4 found */
        s4 = (struct sockaddr_in *) info->ai_addr;
        in_addr = &s4->sin_addr;
        break;
    case AF_INET6:
        /* IPv6 found */
        s6 = (struct sockaddr_in6 *) info->ai_addr;
        in_addr = &s6->sin6_addr;
        break;
    default:
        /* oh no, this should not happen */
        print_error("Unknown address family: %d.", info->ai_family);
        freeaddrinfo(addr_result);
        close_result = close(socket_fd);
        if (close_result < 0)
        {
            print_error("Could not close socket: %s", strerror(errno));
        }
        return EXIT_FAILURE;

    }
    /* Convert IP address into printable format */
    if (NULL == inet_ntop(info->ai_family, in_addr, straddr, sizeof(straddr)))
    {
        print_error("Could not convert server address: %s.", strerror(errno));
    }
    else
    {
        VERBOSE("Connection to %s (%s) on port %s established!", server,
            straddr, port);
    }

    freeaddrinfo(addr_result); /* No longer needed */

    if (send_request(user, message, image_url, socket_fd) != EXIT_SUCCESS)
    {
        close_result = close(socket_fd);
        if (close_result < 0)
        {
            print_error("Could not close socket: %s", strerror(errno));
        }
        return EXIT_FAILURE;
    }

    read_result = read_response(socket_fd);
    close_result = close(socket_fd);
    if (close_result < 0)
    {
        print_error("Could not close socket: %s", strerror(errno));
    }
    return read_result;

}

/**
 * /brief Send message request to server.
 *
 * /param user which wrote the message.
 * /param message to be shown in bulletin board.
 * /param image_url URL of image or NULL.
 * /param socket_fd open socket file descriptor.
 *
 * /return EXIT_SUCCESS on success, else EXIT_FAILURE.
 */
static int send_request(const char* user, const char* message,
    const char* image_url, int socket_fd)
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
    /** @TODO write blockwise */
    if (write(socket_fd, send_buf, len) != (ssize_t) len)
    {
        print_error("partial/failed write.");
        free(send_buf);
        return EXIT_FAILURE;
    }
    if (0 != shutdown(socket_fd, SHUT_WR)) /* no more writes */
    {
        print_error("Could not shutdown write connection: %s", strerror(errno));
        free(send_buf);
        return EXIT_FAILURE;
    }
    free(send_buf);
    return EXIT_SUCCESS;
}

/**
 * /brief Read response from server.
 *
 * /param socket_fd open socket file descriptor.
 *
 * /return EXIT_SUCCESS on success, else error status from server.
 */
static int read_response(int socket_fd)
{
    ssize_t read_count;
    fd_set set;
    struct timeval timeout;
    int ready;
    char* read_buf;
    char* parse_buf;
    bool finished = false;
    int result;
    size_t read_size;
    bool expect_status = true;
    //bool expect_html_file = false;
    char* current_write_pos;
    size_t amount;
    //size_t parse_amount;
    size_t len_status;
    //size_t len_file;
    // int number_of_files = 0;
    char* search;
    char* found;
    long int server_status;
    char* end_ptr;
    size_t move;

    len_status = strlen(GET_STATUS);
    //len_file = strlen(GET_FILE);

    read_size = smax_filename + 1;

    /* filenames must not exceed maximum name length of system */
    read_buf = malloc(read_size * sizeof(char));
    if (read_buf == NULL)
    {
        print_error("Can not allocate read buffer: %s.", strerror(ENOMEM));
        return EXIT_FAILURE;
    }
    parse_buf = malloc(2 * read_size * sizeof(char));
    if (parse_buf == NULL)
    {
        print_error("Can not allocate parse buffer: %s.", strerror(ENOMEM));
        free(read_buf);
        print_error(strerror(ENOMEM));
        return EXIT_FAILURE;
    }

    /* Initialize the file descriptor set. */
    FD_ZERO(&set);
    FD_SET(socket_fd, &set);

    result = EXIT_FAILURE;
    current_write_pos = parse_buf;
    amount = 0;
    //parse_amount = 0;

    while (!finished)
    {
        /* Initialize the timeout data structure. */
        timeout.tv_sec = SOCKET_TIMEOUT;
        timeout.tv_usec = 0;

        /* select returns 0 if timeout, 1 if input available, -1 if error. */
        /* wait a user defined time for socket to become ready */
        ready = select(socket_fd + 1, &set, NULL, NULL, &timeout);
        if (ready < 0)
        {
            /* can not handle errors or signals */
            print_error(strerror(errno));
            finished = true;
            continue;
        }
        if (ready == 0)
        {
            /* timeout */
            print_error("Timeout on receiving response.");
            finished = true;
            continue;
        }

        read_count = read(socket_fd, read_buf, read_size);
        if (read_count == -1)
        {
            print_error("read failed: %s", strerror(errno));
            free(read_buf);
            free(parse_buf);
            return EXIT_FAILURE;
        }
        amount += read_count;
        if (read_count == 0)
        {
            finished = true;
            /* end of file */
        }
        VERBOSE("Received %ld bytes.", (long ) read_count);

        if (expect_status)
        {
            if (read_count > 0)
            {
                memcpy(current_write_pos, read_buf, read_count);
                current_write_pos += read_count;
            }

            if (finished || (amount >= read_size))
            {
                /* it must contain the status= string now */

                if (amount < (len_status + 2))
                {
                    /* message is too short */
                    finished = true;
                    print_error("Malformed response (status).");
                    continue;
                }
                if (strncmp(GET_STATUS, parse_buf, len_status) != 0)
                {
                    finished = true;
                    print_error("Malformed response (status).");
                    continue;
                }
                /* search for linefeed */
                search = parse_buf + len_status;
                found = search_terminator(search, parse_buf + amount);
                if (found == search)
                {
                    finished = true;
                    print_error("Malformed response (status).");
                    continue;
                }
                if (found == (parse_buf + amount))
                {
                    /* not found */
                    finished = true;
                    print_error("Malformed response (status).");
                    continue;
                }
                *found = '\0'; /* make a string for strtol function */ 
                errno = 0;
                server_status = strtol(search, &end_ptr, 10);
                if ((errno == ERANGE && (server_status == LONG_MAX || server_status == LONG_MIN))
                    || (errno != 0 && server_status == 0))
                {
                    print_error("Can not convert server status (%s).", strerror(errno));
                    return EXIT_FAILURE;
                }

                if (end_ptr == search)
                {
                    print_error("No digits were found.");
                    return EXIT_FAILURE;
                }

                if (server_status < INT_MIN && server_status > INT_MAX)
                {
                    print_error("Server status exceeds int size: %l.", server_status);
                    /* cast the result later */
                }
                expect_status = false;
                //expect_html_file = true;
                move = (end_ptr - parse_buf + 1);
                if (amount > move)
                {
                    memmove(parse_buf, end_ptr + 1, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount; 
                }

            }
            if (expect_status)
            {
                if (amount < (len_status + 2))
                 {
                     /* message is too short */
                     continue;
                 }
                 if (strncmp(GET_STATUS, parse_buf, len_status) != 0)
                 {
                     finished = true;
                     print_error("Malformed response (status).");
                     continue;
                 }
                 /* search for linefeed */
                 search = parse_buf + len_status;
                 found = search_terminator(search, parse_buf + amount);
                 if (found == search)
                 {
                     finished = true;
                     print_error("Malformed response (status).");
                     continue;
                 }
                 if (found == (parse_buf + amount))
                 {
                     /* not found */
                     continue;
                 }
                 *found = '\0'; /* make a string for strtol function */ 
                 errno = 0;
                 server_status = strtol(search, &end_ptr, 10);
                 if ((errno == ERANGE && (server_status == LONG_MAX || server_status == LONG_MIN))
                     || (errno != 0 && server_status == 0))
                 {
                     print_error("Can not convert server status (%s).", strerror(errno));
                     return EXIT_FAILURE;
                 }

                 if (end_ptr == search)
                 {
                     print_error("No digits were found.");
                     return EXIT_FAILURE;
                 }

                 if (server_status < INT_MIN && server_status > INT_MAX)
                 {
                     print_error("Server status exceeds int size: %l.", server_status);
                     /* cast the result later */
                 }
                 expect_status = false;
                 //expect_html_file = true;
                 move = (end_ptr - parse_buf + 1);
                 if (amount > move)
                 {
                     memmove(parse_buf, end_ptr + 1, amount - move);
                     amount -= move;
                     current_write_pos = parse_buf + amount; 
                 }
            }
        }
    }
    free(read_buf);
    free(parse_buf);

    return result;

}

/**
 * \brief searches the terminator character.
 * 
 * \param start where to start the search.
 * \param end marker not considered to be a valid position.
 * 
 * \return pointer to FIELD_TERMINATOR on success, or end when not found.
 */
static char* search_terminator(char* start, char* end)
{
    char* search = start;

    while (search < end)
    {
        if (*search == FIELD_TERMINATOR)
        {
            break;
        }
        ++search;
    }
    return search;

}
