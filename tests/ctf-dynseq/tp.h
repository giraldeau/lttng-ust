#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER dynseq

#if !defined(_TRACEPOINT_DYNSEQ_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TRACEPOINT_DYNSEQ_H

/*
 * Copyright (C) 2015 Francis Giraldeau <francis.giraldeau@gmail.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <lttng/tracepoint.h>


TRACEPOINT_STRUCT(python, bar,
    TP_ARGS(int, lineno),
    TP_FIELDS(
        ctf_integer(int, lineno, lineno)
    )
)

TRACEPOINT_EVENT(dynseq, foo,
	TP_ARGS(unsigned int, len, int *, array, int, x),
	TP_FIELDS(
		ctf_sequence(int, seq, array, unsigned int, len)
		ctf_struct(python, bar, barfield, x)
	)
)

#endif /* _TRACEPOINT_DYNSEQ_H */

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./tp.h"

/* This part must be outside ifdef protection */
#include <lttng/tracepoint-event.h>
