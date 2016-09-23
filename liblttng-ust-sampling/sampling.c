/*
 * Copyright (C) 2016  Francis Giraldeau <francis.giraldeau@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#define _GNU_SOURCE

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/perf_event.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "sampling_tp.h"

#define DEPTH_MAX 100

struct perf_event {
	struct perf_event_attr attr;
	int fd;
};

static struct perf_event event;

static int hits = 0;

static pid_t gettid(void)
{
    return syscall(SYS_gettid);
}

static long sys_perf_event_open(struct perf_event_attr *attr, pid_t pid,
				int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static void handle_sigio(int signo, siginfo_t *info, void *data)
{
	hits++;
	if (event.fd > 0) {
		void *addr[DEPTH_MAX];
		int depth = unw_backtrace((void **)&addr, DEPTH_MAX);
		tracepoint(lttng_ust_sampling, cpu_cycles, addr, depth);
		ioctl(event.fd, PERF_EVENT_IOC_REFRESH, 1);
	}
}

static int setup_sighandler(void *action, int check)
{
	struct sigaction sigact, oldsigact;
	sigset_t set;

	/* check if the signal is blocked */
	if (check) {
		sigemptyset(&set);
		pthread_sigmask(SIG_BLOCK, NULL, &set);
		if (sigismember(&set, SIGIO)) {
			printf("signal SIGIO is blocked\n");
			return -1;
		}
	}

	/* install handler */
	sigact.sa_sigaction = action;
	sigact.sa_flags = SA_SIGINFO;
	if (sigaction(SIGIO, &sigact, &oldsigact) < 0) {
		printf("sigaction() failed\n");
		return -1;
	}
	if (check && !(oldsigact.sa_sigaction == (void *) SIG_DFL ||
		       oldsigact.sa_sigaction == (void *) SIG_IGN)) {
		printf("perf sampling overwrites SIGIO handler\n");
	}
	return 0;
}

__attribute__((constructor))
void lttng_ust_sampling_init(void)
{
	int fd, flags;
	struct f_owner_ex fown;
	int tid = gettid();

	printf("lttng_ust_sampling constructor\n");
	memset(&event.attr, 0, sizeof(event.attr));
	event.attr.size = sizeof(event.attr);	/* API requires actual struct size */
	event.attr.type = PERF_TYPE_HARDWARE;
	event.attr.config = PERF_COUNT_HW_CPU_CYCLES;
	event.attr.sample_period = 1E5;		/* Sample period (number of cpu cycles) */
	event.attr.disabled = 1;		/* Do not start the event on open */
	event.attr.watermark = 0;		/* Selects wakeup_event union */
	event.attr.wakeup_events = 1;		/* Get one callback for every sample */

	fd = sys_perf_event_open(&event.attr, 0, -1, -1, 0);
	if (fd < 0) {
		printf("error: perf_event_open failed (%d)\n", fd);
		return;
	}

	fown.type = F_OWNER_TID;
	fown.pid = tid;
	if (fcntl(fd, F_SETOWN_EX, &fown) < 0) {
		goto fail;
	}

	flags = fcntl(fd, F_GETFL);
	if (fcntl(fd, F_SETFL, flags | FASYNC | O_ASYNC) < 0) {
		goto fail;
	}

	if (setup_sighandler(handle_sigio, 1) < 0) {
		goto fail;
	}

	event.fd = fd;
	__sync_synchronize();
	printf("lttng_ust_sampling before enabling\n");

	ioctl(event.fd, PERF_EVENT_IOC_REFRESH, 1);
	ioctl(event.fd, PERF_EVENT_IOC_ENABLE, 0);
	printf("lttng_ust_sampling enabled!\n");
	return;
fail:
	close(fd);
	return;
}

__attribute__((destructor))
void lttng_ust_sampling_exit(void)
{
	if (event.fd > 0) {
		close(event.fd);
		setup_sighandler(SIG_DFL, 0);
	}
}
