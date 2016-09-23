#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER lttng_ust_sampling

#if !defined(_TRACEPOINT_UST_SAMPLING_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TRACEPOINT_UST_SAMPLING_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (C) 2016  Francis Giraldeau <francis.giraldeau@gmail.com>
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

/*
 * TODO: change event type (i.e. cpu_cycle) to enum in order to suppor other
 * counters
 */

TRACEPOINT_EVENT(lttng_ust_sampling, cpu_cycles,
	TP_ARGS(void *, addr, size_t, unw_depth),
	TP_FIELDS(
		ctf_sequence(void *, addr, addr, size_t, unw_depth)
	)
)

#endif /* _TRACEPOINT_UST_LIBC_H */

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./sampling_tp.h"

/* This part must be outside ifdef protection */
#include <lttng/tracepoint-event.h>

#ifdef __cplusplus
}
#endif
