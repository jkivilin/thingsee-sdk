/****************************************************************************
 * apps/netutils/json/cJSON_stream_print.c
 *
 * This file is a part of NuttX:
 *
 *   Copyright (c) 2011 Gregory Nutt. All rights reserved.
 *   Copyright (c) 2015 Haltian Ltd.
 *     Author: Jussi Kivilinna <jussi.kivilinna@haltian.com>
 *
 * And derives from the cJSON Project which has an MIT license:
 *
 *   Copyright (c) 2009 Dave Gamble
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

#include <apps/netutils/cJSON.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct cJSON_outstream_s
{
  void (*putc_fn)(char c, void *priv);
  void *fn_priv;
} cJSON_outstream;

struct putc_string_priv_s
{
  char *buf;
  size_t buflen;
  size_t numput;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Prototypes
 ****************************************************************************/

static void stream_print_value(cJSON *item, int depth, bool fmt,
                               cJSON_outstream *stream);
static void stream_print_array(cJSON *item, int depth, bool fmt,
                               cJSON_outstream *stream);
static void stream_print_object(cJSON *item, int depth, bool fmt,
                                cJSON_outstream *stream);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline void stream_putc(cJSON_outstream *stream, char c)
{
  stream->putc_fn(c, stream->fn_priv);
}

static inline void stream_puts(cJSON_outstream *stream, char *string)
{
  while (*string)
    {
      stream_putc(stream, *string++);
    }
}

static bool dbl_equal(double a, double b)
{
  double fabs_a = fabs(a);
  double fabs_b = fabs(b);
  return fabs(a - b) <= ((fabs_a > fabs_b ? fabs_b : fabs_a) * DBL_EPSILON);
}

/* Render the number nicely from the given item into a string. */

static void stream_print_number(cJSON *item, cJSON_outstream *stream)
{
  char str[32];
  double d = cJSON_double(item);
  int int_d = cJSON_int(item);
  double id = int_d;
  size_t slen;
  bool round_up = false;

  do
    {
      /* Get type and needed length output string. */

      if (int_d == 0 && d == 0.0)
        {
          /* Handle zero separately as it's relation to DBL_EPSILON is special one
           * and as zero has two exact presentations in floating point format (positive
           * zero and negative zero). */

          if (!signbit(d))
            {
              /* Simply zero. */

              str[0] = '0';
              str[1] = '\0';
              slen = 1;
            }
          else
            {
              /* Negative zero. */

              slen = snprintf(str, sizeof(str), "%s", "-0.0");
            }

          ASSERT(slen < sizeof(str) - 1);
        }
      else if (int_d != 0 && (d <= INT_MAX && d >= INT_MIN) && dbl_equal(d, id))
        {
          /* Integer value can accurately represent this value. */

          slen = snprintf(str, sizeof(str), "%d", int_d);

          ASSERT(slen < sizeof(str) - 1);
        }
      else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e14)
        {
          /* Double can represent 15 significant digits. */

          /* NuttX printf formatter has problems printing large and small
           * numbers, so do exponent formatting manually. */

          int expo = (int)log10(fabs(d));
          double scaled_d;

          if (expo < 0)
            {
              expo--;
            }

          scaled_d = d * pow(10, -expo);
          slen = snprintf(str, sizeof(str), "%.14f", scaled_d);

          if (strchr(str, '.'))
            {
              /* Remove trailing zeros. */

              slen -= 1;

              while (slen > 0)
                {
                  if (str[--slen] != '0')
                    break;

                  str[slen] = '\0';
                }

              /* Remove trailing dot. */

              if (slen > 0 && str[slen] == '.')
                str[slen--] = '\0';

              slen += 1;
            }

          /* Add exponent. */

          slen += snprintf(str + slen, sizeof(str) - slen, "e%d", expo);

          ASSERT(slen < sizeof(str) - 1);
        }
      else
        {
          int ndigits_before_dot = (int)log10(fabs(d));
          int ndigits_after_dot = 15 - ndigits_before_dot;
          char *pdot;
          char fmtstr[8];

          /* Double can represent 15 significant digits. */

          if (ndigits_after_dot < 0)
            ndigits_after_dot = 0;

          snprintf(fmtstr, sizeof(fmtstr), "%%.%df", ndigits_after_dot);

          slen = snprintf(str, sizeof(str), fmtstr, d);

          ASSERT(slen < sizeof(str) - 1);

          pdot = strchr(str, '.');
          if (pdot && !round_up)
            {
              ssize_t ndigits = &str[slen] - (pdot + 1);

              if (ndigits > 2 && str[slen - 1] == '9' && str[slen - 2] == '9')
                {
                  /* Detected trailing nines rounding error. Round up last
                   * digit. */

                  d += copysign(pow(10, -ndigits), d);
                  int_d = d;
                  id = int_d;
                  round_up = true;

                  continue; /* Retry printing with rounded up number. */
                }
            }

          if (pdot)
            {
              /* Remove trailing zeros. */

              slen -= 1;

              while (slen > 0)
                {
                  if (str[--slen] != '0')
                    break;

                  str[slen] = '\0';
                }

              /* Remove trailing dot. */

              if (slen > 0 && str[slen] == '.')
                str[slen] = '\0';
            }
        }

      break; /* Done, no need for restart. */
    }
  while (true);

  stream_puts(stream, str);
}

