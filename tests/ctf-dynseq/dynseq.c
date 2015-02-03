/*
 * Copyright (C) 2015  Francis Giraldeau <francis.giraldeau@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2.1 of
 * the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <string.h>

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "tp.h"
#include "dynseq.h"

void test_array(void)
{
	int i, ret;
	int num = 10;
	struct dynseq tb[num];
	static char *fmt = "hello %d";

	for (i = 0; i < num; i++) {
		struct dynseq *item = &tb[i];
		ret = asprintf(&item->filename, fmt, i);
		if (ret < 0)
			break;
		item->filename_len = strlen(item->filename);
		item->lineno = i;
	}
	tracepoint(dynseq, foo, tb, num);
	for (i = 0; i < num; i++) {
		struct dynseq *item = &tb[i];
		free(item->filename);
	}
}

void test_stack(void)
{
	struct dynseq tb;
	static char *msg = "hello";
	tb.filename = msg;
	tb.filename_len = strlen(msg);
	tb.lineno = 42;
	tracepoint(dynseq, foo, &tb, 1);
}

int main(int argc, char **argv)
{
	fprintf(stderr, "Tracing... ");
	test_stack();
	test_array();
	fprintf(stderr, " done.\n");
	return 0;
}
