/*****************************************************************
 
  socket.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <netdb.h> /* gethostbyname */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utils.h>
#include <bgsocket.h>

/* Opaque address structure so we can support IPv6 in the future */

struct bg_host_address_s 
  {
  int addr_type; /* AF_INET or AF_INET6 */

  union
    {
    struct in_addr  ipv4_addr;
    struct in6_addr ipv6_addr;
    } addr;
  
  int port;
  };

bg_host_address_t * bg_host_address_create()
  {
  bg_host_address_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bg_host_address_destroy(bg_host_address_t * a)
  {
  free(a);
  }

/* This must be extended for other protocols */

static struct
  {
  char * name;
  int    port;
  } default_ports[] =
  {
    { "http://", 80 },
    { (char*)0,   0 },
  };

/*
 *  Get the default port, return -1 if no
 *  registered protocol is found
 */

int get_default_port(const char * url)
  {
  int index;
  index = 0;
  while(default_ports[index].name)
    {
    if(!strncmp(url, default_ports[index].name,
                strlen(default_ports[index].name)))
      return default_ports[index].port;
    }
  return -1;
  }


/* gethostbyname */

static int hostbyname(bg_host_address_t * a, const char * hostname)
  {
  struct hostent   h_ent;
  struct hostent * h_ent_p;
  int gethostbyname_buffer_size;

  char * gethostbyname_buffer = (char*)0;
  int result, herr;
  int ret = 0;
  
  gethostbyname_buffer_size = 1024;
  gethostbyname_buffer = malloc(gethostbyname_buffer_size);

  fprintf(stderr, "Resolving host %s\n", hostname);
    
  while((result = gethostbyname_r(hostname,
                                  &h_ent,
                                  gethostbyname_buffer,
                                  gethostbyname_buffer_size,
                                  &h_ent_p, &herr)) == ERANGE)
    {
    gethostbyname_buffer_size *= 2;
    gethostbyname_buffer = realloc(gethostbyname_buffer,
                                  gethostbyname_buffer_size);
    }
  
  /* Fill in return value           */

  if(result || (h_ent_p == NULL))
    {
    fprintf(stderr, "Could not resolve address\n");
    goto fail;
    }
  if(h_ent_p->h_addrtype == AF_INET)
    {
    a->addr_type = AF_INET;
    memcpy(&(a->addr.ipv4_addr),
           h_ent_p->h_addr, sizeof(a->addr.ipv4_addr));
    }
  else if(h_ent_p->h_addrtype == AF_INET6)
    {
    a->addr_type = AF_INET6;
    memcpy(&(a->addr.ipv6_addr),
           h_ent_p->h_addr, sizeof(a->addr.ipv6_addr));
    }
  else
    {
    fprintf(stderr, "No known address space\n");
    goto fail;
    }
  fprintf(stderr, "Done\n");
  ret = 1;

  fail:
  
  if(gethostbyname_buffer)
    free(gethostbyname_buffer);
  return ret;
  }

/* Set the address from an URL */

int bg_host_address_set_from_url(bg_host_address_t * a, const char * url,
                                const char ** rest)
  {
  const char * pos1;
  const char * pos2;
  char * hostname = (char*)0;
  int ret = 0;
  
  /* First step: Get the default port */

  a->port = get_default_port(url);
  if(a->port == -1)
    {
    fprintf(stderr, "No registered protocol\n");
    return 0;
    }

  /* Second step: Parse the URL      */

  pos1 = strstr(url, "://");
  pos1 += 3;
  
  if(!isalnum(*pos1))
    {
    fprintf(stderr, "Invalid hostname\n");
    return 0;
    }

  pos2 = pos1;
  
  while((*pos2 != '\0') && (*pos2 != '/') && (*pos2 != ':'))
    pos2++;

  hostname = bg_strndup((char*)0, pos1, pos2);

  /* Parse the port */

  if(*pos2 == ':')
    {
    pos2++;
    a->port = atoi(pos2);
    while(isdigit(*pos2))
      pos2++;
    }

  if(!hostbyname(a, hostname))
    {
    goto fail;
    }

  ret = 1;
  if(rest)
    *rest = pos2;
  
  /* Cleanup */
  
  fail:
  
  if(hostname)
    free(hostname);
  return ret;
  }

int bg_host_address_set(bg_host_address_t * a, const char * hostname,
                        int port)
  {
  if(!hostbyname(a, hostname))
    return 0;
  a->port = port;
  return 1;
  }

/* Client connection (stream oriented) */

int bg_socket_connect_inet(bg_host_address_t * a, int milliseconds)
  {
  int ret = -1;
  struct sockaddr_in  addr_in;
  struct sockaddr_in6 addr_in6;
  void * addr;
  int addr_len;
  struct timeval timeout;
  fd_set write_fds;
  
  /* Create the socket */
  
  if((ret = socket (PF_INET, SOCK_STREAM, 0)) < 0)
    {
    fprintf(stderr, "Cannot create socket\n");
    return -1;
    }

  /* Set up address */

  if(a->addr_type == AF_INET)
    {
    addr = &addr_in;
    addr_len = sizeof(addr_in);
    
    memset(&addr_in, 0, sizeof(addr_len));

    addr_in.sin_family = AF_INET;
    memcpy(&(addr_in.sin_addr),
           &(a->addr.ipv4_addr),
           sizeof(a->addr.ipv4_addr));
    addr_in.sin_port = htons(a->port);
    }
  else if(a->addr_type == AF_INET6)
    {
    addr = &addr_in6;
    addr_len = sizeof(addr_in6);

    memset(&addr_in6, 0, addr_len);
    addr_in6.sin6_family = AF_INET6;
    memcpy(&(addr_in6.sin6_addr),
           &(a->addr.ipv6_addr),
           sizeof(a->addr.ipv6_addr));
    addr_in6.sin6_port = htons(a->port);
    }
  
  /* Connect the thing */

  fprintf(stderr, "Connecting...");

  if(fcntl(ret, F_SETFL, O_NONBLOCK) < 0)
    {
    fprintf(stderr, "Cannot set nonblocking mode\n");
    return -1;
    }

  if(connect(ret,(struct sockaddr*)addr,addr_len)<0)
    {
    if(errno == EINPROGRESS)
      {
      timeout.tv_sec = milliseconds / 1000;
      timeout.tv_usec = 1000 * (milliseconds % 1000);
      FD_ZERO (&write_fds);
      FD_SET (ret, &write_fds);
      if(!select(ret+1, (fd_set*)0, &write_fds,(fd_set*)0,&timeout))
        {
        fprintf(stderr, "Connection timed out\n");
        return -1;
        }
      }
    else
      {
      fprintf(stderr, "Failed\n");
      return -1;
      }
    }
  fprintf(stderr, "Connected\n");
  
  /* Set back to blocking mode */
  
  if(fcntl(ret, F_SETFL, 0) < 0)
    {
    fprintf(stderr, "Cannot set blocking mode\n");
    return -1;
    }
  return ret;
  }

int bg_socket_connect_unix(const char * name)
  {
  struct sockaddr_un addr;
  int addr_len;
  int ret;
  ret = socket(PF_LOCAL, SOCK_STREAM, 0);
  if(ret < 0)
    {
    fprintf(stderr, "Cannot create socket\n");
    return -1;
    }

  addr.sun_family = AF_LOCAL;
  strncpy (addr.sun_path, name, sizeof(addr.sun_path));
  addr.sun_path[sizeof (addr.sun_path) - 1] = '\0';

  addr_len = SUN_LEN(&addr);

  fprintf(stderr, "Connecting...");
  
  if(connect(ret,(struct sockaddr*)(&addr),addr_len)<0)
    {
    fprintf(stderr, "failed\n");
    return -1;
    }
  return ret;
  }

void bg_socket_disconnect(int sock)
  {
  close(sock);
  }

/* Server socket (stream oriented) */

int bg_listen_socket_create_inet(int port,
                                 int queue_size)
  {
  int ret;
  struct sockaddr_in name;
  
  /* Create the socket. */
  ret = socket (PF_INET, SOCK_STREAM, 0);
  if (ret < 0)
    {
    fprintf(stderr, "Cannot create socket\n");
    return -1;
    }
  
  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (ret, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
    fprintf(stderr, "Cannot bind socket\n");
    return -1;
    }
  if(fcntl(ret, F_SETFL, O_NONBLOCK) < 0)
    {
    fprintf(stderr, "Cannot set nonblocking mode\n");
    return -1;
    }
  if(listen(ret, queue_size))
    {
    fprintf(stderr, "Cannot put socket into listening mode\n");
    return -1;
    }
  return ret;
  }

int bg_listen_socket_create_unix(const char * name,
                                 int queue_size)
  {
  int ret;

  struct sockaddr_un addr;
  int addr_len;
  ret = socket(PF_LOCAL, SOCK_STREAM, 0);
  if(ret < 0)
    {
    fprintf(stderr, "Cannot create socket\n");
    return -1;
    }

  addr.sun_family = AF_LOCAL;
  strncpy (addr.sun_path, name, sizeof(addr.sun_path));
  addr.sun_path[sizeof (addr.sun_path) - 1] = '\0';
  addr_len = SUN_LEN(&addr);
  if(bind(ret,(struct sockaddr*)(&addr),addr_len)<0)
    {
    fprintf(stderr, "Could not bind socket\n");
    return -1;
    }
  if(fcntl(ret, F_SETFL, O_NONBLOCK) < 0)
    {
    fprintf(stderr, "Cannot set nonblocking mode\n");
    return -1;
    }
  if(listen(ret, queue_size))
    {
    fprintf(stderr, "Cannot put socket into listening mode\n");
    return -1;
    }
  return ret;
  }

/* Accept a new client connection, return -1 if there is none */

int bg_listen_socket_accept(int sock)
  {
  int ret;
  ret = accept(sock, NULL, NULL);
  if(ret < 0)
    return -1;
  return ret;
  }

void bg_listen_socket_destroy(int sock)
  {
  close(sock);
  }
