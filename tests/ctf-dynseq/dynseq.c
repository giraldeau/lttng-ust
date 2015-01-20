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

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "tp.h"

int main(int argc, char **argv)
{
	unsigned int i, num = 10;
	int *array;

	fprintf(stderr, "Tracing... ");
	array = malloc(sizeof(int) * 10);
	for (i = 0; i < num; i++) {
		array[i] = i;
	}
	tracepoint(dynseq, foo, num, array, i);
	free(array);
	fprintf(stderr, " done.\n");
	return 0;
}
