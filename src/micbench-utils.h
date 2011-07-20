#ifndef MICBENCH_UTILS_H
#define MICBENCH_UTILS_H

#include <glib.h>
#include <string.h>
#include <ctype.h>

// unit
#define KILO 1000L
#define KIBI 1024L
#define MEBI (KIBI*KIBI)
#define GIBI (KIBI*MEBI)



/*
  <set> := <consecutive_set> | <consecutive_set> '+' <set>
  <consective_set> := <single_set> | <range_set>
  <single_set> := 0 | [1-9] [0-9]*
  <range_set> := <single_set> '-' <single_set>

  ex)
  1 => 0x0000...0001
  3-5 => 0x0...011100
  3-5+7 => 0x0...01011100
 */

gint64
micbench_parse_range_to_bitmap(const gchar *input, gchar endchar);

glong
micbench_parse_size(const gchar *sz_str);

#endif