/* Render the buffer provided to an hex string that can be printed. */

static void stream_print_buffer(cJSON *item, cJSON_outstream *stream)
{
  struct cJSON_buffer_s buf = cJSON_buffer(item);
  uint8_t *ubuf = buf.ptr;
  char tmp[4];

  stream_putc(stream, '(');

  while (buf.len)
    {
      snprintf(tmp, sizeof(tmp), "%02X", *ubuf);
      stream_puts(stream, tmp);

      ubuf++;
      buf.len--;
    }

  stream_putc(stream, ')');
}

/* Render the cstring provided to an escaped version that can be printed. */

static void stream_print_string_ptr(const char *str, cJSON_outstream *stream)
{
  const char *ptr;
  unsigned char token;

  if (!str)
    {
      return;
    }

  ptr = str;
  stream_putc(stream, '\"');
  while (*ptr)
    {
      if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
        {
          stream_putc(stream, *ptr++);
        }
      else
        {
          char tmp[6];

          stream_putc(stream, '\\');
          switch (token = *ptr++)
            {
            case '\\':
              stream_putc(stream, '\\');
              break;

            case '\"':
              stream_putc(stream, '\"');
              break;

            case '\b':
              stream_putc(stream, 'b');
              break;

            case '\f':
              stream_putc(stream, 'f');
              break;

            case '\n':
              stream_putc(stream, 'n');
              break;

            case '\r':
              stream_putc(stream, 'r');
              break;

            case '\t':
              stream_putc(stream, 't');
              break;

            default:
              /* Escape and print */

              snprintf(tmp, sizeof(tmp), "u%04x", token);
              stream_puts(stream, tmp);
              break;
            }
        }
    }

  stream_putc(stream, '\"');
}

/* Invote print_string_ptr (which is useful) on an item. */

static void stream_print_string(cJSON *item, cJSON_outstream *stream)
{
  stream_print_string_ptr(cJSON_string(item), stream);
}

/* Render a value to text. */

static void stream_print_value(cJSON *item, int depth, bool fmt,
                               cJSON_outstream *stream)
{
  if (!item)
    {
      return;
    }

  switch (cJSON_type(item))
    {
    case cJSON_NULL:
      stream_puts(stream, "null");
      break;

    case cJSON_False:
      stream_puts(stream, "false");
      break;

    case cJSON_True:
      stream_puts(stream, "true");
      break;

    case cJSON_Number:
      stream_print_number(item, stream);
      break;

    case cJSON_String:
      stream_print_string(item, stream);
      break;

    case cJSON_Buffer:
      stream_print_buffer(item, stream);
      break;

    case cJSON_Array:
      stream_print_array(item, depth, fmt, stream);
      break;

    case cJSON_Object:
      stream_print_object(item, depth, fmt, stream);
      break;
    }
}

