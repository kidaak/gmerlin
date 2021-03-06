/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include "gavf-server.h"
#include <string.h>
#include <errno.h>

#define LOG_DOMAIN "gavf-server.program"

#define BUF_ELEMENTS 30

static void program_remove_client_nolock(program_t * p, int idx);


static void set_status(program_t * p, int status)
  {
  pthread_mutex_lock(&p->status_mutex);
  p->status = status;
  pthread_mutex_unlock(&p->status_mutex);
  }

static int element_used(program_t * p, int64_t seq)
  {
  int i;
  int status;
  
  for(i = 0; i < p->num_clients; i++)
    {
    status = client_get_status(p->clients[i]);
    
    if(status == CLIENT_STATUS_DONE)
      continue;
    
    if(client_get_seq(p->clients[i]) == seq)
      return i;
    }
  return -1;
  }

static int program_iteration(program_t * p)
  {
  int i;
  gavl_time_t conn_time;
  gavl_time_t program_time;
  buffer_element_t * el;

  /* Check if we need to stop */
  if(program_get_status(p) == PROGRAM_STATUS_STOP)
    return 0;
  /* Kill dead clients */

  pthread_mutex_lock(&p->client_mutex);
  i = 0;
  while(i < p->num_clients)
    {
    if(client_get_status(p->clients[i]) == CLIENT_STATUS_DONE)
      {
      bg_log(BG_LOG_INFO, LOG_DOMAIN, 
             "Closing client connection for program %s (client thread finished)", p->name);
      program_remove_client_nolock(p, i);
      }
    else
      i++;
    }
  pthread_mutex_unlock(&p->client_mutex);

  /* We need at least 3 elements in the pool */

  while(buffer_get_free(p->buf) < 3)
    {
    el = buffer_get_first(p->buf);
    
    pthread_mutex_lock(&p->client_mutex);
    while((i = element_used(p, el->seq)) >= 0)
      {
      bg_log(BG_LOG_INFO, LOG_DOMAIN,
             "Closing client connection for program %s (client too slow)", p->name);
      program_remove_client_nolock(p, i);
      }
    pthread_mutex_unlock(&p->client_mutex);
    buffer_advance(p->buf);
    }
  /* Read packet from source */

  if(!bg_mediaconnector_iteration(&p->conn))
    return 0;

  /* Throw away old packets */

  pthread_mutex_lock(&p->client_mutex);
  while((el = buffer_get_first(p->buf)) &&
        (element_used(p, el->seq) < 0))
    buffer_advance(p->buf);
  pthread_mutex_unlock(&p->client_mutex);
    
  /* Delay */
  conn_time = bg_mediaconnector_get_min_time(&p->conn);
  program_time = gavl_timer_get(p->timer);

  if((conn_time != GAVL_TIME_UNDEFINED) &&
     (conn_time - program_time > GAVL_TIME_SCALE/2))
    {
    gavl_time_t delay_time;
    delay_time = conn_time - program_time - GAVL_TIME_SCALE/2;
    //    fprintf(stderr, "delay %"PRId64"\n", delay_time);
    gavl_time_delay(&delay_time);
    }
  
  return 1;
  }

static void * thread_func(void * priv)
  {
  program_t * p = priv;
  while(program_iteration(p))
    ;
  set_status(p, PROGRAM_STATUS_DONE);
  return NULL;
  }

static int conn_write_func(void * priv, const uint8_t * data, int len)
  {
  return len;
  }

