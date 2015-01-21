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

#include <unistd.h>
#include <string.h>

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "tp.h"
#include "dynseq.h"


int main(int argc, char **argv)
{
	struct dynseq tb;
	static char *msg = "hello";

	fprintf(stderr, "Tracing... ");
	tb.filename = msg;
	tb.filename_len = strlen(msg);
	tb.lineno = 42;
	tracepoint(dynseq, foo, &tb, 1);
	fprintf(stderr, " done.\n");
	return 0;
}
