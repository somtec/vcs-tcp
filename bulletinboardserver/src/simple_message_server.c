/**
 * @file simple_message_server.c
 * Verteilte Systeme
 * TCP/IP Programmieruebung
 *
 * TCP/IP Server
 *
 * @author Daniel Helm  - 1310258026 - <daniel.helm@technikum-wien.at>
 * @author Daniel Kluka - 1310258060 - <daniel.kluka@technikum-wien.at>
 * @date 2013/12/09
 */

#include <assert.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LISTEN_BACKLOG 10
#define LOGIC_FILENAME "simple_message_server_logic"
#define LOGIC_FILEPATH "/usr/local/bin/simple_message_server_logic"

/* Function Prototypes */

/**
 * \brief should provide an information for the user to show which options are available
 * Moreover, this function is necessary for function smc_parsecommandline()
 *
 */
void printUsage();

/**
 * \brief filters the server port number from command line
 *
 * \param argc - command line argument count
 * \param argv - command line argument variables
 * \param portString - port number that is read from command line in function
 * \return 0 in case of success, in any other case abort program
 */
int parseCommandLineArguments(int argc, char** argv, char** portString);

/**
 * \brief generates a structure that contains information about localhost and the port number
 *
 * \param addressInfoRoot - Pointer to first element of the structure that contains the information
 * \param portString - port number
 * \return 0 in case of success, in any other case abort program
 */
int getAddressInfo(struct addrinfo** addressInfoRoot, char* portString);

/**
 * \brief manages the child process, duplicates sockets and close sockets that are not needed any more
 *
 * \param socketdescriptor2 - listening socket
 * \param new_socketdescriptor2 - connected socket to client
 */
void runChildProcess(int socketdescriptor2, int new_socketdescriptor2);

/**
 * \brief function to remove process entries in order to prevent zombie processes
 * called by the signal handler
 *
 */
void sigchld_handler();


/* globale Variablen */
char* programFilePath;


