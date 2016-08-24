/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* POSIX */
#include <unistd.h>
#include <fcntl.h>

/* sockets */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

static const char ircd_addr[] = "127.0.0.1";
static const int ircd_port = 6667;
static const int debug = 1;

enum irc_states {
    IRC_CONNECTING,
    IRC_CONNECTED
};

void irc_send(
    int sock,
    char *msg
) {
    int n;

    if (debug)
        fprintf(stderr, "sending to ircd: %s", msg);

    n = write(sock, msg, strlen(msg));

    if (n < 0) {
        perror("error writing to socket");
        exit(1);
    }
}

int irc_connect() {
    int sock;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("error opening socket");
        exit(1);
    }

    server = gethostbyname(ircd_addr);

    if (!server) {
        fprintf(stderr, "error: no such host\n");
        exit(1);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    bcopy(
        (char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length
    );

    serv_addr.sin_port = htons(ircd_port);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("error connecting");
        exit(1);
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
        perror("error setting non-blocking on tcp socket");
        exit(1);
    }

    return sock;
}

int get_line(           /* return 1 if line was read */
    int fd,             /* file descriptor to read from */
    char *s,            /* buffer to place line in */
    char *buf,          /* buffer for unprocessed lines */
    size_t buf_size     /* size of buf */
) {
    char buf2[2048];
    char *q;
    int n;

    if ((n = read(fd, buf2, buf_size - strlen(buf) - 1)) > 0) {
        buf2[n] = '\0';
        strcat(buf, buf2);
    }

    if (q = strchr(buf, '\n')) {
        /* If the line is terminated with \r\n then strip the carriage
           return as well. */
        if (*(q - 1) == '\r') {
            *(q - 1) = '\0';
            strcpy(s, buf);
        }

        else {
            *q = '\0';
            strcpy(s, buf);
        }

        /* Shift the rest of the string to the left. */
        memmove(buf, q + 1, strlen(q + 1) + 1);

        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    enum irc_states state = IRC_CONNECTING;
    int irc_sock;               /* file descriptor for IRC connection */
    int n;
    char line[2048];
    char buf[2048];
    char irc_buf[2048] = "";    /* stores unprocessed lines from IRC server */
    char stdin_buf[2048] = "";  /* stores unprocessed lines from stdin*/

    if (argc < 3) {
        fprintf(stderr, "usage: %s nick channel\n", argv[0]);
        return 0;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0) {
        perror("error setting non-blocking on stdin");
        exit(1);
    }

    irc_sock = irc_connect();

    /* Identify to the server. */
    sprintf(buf, "USER %s 0 * :%s\r\n", argv[1], argv[1]);
    irc_send(irc_sock, buf);
    sprintf(buf, "NICK %s\r\n", argv[1]);
    irc_send(irc_sock, buf);

    while (1) {
        /* While there's lines from the IRC server to process... */
        while (get_line(irc_sock, line, irc_buf, sizeof(irc_buf))) {
            if (debug)
                fprintf(stderr, "received from ircd: \"%s\"\n", line);

            switch (state) {
            case IRC_CONNECTING:
                if (strstr(line, "NOTICE Auth")) {
                    state = IRC_CONNECTED;

                    if (debug)
                        fprintf(stderr, "connected!\n");
                }
                break;

            case IRC_CONNECTED:
                /* Respond to any pings from the server. */
                if (strstr(line, "PING :") == line) {
                    sprintf(buf, "PONG :%s\r\n", &line[6]);
                    irc_send(irc_sock, buf);
                }
                break;

            default:
                break;
            }
        }

        if (state != IRC_CONNECTING) {
            while (get_line(STDIN_FILENO, line, stdin_buf, sizeof(stdin_buf))) {
                if (debug)
                    fprintf(stderr, "received from stdin: \"%s\"\n", line);

                switch (state) {
                case IRC_CONNECTED:
                    sprintf(buf, "NAMES #%s\r\n", argv[2]);
                    irc_send(irc_sock, buf);

                    while (1) {
                        char names[1024];   /* store the results of /NAMES */
                        char *p;            /* point to the beginning of the list of names */

                        if (get_line(irc_sock, names, irc_buf, sizeof(irc_buf))) {
                            if (strstr(names, "End of /NAMES list."))
                                break;

                            sprintf(buf, "#%s", argv[2]);

                            if (p = strstr(names, buf)) {
                                char *q;    /* point to a name */

                                p += strlen(buf) + 2;
                                q = strtok(p, " ");

                                while (1) {
                                    if (debug)
                                        fprintf(stderr, "found nick: \"%s\"\n", q);

                                    /* Strip any mode symbols from the name. */
                                    if (
                                        *q == '~' ||
                                        *q == '&' ||
                                        *q == '@' ||
                                        *q == '%'
                                    )
                                        q++;

                                    sprintf(buf, "PRIVMSG %s :%s\r\n", q, line);
                                    irc_send(irc_sock, buf);

                                    if (!(q = strtok(NULL, " ")))
                                        break;
                                }
                            }
                        }
                    }
                    break;

                default:
                    break;
                }
            }
        }

        sleep(1);
    }

    irc_send(irc_sock, "QUIT\r\n");
    close(irc_sock);

    return 0;
}
