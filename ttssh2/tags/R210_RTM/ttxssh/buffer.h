#ifndef BUFFER_H
#define BUFFER_H

#include <openssl/bn.h>

typedef struct buffer {
	char *buf;
	int offset;
	int maxlen;
	int len;
} buffer_t;

void buffer_clear(buffer_t *buf);
buffer_t *buffer_init(void);
void buffer_free(buffer_t *buf);
int buffer_append(buffer_t *buf, char *ptr, int size);
int buffer_append_length(buffer_t *msg, char *ptr, int size);
void buffer_put_string(buffer_t *msg, char *ptr, int size);
void buffer_put_char(buffer_t *msg, int value);
void buffer_put_padding(buffer_t *msg, int size);
void buffer_put_int(buffer_t *msg, int value);
int buffer_len(buffer_t *msg);
char *buffer_ptr(buffer_t *msg);
void buffer_put_bignum2(buffer_t *msg, BIGNUM *value);

void buffer_get_bignum2(char **data, BIGNUM *value);


#endif				/* BUFFER_H */
