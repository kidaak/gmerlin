/*****************************************************************

  audio.h

  Copyright (c) 2003 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de

  http://gmerlin.sourceforge.net

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

*****************************************************************/

/* Private structures for the video converter */

#include <gavl.h>
#include "config.h"

typedef struct gavl_audio_convert_context_s gavl_audio_convert_context_t;
typedef struct gavl_mix_matrix_s gavl_mix_matrix_t;

typedef void (*gavl_audio_func_t)(struct gavl_audio_convert_context_s * ctx);


struct gavl_audio_convert_context_s
  {
  gavl_audio_frame_t * input_frame;
  gavl_audio_frame_t * output_frame;

  gavl_audio_format_t input_format;
  gavl_audio_format_t output_format;
  
  /* Conversion function to be called */

  gavl_audio_func_t func;

  /* Private data */
  
  gavl_mix_matrix_t * mix_matrix;
  
  /* For chaining */
  
  struct gavl_audio_convert_context_s * next;
  };

gavl_audio_convert_context_t *
gavl_audio_convert_context_create(gavl_audio_options_t * opt,
                                  gavl_audio_format_t  * input_format,
                                  gavl_audio_format_t  * output_format);

gavl_audio_convert_context_t *
gavl_mix_context_create(gavl_audio_options_t * opt,
                        gavl_audio_format_t  * input_format,
                        gavl_audio_format_t  * output_format);

gavl_audio_convert_context_t *
gavl_interleave_context_create(gavl_audio_options_t * opt,
                               gavl_audio_format_t  * input_format,
                               gavl_audio_format_t  * output_format);

gavl_audio_convert_context_t *
gavl_sampleformat_context_create(gavl_audio_options_t * opt,
                                 gavl_audio_format_t  * input_format,
                                 gavl_audio_format_t  * output_format);


/* Utility function */

int gavl_bytes_per_sample(gavl_sample_format_t format);

gavl_audio_func_t
gavl_find_sampletype_converter(const gavl_audio_options_t * opt,
                               gavl_audio_format_t * in,
                               gavl_audio_format_t * out);

