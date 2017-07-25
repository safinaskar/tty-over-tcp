#define _POSIX_C_SOURCE 200900L

#include <string.h>

#include <libsh.h>

#include "tcp.h"

int mini_listen_plus (const char *port)
{
  if (*port == '+'){
    return sh_tcp_listen (port + 1, AF_INET6);
  }else
    {
      return sh_tcp_listen (port, AF_INET);
    }
}

int mini_accept_plus (const char *port)
{
  if (*port == '+'){
    return sh_tcp_accept_close (sh_tcp_listen (port + 1, AF_INET6));
  }else
    {
      return sh_tcp_accept_close (sh_tcp_listen (port, AF_INET));
    }
}

int mini_connect_plus (const char *host, const char *port)
{
  if (*port == '+'){
    return sh_tcp_connect (host, port + 1, AF_INET6);
  }else
    {
      return sh_tcp_connect (host, port, AF_INET);
    }
}

/* хорошо бы добавить проверку на logical-совместимость */
int mini_tcp_client_or_server_plus (char ***pargv)
{
  if (strcmp ((*pargv)[0], "tcp-client") == 0){
    int result = mini_connect_plus ((*pargv)[1], (*pargv)[2]);
    *pargv += 3;
    return result;
  }else if (strcmp ((*pargv)[0], "tcp-server") == 0){
    int result = mini_accept_plus ((*pargv)[1]);
    *pargv += 2;
    return result;
  }else
    {
      sh_warnx ("no tcp-client or tcp-server");
      return -1;
    }
}