int main(int argc, char* argv[]) {
	char* port = NULL;
	int socketdescriptor = 0;
	int status_bind = 0;
	int status_listen = 0;
	socklen_t addresslen = sizeof(struct sockaddr_in);
	int new_socketdescriptor = 0;
	int status_parse = 0;
	int status_addrinfo = 0;
	int yes=1;

	struct addrinfo* addressinforoot = NULL;
	struct addrinfo* p = NULL;
	struct sockaddr_in address;
	struct sigaction sa;

	programFilePath = *argv;

	/* Argumente der Commandline einlesen */
	status_parse = parseCommandLineArguments(argc, argv, &port);

	if (status_parse == -1) {
		/* Fehler bei Formatierung */
		/* Error Message wird bereits in Unterfunktion getAddressInfo ausgegeben */
		exit(EXIT_FAILURE);
	}

    /*
     * ### FB_CF: Also hier getaddrinfo() zu verwenden bringt's nicht so
     *             wirklich.
     */

	status_addrinfo = getAddressInfo(&addressinforoot, port);

	if (status_addrinfo == -1) {
		/* Error Message wird bereits in Unterfunktion getAddressInfo ausgegeben */
		exit(EXIT_FAILURE);
	}

	for (p = addressinforoot; p != NULL; p = p->ai_next)
	{
		socketdescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		/* Socketnummer wird im Gutfall als Wert auf socketdescriptor geschrieben
		Bei Fehler wird -1 rueckgegeben */
		if (socketdescriptor == -1) {
		    /*
		     * ### FB_CF: Fehlermeldungen sollten auf stderr
		     */
		    /*
		     * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0] [-2]
		     */
			printf ("Fehler bei Create Socket\n");
			continue;
		}

		if (setsockopt(socketdescriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		    /*
		     * ### FB_CF: Fehlermeldungen sollten auf stderr
		     */
		    /*
		     * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0]
		     */
			printf ("Fehler beim Konfigurieren des Sockets\n");
			/*
			 * ### FB_CF: Ressourceleak (freeaddrinfo() fehlt) [-2]
			 */
			exit(EXIT_FAILURE);
		}

		/* Bind to Socket */
		status_bind = bind(socketdescriptor, p->ai_addr, p->ai_addrlen);

		/* Funktion bind liefert 0 zurueck wenn Port frei
		Wenn Port bereits belegt, dann wird Wert ungleich 0 zurueckgeliefert */
		if (status_bind != 0) {
		    /*
		     * ### FB_CF: Fehlermeldungen sollten auf stderr
		     */
		    /*
		     * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0]
		     */
			printf ("Fehler beim Binden zum Socket - Port nicht frei\n");
			close(socketdescriptor);
			continue;
		}
		break;
	}

	if (p == NULL) {
	    /*
	     * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0]
	     */
		fprintf(stderr, "server: failed to bind\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * ### FB_CF: Ressourceleak (freeaddrinfo() fehlt) [-2]
	 */

	/* Maximale Groesse des Puffers fuer wartende Anfragen definieren
	Verbindung immer nur zu einem Client moeglich */
	status_listen = listen(socketdescriptor, LISTEN_BACKLOG);

	/* Wenn listen -1 liefert, dann ist ein Fehler beim Erstellen passiert */
	if (status_listen == -1) {
	    /*
	     * ### FB_CF: Fehlermeldungen sollten auf stderr
	     */
	    /*
	     * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0]
	     */
		printf ("Fehler beim Definieren der Warteschlangengroesse\n");
		close(socketdescriptor);
		exit(EXIT_FAILURE);
	}

	/*printf ("Server wartet auf Anfrage des Clients\n");*/

	/* register a signal handler (sigchld_handler) => is called child process stops */
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

 	while (1) {
		/* Warten auf Client-Verbindung */
		new_socketdescriptor = accept(socketdescriptor, (struct sockaddr *) &address, &addresslen);

		/*printf ("Server hat Client gefunden\n");*/

		/* Wenn accept einen Wert kleiner als 1 liefert, so gab es einen Fehler beim Verbindungsaufbau */
		if (new_socketdescriptor < 1)
		{
		    /*
		     * ### FB_CF: Fehlermeldungen sollten auf stderr
		     */
		    /*
		     * ### FB_CF: Fehlermeldungen beinhalten nicht argv[0]
		     */
			printf ("Fehler beim Accepten der Verbindung\n");
		}

		switch (fork())
		{
			case -1:
				/* Fehler: erst Fehlermeldung ausgeben, danach Connected-Socket schließen. */
				(void) fprintf(stderr, "%s: error forking server\n", *argv);
				break;
			case 0:
				/* Kind-Prozess */
				/*(void) fprintf(stderr, "%s: successful forking server\n", *argv);*/
				runChildProcess(socketdescriptor, new_socketdescriptor);
				/* Dieser Punkt darf nicht mehr erreicht werden. */
				assert(0);
				break;
			default:
				/* Vater-Prozess: Connected-Socket schließen. */
				break;
		}
		close(new_socketdescriptor);
	}

	/*
	 * ### FB_CF: Dead Code
	 */

	close(socketdescriptor);
	exit(EXIT_SUCCESS);
}


/* subfunctions */

int parseCommandLineArguments(int argc, char *argv[], char** portString)
{
	int optioncharacter = 0;
	unsigned long local_port_number;

	*portString = NULL;

	/* Nach dem Paramter -p in der Commandline-Eingabe suchen */
	while ((optioncharacter = getopt(argc, argv, "p:")) != -1)
	{
		switch (optioncharacter)
		{
		case 'p':
			*portString = optarg;
			break;
		case 'h':
		case '?':
		default:
			/* Fehlermeldung wurde durch getopt_long ausgegeben. */
			printUsage();
			/* Es wird hier kein Exit durchgefuehrt, wird im Hauptprogramm, wo der Rueckgabewert abgefragt wird, durchgefuehrt */
			return -1;
		}
	}

	if (*portString == NULL)
	{
		printUsage();
		/* Es wird hier kein Exit durchgefuehrt, wird im Hauptprogramm, wo der Rueckgabewert abgefragt wird, durchgefuehrt */
		return -1;
	}

	/* umwandeln in unsigned long */
	local_port_number = (unsigned long) strtoul(*portString, NULL, 0);

	/* Port muss innerhalb der Grenzen liegen */
	if (local_port_number < 1024 || local_port_number > 65535) {
		printUsage();
		/* Es wird hier kein Exit durchgefuehrt, wird im Hauptprogramm, wo der Rueckgabewert abgefragt wird, durchgefuehrt */
		return -1;
	}
	return 0;
}

int getAddressInfo(struct addrinfo** addressInfoRoot, char* portString)
{
	int addressinfoerror = 0;
	struct addrinfo addressinfohints;

	memset(&addressinfohints, 0, sizeof(addressinfohints));
	addressinfohints.ai_family = AF_INET;
	addressinfohints.ai_socktype = SOCK_STREAM;
	addressinfohints.ai_flags = AI_PASSIVE;

	if ((addressinfoerror = getaddrinfo(NULL, portString, &addressinfohints, addressInfoRoot)) != 0)
	{
		fprintf(stderr, "%s: error getting address info. %s\n", programFilePath, gai_strerror(addressinfoerror));
		/* Es wird hier kein Exit durchgefuehrt, wird im Hauptprogramm, wo der Rueckgabewert abgefragt wird, durchgefuehrt */
		return -1;
	}

	return 0;
}

void printUsage() {
	fprintf(stderr, "%s:\n", programFilePath);
	fprintf(stderr, "usage: simple_message_server option\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\t-p, --port <port>\n");
	fprintf(stderr, "\t-h, --help\n");
	fprintf(stderr, "TCP Port Range = 1024 - 65535\n");
}

void sigchld_handler()
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void runChildProcess(int socketdescriptor2, int new_socketdescriptor2)
{
	/* Im Kind-Prozess kann das Listening socket geschlossen werden, wird nicht benoetigt */
	if (close(socketdescriptor2) == -1)
	{
		(void) fprintf(stderr, "%s: error closing child listening socket descriptor\n", programFilePath);
		(void) close(new_socketdescriptor2);
		exit(EXIT_FAILURE);
	}

	/* Dupliziere das bestehende Socket und verbinden mit dem Default-standard-input-file-descriptor der auch von read lesen kann (stdin kann das nicht) */
	if (dup2(new_socketdescriptor2, STDIN_FILENO) == -1)
	{
		(void) fprintf(stderr, "%s: error duplicating child connected socket descriptor to STDIN_FILENO\n", programFilePath);
		(void) close(new_socketdescriptor2);
		exit(EXIT_FAILURE);
	}

	/* Dupliziere das bestehende Socket und verbinden mit dem Default-standard-output-file-descriptor der auch mit write schreiben kann (stdout kann das nicht) */
	if (dup2(new_socketdescriptor2, STDOUT_FILENO) == -1)
	{
		(void) fprintf(stderr, "%s: error duplicating child connected socket descriptor to STDOUT_FILENO\n", programFilePath);
		(void) close(new_socketdescriptor2);
		(void) close(STDIN_FILENO);
		exit(EXIT_FAILURE);
	}

	/* Schliessen des bestehenden Socket, da im Kind-Prozess nicht mehr benoetigt */
	if (close(new_socketdescriptor2) == -1)
	{
		(void) fprintf(stderr, "%s: error closing child connected socket descriptor\n", programFilePath);
		(void) close(STDIN_FILENO);
		(void) close(STDOUT_FILENO);
		exit(EXIT_FAILURE);
	}

	/*(void) fprintf(stderr, "%s: OK\n", programFilePath);*/

	/* Prozess-Image überlagern. */
	execlp("simple_message_server_logic", "simple_message_server_logic", NULL);

	/* Sollte nicht ausgeführt werden, da stattdessen die Shell läuft. Wenn doch, Prozess mit Fehler beenden. */
	exit(EXIT_FAILURE);
}
