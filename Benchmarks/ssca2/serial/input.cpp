#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"
#include "error.h"

char in_buf[BUF_SIZE];
unsigned buf_p = 0;

int get_char(FILE *fp) {
  return (buf_p > 0) ? in_buf[--buf_p] : getc(fp);
}

void put_char(int c, FILE *fp) {
  if (buf_p >= BUF_SIZE)
    error("input buffer overflow");
  in_buf[buf_p++] = c;
}

int peek_char(FILE *fp) {
  int c = get_char(fp);
  put_char(c, fp);
  return c;
}

char *get_line(char *s, int n, FILE *fp) {
  int c;
  char *cs;
  
  cs = s;
  while (--n > 0 && (c = get_char(fp)) != EOF)
    if ((*cs++ = c) == '\n')
      break;
  *cs = '\0';
  return (c == EOF && cs == s) ? NULL : s;
}

void find_start(FILE *fp) {
  while(1) {
    if (!get_line(in_buf, BUF_SIZE, fp))
      error("reached end of file without seeing START");
    if (!strncmp(in_buf, "START", 5))
      break;
  }
}

void skip_whitespace(FILE *fp) {
  int c;
  while((c = get_char(fp))) {
    if (c != ' ' && c != '\n') {
      put_char(c, fp);
      break;
    }
  }
}

void find_string(FILE *fp, const char* fmt, ...) {
  char string[BUF_SIZE];
  va_list args;

  va_start(args, fmt);
  vsprintf(string, fmt, args);
  va_end(args);

#ifdef DEBUG
  printf("finding %s\n", string);
#endif

  skip_whitespace(fp);  
  int n = strlen(string);
  int c;
  char *s_ptr = string;
  while (n-- > 0) {
    c = get_char(fp);
    if ((c == EOF) || c != *s_ptr++)
      error("expected %s, got %c", string, c);
  }
}

long long get_int(FILE *fp) {
  char s[BUF_SIZE];
  unsigned len = 0;
  char c;

  skip_whitespace(fp);
  c = get_char(fp);
  if (c == '-' || (c >= '0' && c <= '9'))
    s[len++] = c;
  else
    error("expected int, got %c", c);

  while ((c = get_char(fp))) {
    if (c >= '0' && c <= '9')
      s[len++] = c;
    else {
      put_char(c, fp);
      break;
    }
  }
  s[len] = 0;

  if (len == 0 || (len == 1 && s[0] == '-'))
    error("expected int, got %s", s);

  long long result = strtoll(s,0,10);
#ifdef DEBUG
  printf("got int %dll\n", result);
#endif
  return result;
}

double get_double(FILE *fp) {
  char s[BUF_SIZE];
  unsigned len = 0;
  char c;
  int point = 0;

  skip_whitespace(fp);
  c = get_char(fp);
  if (c == '-' || (c >= '0' && c <= '9'))
    s[len++] = c;
  else if (c == '.') {
    s[len++] = c;
    point = 1;
  } else
    error("expected double, got %c", c);

  while ((c = get_char(fp))) {
    if (c >= '0' && c <= '9')
      s[len++] = c;
    else if (c == '.' && !point) {
      s[len++] = c;
      point = 1;
    } else {
      put_char(c, fp);
      break;
    }
  }
  s[len] = 0;

  if (len == 0 || (len == 1 && (s[0] == '-' || s[0] == '.')) ||
      (len == 2 && s[0] == '-' && s[1] == '.'))
    error("expected int, got %s", s);

  double result = atof(s);
#ifdef DEBUG
  printf("got double %f\n", result);
#endif
  return result;
}
