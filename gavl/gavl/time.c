#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <inttypes.h>
#include <gavltime.h>

/* Sleep for a specified time */

void gavl_time_delay(gavl_time_t * t)
  {
  struct timeval tv;
  
  tv.tv_sec  = *t / 1000000;
  tv.tv_usec = *t % 1000000;
  select(0, NULL, NULL, NULL, &tv);
  }

void gavl_samples_to_time(gavl_time_t * ret,
                          int samplerate, int64_t samples)
  {
  *ret = samples * 1000000 / samplerate; 
  }

void gavl_frames_to_time(gavl_time_t * ret,
                         double framerate, int64_t frames)
  {
  *ret = (gavl_time_t)((double)frames*1000000.0/framerate);
  }


void gavl_time_to_samples(int64_t * ret,
                          int samplerate, gavl_time_t time)
  {
  *ret = (time*samplerate)/1000000;
  }

void gavl_time_to_frames(int64_t * ret,
                         double framerate, gavl_time_t time)
  {
  *ret = (int64_t)(((double)time*framerate)/1000000.0+0.5);
  }

/*
 *  Pretty print a time in the format:
 *  hhh:mm:ss
 */

static char digit_to_char_array[] = "0123456789 ";

static char digit_to_char(int digit)
  {
  if((digit > 9) || (digit < 0))
    return ' ';
  return digit_to_char_array[digit];
  }

void
gavl_time_prettyprint(gavl_time_t time, char ret[GAVL_TIME_STRING_LEN])
  {
  int64_t total_seconds;
  char * pos;
  int seconds;
  int minutes;
  int hours;
  int negative;
    
  total_seconds = time / 1000000;
  if(total_seconds < 0)
    {
    negative = 1;
    total_seconds = -total_seconds;
    }
  else
    {
    negative = 0;
    }

  pos = &(ret[GAVL_TIME_STRING_LEN-1]);
  *pos = '\0';
  pos--;

  seconds = total_seconds % 60;
  total_seconds /= 60;
  minutes = total_seconds % 60;
  total_seconds /= 60;
  hours = total_seconds;

  /* Print seconds */

  *pos = digit_to_char(seconds % 10);
  pos--;
  *pos = digit_to_char(seconds / 10);
  pos--;

  *pos = ':';
  pos--;

  *pos = digit_to_char(minutes % 10);
  pos--;
  
  if((minutes / 10) || hours)
    {
    /* Print minutes */

    *pos = digit_to_char(minutes / 10);
    pos--;
    
    if(hours)
      {
      *pos = ':';
      pos--;

      *pos = digit_to_char(hours % 10);
      pos--;

      hours /= 10;
      if(hours)
        {
        *pos = digit_to_char(hours % 10);
        pos--;
        hours /= 10;
        }
      if(hours)
        {
        *pos = digit_to_char(hours % 10);
        pos--;
        }
      }
    }

  if(negative)
    {
    *pos = '-';
    pos--;
    }

  while(pos >= ret)
    {
    *pos = ' ';
    pos--;
    }
  }
