/*****************************************************************
 
  in_file.c
 
  Copyright (c) 2003-2006 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <avdec_private.h>
#define LOG_DOMAIN "in_file"
#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_FSEEKO
#define BGAV_FSEEK fseeko
#else
#define BGAV_FSEEK fseek
#endif

#ifdef HAVE_FTELLO
#define BGAV_FTELL ftello
#else
#define BGAV_FSEEK ftell
#endif

static int open_file(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  FILE * f;

  if(!strncmp(url, "file://", 7))
    url += 7;
  
  f = fopen(url, "rb");
  if(!f)
    {
    bgav_log(ctx->opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Cannot open %s: %s",
             url, strerror(errno));
    return 0;
    }
  ctx->priv = f;

  BGAV_FSEEK((FILE*)(ctx->priv), 0, SEEK_END);
  ctx->total_bytes = BGAV_FTELL((FILE*)(ctx->priv));
    
  BGAV_FSEEK((FILE*)(ctx->priv), 0, SEEK_SET);

  
  ctx->filename = bgav_strdup(url);
  return 1;
  }

static int     read_file(bgav_input_context_t* ctx,
                         uint8_t * buffer, int len)
  {
  return fread(buffer, 1, len, (FILE*)(ctx->priv)); 
  }

static int64_t seek_byte_file(bgav_input_context_t * ctx,
                              int64_t pos, int whence)
  {
  BGAV_FSEEK((FILE*)(ctx->priv), ctx->position, SEEK_SET);
  return BGAV_FTELL((FILE*)(ctx->priv));
  }

static void    close_file(bgav_input_context_t * ctx)
  {
  if(ctx->priv)
    fclose((FILE*)(ctx->priv));
  }

static int open_stdin(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  ctx->priv = stdin;
  return 1;
  }

static void close_stdin(bgav_input_context_t * ctx)
  {
  /* Nothing to do here */
  }

bgav_input_t bgav_input_file =
  {
    name:      "file",
    open:      open_file,
    read:      read_file,
    seek_byte: seek_byte_file,
    close:     close_file
  };

bgav_input_t bgav_input_stdin =
  {
    name:      "stdin",
    open:      open_stdin,
    read:      read_file,
    close:     close_stdin
  };