static int conn_cb_func(void * priv, int type, const void * data)
  {
  buffer_element_t * el;
  program_t * p = priv;
  
  switch(type)
    {
    case GAVF_IO_CB_PROGRAM_HEADER_START:
      //      fprintf(stderr, "Got program header:\n");
      //      gavf_program_header_dump(data);

      if(!(p->flags & PROGRAM_HAVE_HEADER))
        {
        gavf_program_header_copy(&p->ph, data);
        p->flags |= PROGRAM_HAVE_HEADER;
        }
      break;
    case GAVF_IO_CB_PROGRAM_HEADER_END:
      break;
    case GAVF_IO_CB_PACKET_START:
      //      fprintf(stderr, "Got packet: %d\n", ((gavl_packet_t*)data)->id);
      //      gavl_packet_dump(data);

      el = buffer_get_write(p->buf);
      if(!el)
        return 0;
      
      buffer_element_set_packet(el, data);
      buffer_done_write(p->buf);
      
      break;
    case GAVF_IO_CB_PACKET_END:
      break;
    case GAVF_IO_CB_METADATA_START:
      //      fprintf(stderr, "Got metadata:\n");
      //      gavl_metadata_dump(data, 2);

      pthread_mutex_lock(&p->metadata_mutex);
      gavl_metadata_free(&p->m);
      gavl_metadata_init(&p->m);
      gavl_metadata_copy(&p->m, data);
      p->have_m = 1;
      pthread_mutex_unlock(&p->metadata_mutex);

      el = buffer_get_write(p->buf);
      if(!el)
        return 0;

      buffer_element_set_metadata(el, data);
      buffer_done_write(p->buf);

      break;
    case GAVF_IO_CB_METADATA_END:
      break;
    case GAVF_IO_CB_SYNC_HEADER_START:
      //      fprintf(stderr, "Got sync_header\n");

      if(!gavl_timer_is_running(p->timer))
        gavl_timer_start(p->timer);
      
      el = buffer_get_write(p->buf);
      if(!el)
        return 0;

      buffer_element_set_sync_header(el);
      buffer_done_write(p->buf);
      
      break;
    case GAVF_IO_CB_SYNC_HEADER_END:
      break;
    }
  
  return 1;
  }

program_t * program_create_from_socket(const char * name, int fd,
                                       const gavl_metadata_t * url_vars)
  {
  bg_plug_t * plug = NULL;
  gavf_io_t * io = NULL;
  int flags = 0;

  io = bg_plug_io_open_socket(fd, BG_PLUG_IO_METHOD_READ, &flags, CLIENT_TIMEOUT);
  if(!io)
    goto fail;
  
  plug = bg_plug_create_reader(plugin_reg);
  if(!plug)
    goto fail;

  if(!bg_plug_open(plug, io,
                   NULL, NULL, flags))
    goto fail;

  gavftools_set_stream_actions(plug);

  if(!bg_plug_start(plug))
    goto fail;

  return program_create_from_plug(name, plug, url_vars);
  
  fail:

  if(plug)
    bg_plug_destroy(plug);
  return NULL;
  
  }

program_t * program_create_from_plug(const char * name, bg_plug_t * plug,
                                     const gavl_metadata_t * url_vars)
  {
  gavf_io_t * io;
  program_t * ret;
  gavf_t * g;

  int buf_elements = BUF_ELEMENTS;
  
  ret = calloc(1, sizeof(*ret));

  ret->in_plug = plug;
  
  pthread_mutex_init(&ret->client_mutex, NULL);
  pthread_mutex_init(&ret->status_mutex, NULL);
  pthread_mutex_init(&ret->metadata_mutex, NULL);

  ret->name = gavl_strdup(name);

  gavl_metadata_get_int(url_vars, "buf", &buf_elements);
  
  ret->buf = buffer_create(buf_elements);
  ret->timer = gavl_timer_create();
  ret->status = PROGRAM_STATUS_RUNNING;
  
  /* Set up media connector */
  
  bg_plug_setup_reader(ret->in_plug, &ret->conn);
  bg_mediaconnector_create_conn(&ret->conn);
  
  /* Setup connection plug */
  io = gavf_io_create(NULL,
                      conn_write_func,
                      NULL,
                      NULL,
                      NULL,
                      ret);

  gavf_io_set_cb(io, conn_cb_func, ret);

  ret->conn_plug = bg_plug_create_writer(plugin_reg);
  
  bg_plug_open(ret->conn_plug, io,
               bg_plug_get_metadata(ret->in_plug), NULL, 0);

  bg_plug_transfer_metadata(ret->in_plug, ret->conn_plug);
  
  if(!bg_plug_setup_writer(ret->conn_plug, &ret->conn))
    goto fail;

  bg_mediaconnector_start(&ret->conn);

  g = bg_plug_get_gavf(ret->conn_plug);
  bg_log(BG_LOG_INFO, LOG_DOMAIN,
         "Created program: %s (AS: %d, VS: %d, TS: %d, OS: %d, buf: %d)",
         name,
         gavf_get_num_streams(g, GAVF_STREAM_AUDIO),
         gavf_get_num_streams(g, GAVF_STREAM_VIDEO),
         gavf_get_num_streams(g, GAVF_STREAM_TEXT),
         gavf_get_num_streams(g, GAVF_STREAM_OVERLAY), buf_elements);
  
  pthread_create(&ret->thread, NULL, thread_func, ret);
  
  return ret;
  
  fail:
  program_destroy(ret);
  return NULL;
  
  }


