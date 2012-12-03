#ifndef INPUT_H
#define INPUT_H

#define BUF_SIZE 256

//#define DEBUG

int get_char(FILE *fp);

void put_char(int c, FILE *fp);

int peek_char(FILE *fp);

char *get_line(char *s, int n, FILE *fp);

void find_start(FILE *fp);

void skip_whitespace(FILE *fp);

void find_string(FILE *fp, const char* fmt, ...);

long long int get_int(FILE *fp);

double get_double(FILE *fp);

#endif
