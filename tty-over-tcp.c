/* -lutil */
/* Что будет, если одна из сторон завершится? */
/* ^] обрабатывается на стороне логического сервера, что не логично */
/* Если прибить logical-server с помощью Ctrl-C (независимо от tcp-серверности, проверено), то logical-client падает без вывода всякого сообщения об ошибке */
/* В каких случаях нужно печатать \n перед сообщением? */
/* Баг: запускаем logical-client, в нём mc, выходим, выходим из logical-client, размеры терминала по-прежнему неправильные, русский не работает */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <termios.h>

#include <pty.h>

#include <err.h>

#include <libsh.h>

#include "tcp.h"

/* tcp-сервер подключает только одного клиента */

bool logical_server;
int sock;
int master;

void my_log(const char *message)
{
  if (!logical_server)
    {
      fprintf(stderr, "logical-client: I'm not logical-server, but my_log(...) is called\r\n");
      return;
    }

  fprintf(stderr, "logical-server: %s\n", message);
  dprintf(sock, "logical-server: %s\r\n", message);
}

void chld_handler(int unused)
{
  /*if (verbose){
    fprintf(logfp, "[CLD] Child terminated\n");
  }*/

  for (;;){
    char c;
    int read_returned = read(master, &c, 1);

    if (read_returned == -1){
      /* Child closed tty */
      /*if (verbose){
        fprintf(logfp, "[EOF] Child  stdout -> parent stdout\n");
      }*/
      break;
    }else if (read_returned == 0){
      my_log("cannot read from master: EOF");
      exit (EXIT_FAILURE);
    }

    /*if (verbose){
      fprintf(logfp, "[ %c ] Child  stdout -> parent stdout\n", c);
    }*/

    switch (write(sock, &c, 1)){
      case -1:
        my_log(strerror(errno));
        my_log("cannot write to socket");
        exit (EXIT_FAILURE);
      case 0:
        my_log("write(sock) returned 0");
        exit (EXIT_FAILURE);
    }
  }

  my_log("child terminated, calling wait(...)...");

  int status;

  if (wait(&status) == -1)
    {
      my_log(strerror(errno));
      my_log("wait");
      exit (EXIT_FAILURE);
    }

  my_log("done, terminating");

  if (WIFEXITED(status)){
    exit (WEXITSTATUS(status));
  }else if (WIFSIGNALED(status)){
    exit (EXIT_FAILURE);
  }else
    {
      my_log("child terminated in unusual way");
      exit (EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
  sh_init (argv[0]);

  if (argc < 4){
    fprintf(stderr,
      "Usage:\n"
      "tty-over-tcp tcp-server [+]PORT      logical-server COMMAND [ARG]...\n"
      "tty-over-tcp tcp-server [+]PORT      logical-client\n"
      "tty-over-tcp tcp-client HOST [+]PORT logical-server COMMAND [ARG]...\n"
      "tty-over-tcp tcp-client HOST [+]PORT logical-client\n"
    );
    exit (EXIT_FAILURE);
  }

  ++argv;

  fprintf(stderr, "logical-client-or-server: establishing connection...\n");

  sock = mini_tcp_client_or_server_plus(&argv);

  if (sock == -1)
    {
      exit (EXIT_FAILURE);
    }

  fprintf(stderr, "logical-client-or-server: done\n");

  if (strcmp(argv[0], "logical-server") == 0){
    logical_server = true;
  }else if (strcmp(argv[0], "logical-client") == 0){
    logical_server = false;
  }else
    {
      fprintf(stderr, "Usage\n");
      exit (EXIT_FAILURE);
    }

  /* На всякий случай */
  setvbuf (stdin,  NULL, _IONBF, 0);
  setvbuf (stdout, NULL, _IONBF, 0);
  setvbuf (stderr, NULL, _IONBF, 0);

  if (logical_server){
    /* logical-server */

    FILE *fsock = fdopen(sock, "r");

    setvbuf (fsock, NULL, _IONBF, 0);

    char *term = NULL;

    {
      size_t term_size = 0;

      if (getdelim(&term, &term_size, '\0', fsock) == -1){
        my_log("cannot read TERM");
        exit (EXIT_FAILURE);
      }
    }

    my_log("calling forkpty(...)...");
    pid_t pid = forkpty(&master, NULL, NULL, NULL);

    switch (pid){
      case -1:
        my_log(strerror(errno));
        my_log("is /dev/pts mounted?");
        exit (EXIT_FAILURE);
      case 0:
        /* Child */
        close(sock);
        setenv("TERM", term, 1 /* Overwrite */);
        execvp(argv[1], argv + 1);
        my_log(strerror(errno));
        exit (EXIT_FAILURE);
    }

    my_log("done, calling signal...");

    /* Parent */
    /* Copied from my program "pty" */
    if (signal(SIGCHLD, &chld_handler) == SIG_ERR){
      my_log(strerror(errno));
      my_log("signal");
      exit (EXIT_FAILURE);
    }

    my_log("done, working (press ^] to exit)");

    bool eof_send = false;

    for (;;){
      fd_set set;

      FD_ZERO(&set);
      FD_SET(sock, &set);
      FD_SET(master, &set);

      if (select(100, &set, NULL, NULL, NULL) == -1){
        if (errno == EINTR){
          continue;
        }else{
          my_log(strerror(errno));
          my_log("select");
          exit (EXIT_FAILURE);
        }
      }

      if (FD_ISSET(sock, &set)){
        char c;
        int read_returned = read(sock, &c, 1);

        if (read_returned == -1){
          my_log(strerror(errno));
          my_log("cannot read from socket");
          exit (EXIT_FAILURE);
        }else if (read_returned == 0){
          /*if (verbose && !eof_send){
            fprintf(logfp, "[EOF] Parent stdin  -> child  stdin\n");
            eof_send = true;
          }*/
        }else if (c == 29 /* ^] */){
          my_log("^], exiting");
          exit (EXIT_SUCCESS);
        }else{
          /*if (verbose){
            fprintf(logfp, "[ %c ] Parent stdin  -> child  stdin\n", c);
          }*/

          switch (write(master, &c, 1)){
            case -1:
              my_log(strerror(errno));
              my_log("cannot write to master");
              exit (EXIT_FAILURE);
            case 0:
              my_log("write(master) returned 0");
              exit (EXIT_FAILURE);
          }
        }
      }

      if (FD_ISSET(master, &set)){
        char c;
        int read_returned = read(master, &c, 1);

        if (read_returned == -1){
          /* Child closed tty */
          /*if (verbose){
            fprintf(logfp, "[EOF] Child  stdout -> parent stdout\n");
          }*/
          continue;
        }else if (read_returned == 0){
          my_log("cannot read from master: EOF");
          exit (EXIT_FAILURE);
        }

        /*if (verbose){
          fprintf(logfp, "[ %c ] Child  stdout -> parent stdout\n", c);
        }*/

        switch (write(sock, &c, 1)){
          case -1:
            my_log(strerror(errno));
            my_log("cannot write to socket");
            exit (EXIT_FAILURE);
          case 0:
            my_log("write(sock) returned 0");
            exit (EXIT_FAILURE);
        }
      }
    }

    /* NOTREACHED */
  }else{
    /* logical-client */

    const char *term = getenv("TERM");

    if (term == NULL)
      {
        term = "";
      }

    if (*term == '\0')
      {
        fprintf(stderr, "logical-client: warning: TERM is empty or not set\n");
      }

    if (write(sock, term, strlen(term) + 1) != strlen(term) + 1){
      fprintf(stderr, "logical-client: error: cannot write TERM to socket\n");
      exit (EXIT_FAILURE);
    }

    fprintf(stderr, "logical-client: preparing terminal...\n");

    {
      struct termios term;
      tcgetattr(fileno(stdin), &term);

      // Copied from man cfmakeraw (I cannot write cfmakeraw, because Interix has no this function)
      term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
        ICRNL | IXON);
      term.c_oflag &= ~OPOST;
      term.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
      term.c_cflag &= ~(CSIZE | PARENB);
      term.c_cflag |= CS8;
      // End of man

      tcsetattr(fileno(stdin), 0, &term);
    }

    fprintf(stderr, "logical-client: done, working\r\n");

    {
      struct sh_multicat_t m[2] = {{0}};

      m[0].src = 0;
      m[0].dst = sock;
      m[0].flags = sh_rclose | sh_wshutdownw;

      m[1].src = sock;
      m[1].dst = 1;
      m[1].flags = sh_wclose;

      sh_multicat (m, 2);
    }

    {
      struct termios term;
      tcgetattr(fileno(stdin), &term);

      // Copied from man cfmakeraw (I cannot write cfmakeraw, because Interix has no this function)
      term.c_iflag |= (IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
        ICRNL | IXON);
      term.c_oflag |= OPOST;
      term.c_lflag |= (ECHO | ECHONL | ICANON | ISIG | IEXTEN);
      term.c_cflag |= (CSIZE | PARENB);
      term.c_cflag &= ~CS8;
      // End of man

      tcsetattr(fileno(stdin), 0, &term);
    }

    exit (EXIT_SUCCESS);

    /* NOTREACHED */
  }

  exit (EXIT_SUCCESS); /* For compiler */
}
