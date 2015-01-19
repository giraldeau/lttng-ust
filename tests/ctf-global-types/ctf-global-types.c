/*
 * Copyright (C) 2014  Geneviève Bastien <gbastien@versatic.net>
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
#include "ust_tests_ctf_global_types.h"

int main(int argc, char **argv)
{
	int i;
	int delay = 0;

	if (argc == 2)
		delay = atoi(argv[1]);

	fprintf(stderr, "Hello, World!\n");

	sleep(delay);

	fprintf(stderr, "Tracing... ");
	for (i = 0; i < 100; i++) {
		tracepoint(ust_tests_ctf_global_types, tptest, i, (i % 6));
	}

	for (i = 0; i < 10; i++) {
		tracepoint(ust_tests_ctf_global_types, tptest_bis, i, (i % 5));
	}
	fprintf(stderr, " done.\n");
	return 0;
}