/* Render an array to text */

static void stream_print_array(cJSON *item, int depth, bool fmt,
                               cJSON_outstream *stream)
{
  cJSON *child;

  /* Compose the output array. */

  stream_putc(stream, '[');

  child = cJSON_child(item);
  while (child)
    {
      /* Print all childs. */

      stream_print_value(child, depth + 1, fmt, stream);

      child = cJSON_next(child);
      if (child)
        {
          stream_putc(stream, ',');
          if (fmt)
            {
              stream_putc(stream, ' ');
            }
        }
    }

  stream_putc(stream, ']');
}

/* Render an object to text. */

static void stream_print_object(cJSON *item, int depth, bool fmt,
                                cJSON_outstream *stream)
{
  cJSON *child;
  int j, i;

  /* Compose the output object. */

  stream_putc(stream, '{');
  if (fmt)
    {
      stream_putc(stream, '\n');
    }

  child = cJSON_child(item);
  while (child)
    {
      if (fmt)
        {
          for (j = 0; j < depth; j++)
            {
              stream_putc(stream, '\t');
            }
        }

      /* Print object name. */

      stream_print_string_ptr(cJSON_name(child), stream);

      stream_putc(stream, ':');
      if (fmt)
        {
          stream_putc(stream, '\t');
        }

      /* Print object value. */

      stream_print_value(child, depth + 1, fmt, stream);

      child = cJSON_next(child);
      if (child)
        {
          stream_putc(stream, ',');
        }

      if (fmt)
        {
          stream_putc(stream, '\n');
        }
    }

  if (fmt)
    {
      for (i = 0; i < depth - 1; i++)
        {
          stream_putc(stream, '\t');
        }
    }

  stream_putc(stream, '}');
}

void putc_string(char c, void *_priv)
{
  struct putc_string_priv_s *priv = _priv;

  if (priv->numput >= priv->buflen)
    {
      priv->numput++;
      return;
    }

  priv->buf[priv->numput++] = c;
}

/* Render a cJSON item/entity/structure to text. */

static char *cJSON_Print_String(cJSON *item, bool formatted)
{
  struct putc_string_priv_s priv = {};
  size_t num_chars;
  char *out;

  priv.buf = NULL;
  priv.buflen = 0;
  priv.numput = 0;

  cJSON_Print_Stream(item, formatted, putc_string, &priv);

  num_chars = priv.numput;
  out = malloc(num_chars + 1);
  if (!out)
    {
      return NULL;
    }

  priv.buf = out;
  priv.buflen = num_chars;
  priv.numput = 0;

  cJSON_Print_Stream(item, formatted, putc_string, &priv);

  DEBUGASSERT(priv.numput == num_chars);

  out[num_chars] = '\0';
  return out;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* Render a cJSON item/entity/structure to stream function. */

void cJSON_Print_Stream(cJSON *item, bool formatted,
                        void (*putc_fn)(char c, void *priv), void *putc_priv)
{
  cJSON_outstream stream = {};

  stream.putc_fn = putc_fn;
  stream.fn_priv = putc_priv;

  stream_print_value(item, 0, formatted, &stream);
}

/* Render a cJSON item/entity/structure to newly allocated text string. */

char *cJSON_Print(cJSON *item)
{
  return cJSON_Print_String(item, true);
}

char *cJSON_PrintUnformatted(cJSON *item)
{
  return cJSON_Print_String(item, false);
}

/* Render a cJSON item/entity/structure to buffer. */

size_t cJSON_Print_Buf(cJSON *item, bool formatted, char *buf, size_t buflen)
{
  struct putc_string_priv_s priv = {};

  priv.buf = buf;
  priv.buflen = buflen;
  priv.numput = 0;

  cJSON_Print_Stream(item, formatted, putc_string, &priv);

  return priv.numput;
}
