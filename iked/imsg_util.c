/*	$OpenBSD: imsg_util.c,v 1.14 2023/05/23 12:43:26 claudio Exp $	*/

/*
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <event.h>

#include "iked.h"

/*
 * Extending the imsg buffer API for internal use
 */

int
ibuf_cat(struct ibuf *dst, struct ibuf *src)
{
	return (ibuf_add(dst, src->buf, ibuf_size(src)));
}

struct ibuf *
ibuf_new(const void *data, size_t len)
{
	struct ibuf	*buf;

	if ((buf = ibuf_dynamic(len,
	    IKED_MSGBUF_MAX)) == NULL)
		return (NULL);

	if (len == 0)
		return (buf);

	if (data == NULL) {
		if (ibuf_advance(buf, len) == NULL) {
			ibuf_free(buf);
			return (NULL);
		}
	} else {
		if (ibuf_add(buf, data, len) != 0) {
			ibuf_free(buf);
			return (NULL);
		}
	}

	return (buf);
}

struct ibuf *
ibuf_static(void)
{
	return ibuf_open(IKED_MSGBUF_MAX);
}

void *
ibuf_advance(struct ibuf *buf, size_t len)
{
	return ibuf_reserve(buf, len);
}

void
ibuf_release(struct ibuf *buf)
{
	ibuf_free(buf);
}

size_t
ibuf_length(struct ibuf *buf)
{
	if (buf == NULL || buf->buf == NULL)
		return (0);
	return (ibuf_size(buf));
}

uint8_t *
ibuf_data(struct ibuf *buf)
{
	return (ibuf_seek(buf, 0, 0));
}

void *
ibuf_getdata(struct ibuf *buf, size_t len)
{
	void	*data;

	if ((data = ibuf_seek(buf, buf->rpos, len)) == NULL)
		return (NULL);
	buf->rpos += len;

	return (data);
}

struct ibuf *
ibuf_get(struct ibuf *buf, size_t len)
{
	void		*data;

	if ((data = ibuf_getdata(buf, len)) == NULL)
		return (NULL);

	return (ibuf_new(data, len));
}

struct ibuf *
ibuf_dup(struct ibuf *buf)
{
	if (buf == NULL)
		return (NULL);
	return (ibuf_new(ibuf_data(buf), ibuf_size(buf)));
}

struct ibuf *
ibuf_random(size_t len)
{
	struct ibuf	*buf;
	void		*ptr;

	if ((buf = ibuf_open(len)) == NULL)
		return (NULL);
	if ((ptr = ibuf_reserve(buf, len)) == NULL) {
		ibuf_free(buf);
		return (NULL);
	}
	arc4random_buf(ptr, len);
	return (buf);
}

int
ibuf_setsize(struct ibuf *buf, size_t len)
{
	if (len > buf->size)
		return (-1);
	buf->wpos = len;
	return (0);
}

int
ibuf_prepend(struct ibuf *buf, void *data, size_t len)
{
	struct ibuf	*new;

	/* Swap buffers (we could also use memmove here) */
	if ((new = ibuf_new(data, len)) == NULL)
		return (-1);
	if (ibuf_cat(new, buf) == -1) {
		ibuf_release(new);
		return (-1);
	}
	free(buf->buf);
	memcpy(buf, new, sizeof(*buf));
	free(new);

	return (0);
}

int
ibuf_strcat(struct ibuf **buf, const char *s)
{
	size_t slen;

	if (buf == NULL)
		return (-1);
	slen = strlen(s);
	if (*buf == NULL) {
		if ((*buf = ibuf_new(s, slen)) == NULL)
			return (-1);
		return (0);
	}
	return (ibuf_add(*buf, s, slen));
}

int
ibuf_strlen(struct ibuf *buf)
{
	if (ibuf_length(buf) > INT_MAX)
		return (INT_MAX);
	return ((int)ibuf_length(buf));
}
