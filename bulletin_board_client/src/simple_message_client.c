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
 * TODO remove duplicated code.
 * TODO check if we received at minimum 1 html file in response.
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
#include <assert.h>

/*
 * ---------------------------------------------------------------- defines --
 */

/* decimal format base for strtol */
#define INPUT_NUM_BASE 10

/* defines for the allowed port range */
#define LOWER_PORT_RANGE 0
#define UPPER_PORT_RANGE 65535

/* macro used for printing source line etc. in verbose function */
#define VERBOSE(...) verbose(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Defines for the request/response string literals */
#define SET_USER "user="
#define SET_IMAGE "img="
#define GET_STATUS "status="
#define GET_FILE "file="
#define GET_LEN "len="

/* Define for the request field terminator */
#define FIELD_TERMINATOR '\n'

/* Timeout for waiting on socket to become ready in seconds */
#define SOCKET_TIMEOUT 30

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
static int convert_server_status(char* start, char* end, int* server_status);
static int convert_file_size(char* start, char* end, long* size);
int check_text(size_t amount, char* text, char* parse_buf);
int search_end_marker(char** found, char* parse_buf, int amount,
    bool buffer_full);

/*
 * -------------------------------------------------------------- functions --
 */

/**
 * @brief main function
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
    port_nr = strtol(port, &end_ptr, INPUT_NUM_BASE);
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

    if (port_nr < LOWER_PORT_RANGE || port_nr > UPPER_PORT_RANGE)
    {
        print_error("Port number out of range.");
        print_usage(stderr, sprogram_arg0, EXIT_FAILURE);
    }

    img_url_text = img_url == NULL ? "<no image>" : img_url;
    VERBOSE("Got parameter server %s, port %s, user %s, message %s, "
            "image %s", server, port, user, message, img_url_text);

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
    errno = 0;
    smax_filename = pathconf(".", _PC_NAME_MAX);
    if (-1 == smax_filename)
    {
        smax_filename = 0;
        if (errno != 0)
        {
            print_error("pathconf() failed: %s.", strerror(errno));
        }
        else
        {
            print_error("Could not determine maximum filename length.");
        }
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
        {
            continue;
        }

        if (connect(socket_fd, info->ai_addr, info->ai_addrlen) != -1)
        {
            /* got a file descriptor, success */
            break;
        }

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

    /* For verbose output determine IP family of found address */
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
        /* which in_addr is to set here? */
        print_error("Unknown address family: %d.", info->ai_family);
        freeaddrinfo(addr_result);
        close_result = close(socket_fd);
        if (close_result < 0)
        {
            print_error("Could not close socket: %s", strerror(errno));
        }
        assert(0); /* unreachable, therefore mandatory due to our C rules */
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
    ssize_t written;
    ssize_t to_be_written;
    char* current_write_pos;

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
    current_write_pos = send_buf;
    to_be_written = len;
    while (to_be_written > 0)
    {
        written = write(socket_fd, current_write_pos, to_be_written);
        if (written < 0)
        {
            print_error("Could not write request: %s", strerror(errno));
            free(send_buf);
            return EXIT_FAILURE;
        }
        current_write_pos += written;
        to_be_written -= written;
    }
    VERBOSE("Send request of %ld bytes successful.", (long ) len);

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
    bool expect_file = false;
    char* current_write_pos;
    size_t amount;
    char* search;
    char* found;
    int server_status;
    size_t move;
    bool receive_status = true;
    bool search_filename = true;
    bool search_end;
    bool buffer_full;
    char* filename_buf;
    long file_size;
    bool search_len = false;
    bool store_file = false;
    bool search_end2 = false;
    FILE* store = NULL;
    bool eof = false;
    size_t written = 0;
    size_t file_written;
    size_t to_file;
    bool check_further_file = false;

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
    filename_buf = malloc(read_size * sizeof(char));
    if (filename_buf == NULL)
    {
        free(read_buf);
        free(parse_buf);
        print_error("Can not allocate filename buffer: %s.", strerror(ENOMEM));
        return EXIT_FAILURE;
    }
    filename_buf[0] = '\0';

    /* Initialize the file descriptor set. */
    FD_ZERO(&set);
    FD_SET(socket_fd, &set);

    result = EXIT_FAILURE;
    current_write_pos = parse_buf;
    amount = 0;

    while (!finished)
    {
        if (!eof)
        {
            /* Initialize the timeout data structure. */
            timeout.tv_sec = SOCKET_TIMEOUT;
            timeout.tv_usec = 0;
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
                free(filename_buf);
                return EXIT_FAILURE;
            }
            if (check_further_file)
            {
                check_further_file = false;
                if (read_count == 0)
                {
                    finished = true;
                    continue;
                }
                else
                {
                    expect_file = true;
                    store_file = false;
                    search_filename = true;
                    written = 0;
                }
            }
            amount += read_count;
            if (read_count == 0)
            {
                finished = true;
                eof = true;
                /* end of file */
            }
            VERBOSE("Received %ld bytes.", (long ) read_count);
            buffer_full = eof || (amount >= read_size);
            if (read_count > 0)
            {
                /* copy the read bytes into the parse buffer */
                memcpy(current_write_pos, read_buf, read_count);
                current_write_pos += read_count;
            }
        }
        if (expect_status)
        {
            if (receive_status)
            {
                VERBOSE("Receiving status.");
                receive_status = false;
            }
            if (buffer_full)
            {
                /* it must contain the status= string now */
                if (check_text(amount, GET_STATUS, parse_buf) != EXIT_SUCCESS)
                {
                    finished = true;
                    continue;
                }

                /* search for line feed */
                search = parse_buf + strlen(GET_STATUS);
                found = search_terminator(search, parse_buf + amount);
                if (found == search)
                {
                    finished = true;
                    print_error("Malformed response (no status nr).");
                    continue;
                }
                if (found == (parse_buf + amount))
                {
                    /* not found */
                    finished = true;
                    print_error("Malformed response (no status terminator).");
                    continue;
                }
                if (EXIT_SUCCESS
                        != convert_server_status(search, found, &server_status))
                {
                    finished = true;
                    continue;
                }
                expect_status = false;
                VERBOSE("Received status.");
                expect_file = true;
                move = (found - parse_buf + 1);
                if (move > 0)
                {
                    memmove(parse_buf, found + 1, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                }
            }
            else
            {
                if (amount < (strlen(GET_STATUS) + 2))
                {
                    /* message is too short */
                    continue;
                }
                /* it must contain the status= string now */
                if (check_text(amount, GET_STATUS, parse_buf) != EXIT_SUCCESS)
                {
                    finished = true;
                    continue;
                }
                /* search for line feed */
                search = parse_buf + strlen(GET_STATUS);
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
                if (convert_server_status(search, found,
                        &server_status) != EXIT_SUCCESS)
                {
                    finished = true;
                    continue;
                }
                expect_status = false;
                VERBOSE("Received status.");
                expect_file = true;
                move = (found - parse_buf + 1);
                if (move > 0)
                {
                    memmove(parse_buf, found + 1, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                }
                if (!eof && (amount == 0))
                {
                    continue;
                }
            }
        }

        if (expect_file)
        {
            VERBOSE("Receive response file.");
            if (buffer_full)
            {
                if (search_filename)
                {
                    /* it must contain the file= string now */
                    if (check_text(amount, GET_FILE, parse_buf) != EXIT_SUCCESS)
                    {
                        finished = true;
                        continue;
                    }
                    search_filename = false;
                    move = strlen(GET_FILE);

                    memmove(parse_buf, parse_buf + move, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_end = true;
                }
                if (search_end)
                {
                    /* search for line feed */
                    if (EXIT_SUCCESS
                            != search_end_marker(&found, parse_buf, amount,
                                    buffer_full))
                    {
                        finished = true;
                        continue;
                    }
                    if (found == NULL)
                    {
                        continue;
                    }
                    search_end = false;
                    /* make 0 string */
                    *found = 0;
                    search_end = false;
                    /* get filename now */
                    if (found >= (parse_buf + read_size))
                    {
                        /* filename too long, can not store it */
                        print_error("Filename too long.");
                        finished = true;
                        continue;
                    }

                    strcpy(filename_buf, parse_buf);
                    /* the evil testcase 8 has written do /dev/null */
                    /* so I do not allow to write to paths other than . */
                    if (strchr(filename_buf, '/') != NULL)
                    {
                        print_error("File %s is not allowed.", filename_buf);
                        finished = true;
                        continue;
                    }
                    move = strlen(filename_buf) + 1;
                    memmove(parse_buf, parse_buf + move, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_len = true;
                }
                if (search_len)
                {
                    if (amount < (strlen(GET_LEN) + 2))
                    {
                        /* message is too short */
                        continue;
                    }
                    /* it must contain the file= string now */

                    if (check_text(amount, GET_LEN, parse_buf) != EXIT_SUCCESS)
                    {
                        finished = true;
                        continue;
                    }
                    search_len = false;
                    move = strlen(GET_LEN);

                    memmove(parse_buf, parse_buf + move, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_end2 = true;
                }
                if (search_end2)
                {
                    if (EXIT_SUCCESS
                            != search_end_marker(&found, parse_buf, amount,
                                    buffer_full))
                    {
                        finished = true;
                        continue;
                    }
                    if (found == NULL)
                    {
                        continue;
                    }
                    /* get file size now */
                    if (convert_file_size(parse_buf, found, &file_size) !=
                    EXIT_SUCCESS)
                    {
                        finished = true;
                        continue;
                    }
                    move = found - parse_buf + 1;
                    memmove(parse_buf, found + 1, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_end2 = false;
                    store_file = true;
                }
                if (store_file)
                {
                    if (store == NULL)
                    {
                        store = fopen(filename_buf, "w+");
                        if (NULL == store)
                        {
                            print_error("Can not create file %s", filename_buf);
                            finished = true;
                            continue;
                        }
                    }
                    if (file_size == 0)
                    {
                        if (0 != fclose(store))
                        {
                            print_error("Can not close file: %s",
                                    strerror(errno));
                        }
                        else
                        {
                            VERBOSE("File %s stored.", filename_buf);
                        }

                        store = NULL;
                        if (amount > 0)
                        {
                            expect_file = true;
                            store_file = false;
                            search_filename = true;
                            written = 0;
                        }
                        else
                        {
                            check_further_file = true;
                        }
                        continue;
                    }
                    if (eof)
                    {
                        if (amount == 0)
                        {
                            print_error("Too less data found for file %s.",
                                    filename_buf);
                            finished = true;
                            continue;
                        }
                        to_file =
                                amount < (file_size - written) ?
                                        amount : (file_size - written);
                        file_written = fwrite(parse_buf, sizeof(char), to_file,
                                store);
                        if (file_written < to_file)
                        {
                            print_error("Error on writing file %s",
                                    filename_buf);
                            finished = true;
                            continue;
                        }
                        written += file_written;
                        if ((long)written < file_size)
                        {
                            /* sorry this is too less data */
                            print_error("Too less data for file %s",
                                    filename_buf);
                            finished = true;
                            continue;
                        }
                        if (0 != fclose(store))
                        {
                            print_error("Can not close file: %s",
                                    strerror(errno));
                        }
                        else
                        {
                            VERBOSE("File %s stored.", filename_buf);
                        }
                        store = NULL;

                        memmove(parse_buf, parse_buf + file_written,
                                amount - file_written);
                        amount -= file_written;
                        current_write_pos = parse_buf + amount;
                        buffer_full = eof || (amount >= read_size);
                        if (amount > 0)
                        {
                            expect_file = true;
                            store_file = false;
                            search_filename = true;
                            written = 0;
                            continue;
                        }
                        finished = true;
                    }
                    else
                    {
                        if (amount == 0)
                        {
                            continue;
                        }
                        to_file =
                                amount < (file_size - written) ?
                                        amount : (file_size - written);
                        file_written = fwrite(parse_buf, sizeof(char), to_file,
                                store);
                        if (file_written < to_file)
                        {
                            print_error("Error on writing file %s",
                                    filename_buf);
                            finished = true;
                            continue;
                        }
                        written += file_written;
                        if ((long)written == file_size)
                        {
                            if (0 != fclose(store))
                            {
                                print_error("Can not close file: %s",
                                        strerror(errno));
                            }
                            else
                            {
                                VERBOSE("File %s stored.", filename_buf);
                            }

                            store = NULL;
                        }
                        memmove(parse_buf, parse_buf + file_written,
                                    amount - file_written);
                        amount -= file_written;
                        current_write_pos = parse_buf + amount;
                        buffer_full = eof || (amount >= read_size);
                        if (store == NULL)
                        {
                            if (amount > 0)
                            {
                                expect_file = true;
                                store_file = false;
                                search_filename = true;
                                written = 0;
                                continue;
                            }
                            else
                            {
                                /* see for further file */
                                check_further_file = true;
                            }
                        }
                        continue;
                    }
                }
            }
            else
            {

                if (search_filename)
                {
                    if (amount < (strlen(GET_FILE) + 2))
                    {
                        /* message is too short */
                        continue;
                    }
                    /* it must contain the file= string now */
                    if (check_text(amount, GET_FILE, parse_buf) != EXIT_SUCCESS)
                    {
                        finished = true;
                        continue;
                    }
                    search_filename = false;
                    move = strlen(GET_FILE);

                    memmove(parse_buf, parse_buf + move, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_end = true;
                }
                if (search_end)
                {
                    /* search for line feed */
                    if (EXIT_SUCCESS
                            != search_end_marker(&found, parse_buf, amount,
                                    buffer_full))
                    {
                        finished = true;
                        continue;
                    }
                    if (found == NULL)
                    {
                        continue;
                    }
                    search_end = false;
                    /* make 0 string */
                    *found = 0;
                    search_end = false;
                    /* get filename now */
                    if (found >= (parse_buf + read_size))
                    {
                        /* filename too long, can not store it */
                        print_error("Filename too long.");
                        finished = true;
                        continue;
                    }
                    strcpy(filename_buf, parse_buf);
                    /* the evil testcase 8 has written do /dev/null */
                    /* so I do not allow to write to paths other than . */
                    if (strchr(filename_buf, '/') != NULL)
                    {
                        print_error("File %s is not allowed.", filename_buf);
                        finished = true;
                        continue;
                    }
                    move = strlen(filename_buf) + 1;
                    memmove(parse_buf, parse_buf + move, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_len = true;
                }
                if (search_len)
                {
                    if (amount < (strlen(GET_LEN) + 2))
                    {
                        /* message is too short */
                        continue;
                    }
                    /* it must contain the file= string now */

                    if (check_text(amount, GET_LEN, parse_buf) != EXIT_SUCCESS)
                    {
                        finished = true;
                        continue;
                    }
                    search_len = false;
                    move = strlen(GET_LEN);

                    memmove(parse_buf, parse_buf + move, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_end2 = true;
                }
                if (search_end2)
                {
                    if (EXIT_SUCCESS
                            != search_end_marker(&found, parse_buf, amount,
                                    buffer_full))
                    {
                        finished = true;
                        continue;
                    }
                    if (found == NULL)
                    {
                        continue;
                    }
                    /* get file size now */
                    if (convert_file_size(parse_buf, found, &file_size) !=
                    EXIT_SUCCESS)
                    {
                        finished = true;
                        continue;
                    }
                    move = found - parse_buf + 1;
                    memmove(parse_buf, found + 1, amount - move);
                    amount -= move;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    search_end2 = false;
                    store_file = true;
                }
                if (store_file)
                {
                    if (store == NULL)
                    {
                        store = fopen(filename_buf, "w+");
                        if (NULL == store)
                        {
                            print_error("Can not create file %s", filename_buf);
                            finished = true;
                            continue;
                        }
                    }
                    if (file_size == 0)
                    {
                        if (0 != fclose(store))
                        {
                            print_error("Can not close file: %s",
                                    strerror(errno));
                        }
                        store = NULL;
                        if (amount > 0)
                        {
                            expect_file = true;
                            store_file = false;
                            search_filename = true;
                            written = 0;
                        }
                        else
                        {
                            check_further_file = true;
                        }
                        continue;
                    }

                    if (amount == 0)
                    {
                        continue;
                    }
                    to_file =
                            amount < (file_size - written) ?
                                    amount : (file_size - written);
                    file_written = fwrite(parse_buf, sizeof(char), to_file,
                            store);
                    if (file_written < to_file)
                    {
                        print_error("Error on writing file %s", filename_buf);
                        finished = true;
                        continue;
                    }
                    written += file_written;
                    if ((long) written == file_size)
                    {
                        if (0 != fclose(store))
                        {
                            print_error("Can not close file: %s",
                                    strerror(errno));
                        }
                        else
                        {
                            VERBOSE("File %s stored.", filename_buf);
                        }

                        store = NULL;
                    }
                    memmove(parse_buf, parse_buf + file_written, amount - file_written);
                    amount -= file_written;
                    current_write_pos = parse_buf + amount;
                    buffer_full = eof || (amount >= read_size);
                    if (store == NULL)
                    {
                        if (amount > 0)
                        {
                            expect_file = true;
                            store_file = false;
                            search_filename = true;
                            written = 0;
                            continue;
                        }
                        else
                        {
                            /* see for further file */
                            check_further_file = true;
                        }
                    }
                    continue;
                }
            }
        }
    }
    free(read_buf);
    free(parse_buf);
    free(filename_buf);
    if (store != NULL)
    {
        if (0 != fclose(store))
        {
            print_error("Can not close file: %s", strerror(errno));
        }
    }

    return result;

}

/**
 * /brief Search the terminator character.
 *
 * /param found return value pointer where the terminator has been found
 *      or NULL if not found
 * /param parse_buf where to search the terminator.
 * /param amount of data to be tested.
 * /param buffer_full indicates if the read buffer is full.
 *
 * /return EXIT_SUCCESS if found or too less data, EXIT_FAILURE if terminator
 *      is at wrong position or not contained if buffer is full.
 */
int search_end_marker(char** found, char* parse_buf, int amount,
    bool buffer_full)
{
    char* search;
    char* terminator;

    /* search for line feed */
    search = parse_buf;
    terminator = search_terminator(search, parse_buf + amount);
    if (terminator == search)
    {
        print_error("Malformed response.");
        return EXIT_FAILURE;
    }
    if (terminator == (parse_buf + amount))
    {
        /* not found */
        if (buffer_full)
        {
            /* it must contain the terminator in this case */
            print_error("Malformed response.");
            return EXIT_FAILURE;
        }
        /* see if there is further data */
        *found = NULL;
        return EXIT_SUCCESS;
    }
    /* make 0 string */
    *terminator = 0;
    *found = terminator;
    return EXIT_SUCCESS;
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

/**
 * \brief searches the terminator character.
 *
 * \param start where to start the number begins.
 * \param end marker contains newline.
 * \server_status converted server status as number.
 *
 * \return EXIT_SUCCESS if server status was converted, else EXIT_FAILURE.
 */
static int convert_server_status(char* start, char* end, int* server_status)
{
    long int result;
    char* end_strtol;

    *end = '\0'; /* make a string for strtol function */
    errno = 0;
    result = strtol(start, &end_strtol, INPUT_NUM_BASE);
    if ((errno == ERANGE && (result == LONG_MAX || result == LONG_MIN))
            || (errno != 0 && result == 0))
    {
        print_error("Can not convert server status (%s).", strerror(errno));
        return EXIT_FAILURE;
    }

    if (end_strtol == start)
    {
        print_error("No digits were found.");
        return EXIT_FAILURE;
    }

    if (result < INT_MIN || result > INT_MAX)
    {
        print_error("Server status exceeds int size: %l.", result);
        /* make result clearly a failure */
        *server_status = EXIT_FAILURE;
    }
    else
    {
        *server_status = (int) result;
    }

    return EXIT_SUCCESS;
}

/**
 * \brief searches the terminator character.
 *
 * \param start where to start the number begins.
 * \param end marker contains newline.
 * \size converted size as result.
 *
 * \return EXIT_SUCCESS if file size was converted, else EXIT_FAILURE.
 */
static int convert_file_size(char* start, char* end, long* size)
{
    long result;
    char* end_strtol;

    *end = '\0'; /* make a string for strtol function */
    errno = 0;
    result = strtol(start, &end_strtol, INPUT_NUM_BASE);
    if ((errno == ERANGE && (result == LONG_MAX || result == LONG_MIN))
            || (errno != 0 && result == 0))
    {
        print_error("Can not convert file size (%s).", strerror(errno));
        return EXIT_FAILURE;
    }

    if (end_strtol == start)
    {
        print_error("No digits were found.");
        return EXIT_FAILURE;
    }

    if (result < 0)
    {
        print_error("File size is negative: %l.", result);
        return EXIT_FAILURE;
    }
    *size = result;

    return EXIT_SUCCESS;
}

/**
 * /brief Checks if response field is correct.
 *
 * /param amount of bytes in parse_buf.
 * /param text to be checked against parse_buf.
 * /param parse_buf where to find text.
 *
 * /return EXIT_SUCCESS if text is in response, else EXIT_FAILURE.
 */
int check_text(size_t amount, char* text, char* parse_buf)
{
    size_t len;
    len = strlen(text);

    /* it must contain the status= string now */
    if (amount < (len + 2))
    {
        /* message is too short */
        print_error("Malformed response (too short %s).", text);
        EXIT_FAILURE;
    }
    if (strncmp(text, parse_buf, len) != 0)
    {
        print_error("Malformed response (no %s).", text);
        EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