void program_destroy(program_t * p)
  {
  if(program_get_status(p) == PROGRAM_STATUS_RUNNING)
    set_status(p, PROGRAM_STATUS_STOP);

  if(pthread_join(p->thread, NULL))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Could not join thread: %s",
           strerror(errno));
    }

  if(p->in_plug)
    bg_plug_destroy(p->in_plug);

  if(p->conn_plug)
    bg_plug_destroy(p->conn_plug);

  bg_mediaconnector_free(&p->conn);
    
  if(p->buf)
    buffer_stop(p->buf);
  
  pthread_mutex_lock(&p->client_mutex);
  while(p->num_clients)
    {
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Closing client connection for program %s", p->name);
    program_remove_client_nolock(p, 0);
    }
  pthread_mutex_unlock(&p->client_mutex);
  
  
  if(p->name)
    free(p->name);
  if(p->timer)
    gavl_timer_destroy(p->timer);

  if(p->buf)
    buffer_destroy(p->buf);

  gavf_program_header_free(&p->ph);
  
  if(p->clients)
    free(p->clients);

  gavl_metadata_free(&p->m);
  
  pthread_mutex_destroy(&p->client_mutex);
  pthread_mutex_destroy(&p->status_mutex);
  pthread_mutex_destroy(&p->metadata_mutex);

  free(p);
  }

void program_attach_client(program_t * p, int fd,
                           const gavl_metadata_t * req,
                           gavl_metadata_t * res,
                           const gavl_metadata_t * url_vars)
  {
  client_t * cl;

  bg_log(BG_LOG_INFO, LOG_DOMAIN,
         "Got new client connection for program %s", p->name);

  pthread_mutex_lock(&p->metadata_mutex); 
  cl = client_create(fd, &p->ph, p->buf, req, res,
                     url_vars, p->have_m ? &p->m : NULL);
  pthread_mutex_unlock(&p->metadata_mutex);

  if(!cl)
    {
    bg_log(BG_LOG_INFO, LOG_DOMAIN,
           "Closed connection for program %s", p->name);
    
    return;
    }
  cl->seq = buffer_get_start_seq(p->buf);
  
  pthread_mutex_lock(&p->client_mutex);

    
  if(p->num_clients + 1 > p->clients_alloc)
    {
    p->clients_alloc += 16;
    p->clients = realloc(p->clients,
                         p->clients_alloc * sizeof(*p->clients));
    memset(p->clients + p->num_clients, 0,
           (p->clients_alloc - p->num_clients) * sizeof(*p->clients));
    }

  p->clients[p->num_clients] = cl;

  bg_log(BG_LOG_INFO, LOG_DOMAIN,
         "Established new client connection for program %s", p->name);
  
  p->num_clients++;
  pthread_mutex_unlock(&p->client_mutex);
  }

static void program_remove_client_nolock(program_t * p, int idx)
  {
  client_destroy(p->clients[idx]);
  if(idx < p->num_clients-1)
    {
    memmove(&p->clients[idx], &p->clients[idx+1],
            (p->num_clients-1 - idx) * sizeof(p->clients[idx]));
    }
  p->num_clients--;
  bg_log(BG_LOG_INFO, LOG_DOMAIN,
         "Closed client connection for program %s", p->name);
  
  }

int program_get_status(program_t * p)
  {
  int ret;
  pthread_mutex_lock(&p->status_mutex);
  ret = p->status;
  pthread_mutex_unlock(&p->status_mutex);
  return ret;  
  }
