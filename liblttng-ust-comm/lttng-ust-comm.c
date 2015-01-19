/*
 * Copyright (C)  2011 - David Goulet <david.goulet@polymtl.ca>
 * Copyright (C)  2011-2013 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; only
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _GNU_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <lttng/ust-ctl.h>
#include <ust-comm.h>
#include <helper.h>
#include <lttng/ust-error.h>
#include <lttng/ust-events.h>
#include <usterr-signal-safe.h>

#include "../liblttng-ust/compat.h"

#define USTCOMM_CODE_OFFSET(code)	\
	(code == LTTNG_UST_OK ? 0 : (code - LTTNG_UST_ERR + 1))

#define USTCOMM_MAX_SEND_FDS	4

/*
 * Human readable error message.
 */
static const char *ustcomm_readable_code[] = {
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_OK) ] = "Success",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR) ] = "Unknown error",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_NOENT) ] = "No entry",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_EXIST) ] = "Object already exists",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_INVAL) ] = "Invalid argument",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_PERM) ] = "Permission denied",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_NOSYS) ] = "Not implemented",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_EXITING) ] = "Process is exiting",

	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_INVAL_MAGIC) ] = "Invalid magic number",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_INVAL_SOCKET_TYPE) ] = "Invalid socket type",
	[ USTCOMM_CODE_OFFSET(LTTNG_UST_ERR_UNSUP_MAJOR) ] = "Unsupported major version",
};

/*
 * lttng_ust_strerror
 *
 * Receives positive error value.
 * Return ptr to string representing a human readable
 * error code from the ustcomm_return_code enum.
 */
const char *lttng_ust_strerror(int code)
{
	if (code == LTTNG_UST_OK)
		return ustcomm_readable_code[USTCOMM_CODE_OFFSET(code)];
	if (code < LTTNG_UST_ERR)
		return strerror(code);
	if (code >= LTTNG_UST_ERR_NR)
		code = LTTNG_UST_ERR;
	return ustcomm_readable_code[USTCOMM_CODE_OFFSET(code)];
}

/*
 * ustcomm_connect_unix_sock
 *
 * Connect to unix socket using the path name.
 */
int ustcomm_connect_unix_sock(const char *pathname)
{
	struct sockaddr_un sun;
	int fd, ret;

	/*
	 * libust threads require the close-on-exec flag for all
	 * resources so it does not leak file descriptors upon exec.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		PERROR("socket");
		ret = -errno;
		goto error;
	}
	ret = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (ret < 0) {
		PERROR("fcntl");
		ret = -errno;
		goto error_fcntl;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, pathname, sizeof(sun.sun_path));
	sun.sun_path[sizeof(sun.sun_path) - 1] = '\0';

	ret = connect(fd, (struct sockaddr *) &sun, sizeof(sun));
	if (ret < 0) {
		/*
		 * Don't print message on connect ENOENT error, because
		 * connect is used in normal execution to detect if
		 * sessiond is alive. ENOENT is when the unix socket
		 * file does not exist, and ECONNREFUSED is when the
		 * file exists but no sessiond is listening.
		 */
		if (errno != ECONNREFUSED && errno != ECONNRESET
				&& errno != ENOENT && errno != EACCES)
			PERROR("connect");
		ret = -errno;
		if (ret == -ECONNREFUSED || ret == -ECONNRESET)
			ret = -EPIPE;
		goto error_connect;
	}

	return fd;

error_connect:
error_fcntl:
	{
		int closeret;

		closeret = close(fd);
		if (closeret)
			PERROR("close");
	}
error:
	return ret;
}

/*
 * ustcomm_accept_unix_sock
 *
 * Do an accept(2) on the sock and return the
 * new file descriptor. The socket MUST be bind(2) before.
 */
int ustcomm_accept_unix_sock(int sock)
{
	int new_fd;
	struct sockaddr_un sun;
	socklen_t len = 0;

	/* Blocking call */
	new_fd = accept(sock, (struct sockaddr *) &sun, &len);
	if (new_fd < 0) {
		if (errno != ECONNABORTED)
			PERROR("accept");
		new_fd = -errno;
		if (new_fd == -ECONNABORTED)
			new_fd = -EPIPE;
	}
	return new_fd;
}

/*
 * ustcomm_create_unix_sock
 *
 * Creates a AF_UNIX local socket using pathname
 * bind the socket upon creation and return the fd.
 */
int ustcomm_create_unix_sock(const char *pathname)
{
	struct sockaddr_un sun;
	int fd, ret;

	/* Create server socket */
	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		PERROR("socket");
		ret = -errno;
		goto error;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, pathname, sizeof(sun.sun_path));
	sun.sun_path[sizeof(sun.sun_path) - 1] = '\0';

	/* Unlink the old file if present */
	(void) unlink(pathname);
	ret = bind(fd, (struct sockaddr *) &sun, sizeof(sun));
	if (ret < 0) {
		PERROR("bind");
		ret = -errno;
		goto error_close;
	}

	return fd;

error_close:
	{
		int closeret;

		closeret = close(fd);
		if (closeret) {
			PERROR("close");
		}
	}
error:
	return ret;
}

/*
 * ustcomm_listen_unix_sock
 *
 * Make the socket listen using LTTNG_UST_COMM_MAX_LISTEN.
 */
int ustcomm_listen_unix_sock(int sock)
{
	int ret;

	ret = listen(sock, LTTNG_UST_COMM_MAX_LISTEN);
	if (ret < 0) {
		ret = -errno;
		PERROR("listen");
	}

	return ret;
}

/*
 * ustcomm_close_unix_sock
 *
 * Shutdown cleanly a unix socket.
 */
int ustcomm_close_unix_sock(int sock)
{
	int ret;

	ret = close(sock);
	if (ret < 0) {
		PERROR("close");
		ret = -errno;
	}

	return ret;
}

/*
 * ustcomm_recv_unix_sock
 *
 * Receive data of size len in put that data into
 * the buf param. Using recvmsg API.
 * Return the size of received data.
 * Return 0 on orderly shutdown.
 */
ssize_t ustcomm_recv_unix_sock(int sock, void *buf, size_t len)
{
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t ret = -1;
	size_t len_last;

	memset(&msg, 0, sizeof(msg));

	iov[0].iov_base = buf;
	iov[0].iov_len = len;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	do {
		len_last = iov[0].iov_len;
		ret = recvmsg(sock, &msg, 0);
		if (ret > 0) {
			iov[0].iov_base += ret;
			iov[0].iov_len -= ret;
			assert(ret <= len_last);
		}
	} while ((ret > 0 && ret < len_last) || (ret < 0 && errno == EINTR));

	if (ret < 0) {
		int shutret;

		if (errno != EPIPE && errno != ECONNRESET && errno != ECONNREFUSED)
			PERROR("recvmsg");
		ret = -errno;
		if (ret == -ECONNRESET || ret == -ECONNREFUSED)
			ret = -EPIPE;

		shutret = shutdown(sock, SHUT_RDWR);
		if (shutret)
			ERR("Socket shutdown error");
	} else if (ret > 0) {
		ret = len;
	}
	/* ret = 0 means an orderly shutdown. */

	return ret;
}

/*
 * ustcomm_send_unix_sock
 *
 * Send buf data of size len. Using sendmsg API.
 * Return the size of sent data.
 */
ssize_t ustcomm_send_unix_sock(int sock, const void *buf, size_t len)
{
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t ret;

	memset(&msg, 0, sizeof(msg));

	iov[0].iov_base = (void *) buf;
	iov[0].iov_len = len;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	/*
	 * Using the MSG_NOSIGNAL when sending data from sessiond to
	 * libust, so libust does not receive an unhandled SIGPIPE or
	 * SIGURG. The sessiond receiver side can be made more resilient
	 * by ignoring SIGPIPE, but we don't have this luxury on the
	 * libust side.
	 */
	do {
		ret = sendmsg(sock, &msg, MSG_NOSIGNAL);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		int shutret;

		if (errno != EPIPE && errno != ECONNRESET)
			PERROR("sendmsg");
		ret = -errno;
		if (ret == -ECONNRESET)
			ret = -EPIPE;

		shutret = shutdown(sock, SHUT_RDWR);
		if (shutret)
			ERR("Socket shutdown error");
	}

	return ret;
}

/*
 * Send a message accompanied by fd(s) over a unix socket.
 *
 * Returns the size of data sent, or negative error value.
 */
ssize_t ustcomm_send_fds_unix_sock(int sock, int *fds, size_t nb_fd)
{
	struct msghdr msg;
	struct cmsghdr *cmptr;
	struct iovec iov[1];
	ssize_t ret = -1;
	unsigned int sizeof_fds = nb_fd * sizeof(int);
	char tmp[CMSG_SPACE(sizeof_fds)];
	char dummy = 0;

	memset(&msg, 0, sizeof(msg));
	memset(tmp, 0, CMSG_SPACE(sizeof_fds) * sizeof(char));

	if (nb_fd > USTCOMM_MAX_SEND_FDS)
		return -EINVAL;

	msg.msg_control = (caddr_t)tmp;
	msg.msg_controllen = CMSG_LEN(sizeof_fds);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	cmptr->cmsg_len = CMSG_LEN(sizeof_fds);
	memcpy(CMSG_DATA(cmptr), fds, sizeof_fds);
	/* Sum of the length of all control messages in the buffer: */
	msg.msg_controllen = cmptr->cmsg_len;

	iov[0].iov_base = &dummy;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	do {
		ret = sendmsg(sock, &msg, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		/*
		 * We consider EPIPE and ECONNRESET as expected.
		 */
		if (errno != EPIPE && errno != ECONNRESET) {
			PERROR("sendmsg");
		}
		ret = -errno;
		if (ret == -ECONNRESET)
			ret = -EPIPE;
	}
	return ret;
}

/*
 * Recv a message accompanied by fd(s) from a unix socket.
 *
 * Returns the size of received data, or negative error value.
 *
 * Expect at most "nb_fd" file descriptors. Returns the number of fd
 * actually received in nb_fd.
 * Returns -EPIPE on orderly shutdown.
 */
ssize_t ustcomm_recv_fds_unix_sock(int sock, int *fds, size_t nb_fd)
{
	struct iovec iov[1];
	ssize_t ret = 0;
	struct cmsghdr *cmsg;
	size_t sizeof_fds = nb_fd * sizeof(int);
	char recv_fd[CMSG_SPACE(sizeof_fds)];
	struct msghdr msg;
	char dummy;

	memset(&msg, 0, sizeof(msg));

	/* Prepare to receive the structures */
	iov[0].iov_base = &dummy;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = recv_fd;
	msg.msg_controllen = sizeof(recv_fd);

	do {
		ret = recvmsg(sock, &msg, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		if (errno != EPIPE && errno != ECONNRESET) {
			PERROR("recvmsg fds");
		}
		ret = -errno;
		if (ret == -ECONNRESET)
			ret = -EPIPE;
		goto end;
	}
	if (ret == 0) {
		/* orderly shutdown */
		ret = -EPIPE;
		goto end;
	}
	if (ret != 1) {
		ERR("Error: Received %zd bytes, expected %d\n",
				ret, 1);
		goto end;
	}
	if (msg.msg_flags & MSG_CTRUNC) {
		ERR("Error: Control message truncated.\n");
		ret = -1;
		goto end;
	}
	cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg) {
		ERR("Error: Invalid control message header\n");
		ret = -1;
		goto end;
	}
	if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
		ERR("Didn't received any fd\n");
		ret = -1;
		goto end;
	}
	if (cmsg->cmsg_len != CMSG_LEN(sizeof_fds)) {
		ERR("Error: Received %zu bytes of ancillary data, expected %zu\n",
				(size_t) cmsg->cmsg_len, (size_t) CMSG_LEN(sizeof_fds));
		ret = -1;
		goto end;
	}
	memcpy(fds, CMSG_DATA(cmsg), sizeof_fds);
	ret = sizeof_fds;
end:
	return ret;
}

int ustcomm_send_app_msg(int sock, struct ustcomm_ust_msg *lum)
{
	ssize_t len;

	len = ustcomm_send_unix_sock(sock, lum, sizeof(*lum));
	switch (len) {
	case sizeof(*lum):
		break;
	default:
		if (len < 0) {
			return len;
		} else {
			ERR("incorrect message size: %zd\n", len);
			return -EINVAL;
		}
	}
	return 0;
}

int ustcomm_recv_app_reply(int sock, struct ustcomm_ust_reply *lur,
			  uint32_t expected_handle, uint32_t expected_cmd)
{
	ssize_t len;

	memset(lur, 0, sizeof(*lur));
	len = ustcomm_recv_unix_sock(sock, lur, sizeof(*lur));
	switch (len) {
	case 0:	/* orderly shutdown */
		return -EPIPE;
	case sizeof(*lur):
		if (lur->handle != expected_handle) {
			ERR("Unexpected result message handle: "
				"expected: %u vs received: %u\n",
				expected_handle, lur->handle);
			return -EINVAL;
		}
		if (lur->cmd != expected_cmd) {
			ERR("Unexpected result message command "
				"expected: %u vs received: %u\n",
				expected_cmd, lur->cmd);
			return -EINVAL;
		}
		return lur->ret_code;
	default:
		if (len >= 0) {
			ERR("incorrect message size: %zd\n", len);
		}
		return len;
	}
}

int ustcomm_send_app_cmd(int sock,
			struct ustcomm_ust_msg *lum,
			struct ustcomm_ust_reply *lur)
{
	int ret;

	ret = ustcomm_send_app_msg(sock, lum);
	if (ret)
		return ret;
	ret = ustcomm_recv_app_reply(sock, lur, lum->handle, lum->cmd);
	if (ret > 0)
		return -EIO;
	return ret;
}

/*
 * chan_data is allocated internally if this function returns the
 * expected var_len.
 */
ssize_t ustcomm_recv_channel_from_sessiond(int sock,
		void **_chan_data, uint64_t var_len,
		int *_wakeup_fd)
{
	void *chan_data;
	ssize_t len, nr_fd;
	int wakeup_fd;

	if (var_len > LTTNG_UST_CHANNEL_DATA_MAX_LEN) {
		len = -EINVAL;
		goto error_check;
	}
	/* Receive variable length data */
	chan_data = zmalloc(var_len);
	if (!chan_data) {
		len = -ENOMEM;
		goto error_alloc;
	}
	len = ustcomm_recv_unix_sock(sock, chan_data, var_len);
	if (len != var_len) {
		goto error_recv;
	}
	/* recv wakeup fd */
	nr_fd = ustcomm_recv_fds_unix_sock(sock, &wakeup_fd, 1);
	if (nr_fd <= 0) {
		if (nr_fd < 0) {
			len = nr_fd;
			goto error_recv;
		} else {
			len = -EIO;
			goto error_recv;
		}
	}
	*_wakeup_fd = wakeup_fd;
	*_chan_data = chan_data;
	return len;

error_recv:
	free(chan_data);
error_alloc:
error_check:
	return len;
}

int ustcomm_recv_stream_from_sessiond(int sock,
		uint64_t *memory_map_size,
		int *shm_fd, int *wakeup_fd)
{
	ssize_t len;
	int ret;
	int fds[2];

	/* recv shm fd and wakeup fd */
	len = ustcomm_recv_fds_unix_sock(sock, fds, 2);
	if (len <= 0) {
		if (len < 0) {
			ret = len;
			goto error;
		} else {
			ret = -EIO;
			goto error;
		}
	}
	*shm_fd = fds[0];
	*wakeup_fd = fds[1];
	return 0;

error:
	return ret;
}

/*
 * Returns 0 on success, negative error value on error.
 */
int ustcomm_send_reg_msg(int sock,
		enum ustctl_socket_type type,
		uint32_t bits_per_long,
		uint32_t uint8_t_alignment,
		uint32_t uint16_t_alignment,
		uint32_t uint32_t_alignment,
		uint32_t uint64_t_alignment,
		uint32_t long_alignment)
{
	ssize_t len;
	struct ustctl_reg_msg reg_msg;

	reg_msg.magic = LTTNG_UST_COMM_MAGIC;
	reg_msg.major = LTTNG_UST_ABI_MAJOR_VERSION;
	reg_msg.minor = LTTNG_UST_ABI_MINOR_VERSION;
	reg_msg.pid = getpid();
	reg_msg.ppid = getppid();
	reg_msg.uid = getuid();
	reg_msg.gid = getgid();
	reg_msg.bits_per_long = bits_per_long;
	reg_msg.uint8_t_alignment = uint8_t_alignment;
	reg_msg.uint16_t_alignment = uint16_t_alignment;
	reg_msg.uint32_t_alignment = uint32_t_alignment;
	reg_msg.uint64_t_alignment = uint64_t_alignment;
	reg_msg.long_alignment = long_alignment;
	reg_msg.socket_type = type;
	lttng_ust_getprocname(reg_msg.name);
	memset(reg_msg.padding, 0, sizeof(reg_msg.padding));

	len = ustcomm_send_unix_sock(sock, &reg_msg, sizeof(reg_msg));
	if (len > 0 && len != sizeof(reg_msg))
		return -EIO;
	if (len < 0)
		return len;
	return 0;
}

static
int serialize_string_encoding(enum ustctl_string_encodings *ue,
		enum lttng_string_encodings le)
{
	switch (le) {
	case lttng_encode_none:
		*ue = ustctl_encode_none;
		break;
	case lttng_encode_UTF8:
		*ue = ustctl_encode_UTF8;
		break;
	case lttng_encode_ASCII:
		*ue = ustctl_encode_ASCII;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static
int serialize_basic_type(enum ustctl_abstract_types *uatype,
		enum lttng_abstract_types atype,
		union _ustctl_basic_type *ubt,
		const union _lttng_basic_type *lbt)
{
	switch (atype) {
	case atype_integer:
	{
		struct ustctl_integer_type *uit;
		const struct lttng_integer_type *lit;

		uit = &ubt->integer;
		lit = &lbt->integer;
		uit->size = lit->size;
		uit->signedness = lit->signedness;
		uit->reverse_byte_order = lit->reverse_byte_order;
		uit->base = lit->base;
		if (serialize_string_encoding(&uit->encoding, lit->encoding))
			return -EINVAL;
		uit->alignment = lit->alignment;
		*uatype = ustctl_atype_integer;
		break;
	}
	case atype_string:
	{
		if (serialize_string_encoding(&ubt->string.encoding,
				lbt->string.encoding))
			return -EINVAL;
		*uatype = ustctl_atype_string;
		break;
	}
	case atype_float:
	{
		struct ustctl_float_type *uft;
		const struct lttng_float_type *lft;

		uft = &ubt->_float;
		lft = &lbt->_float;
		uft->exp_dig = lft->exp_dig;
		uft->mant_dig = lft->mant_dig;
		uft->alignment = lft->alignment;
		uft->reverse_byte_order = lft->reverse_byte_order;
		*uatype = ustctl_atype_float;
		break;
	}
	case atype_enum:
	{
		strncpy(ubt->enumeration.name, lbt->enumeration.name,
				LTTNG_UST_SYM_NAME_LEN);
		ubt->enumeration.name[LTTNG_UST_SYM_NAME_LEN - 1] = '\0';
		*uatype = ustctl_atype_enum;
		break;
	}
	case atype_array:
	case atype_sequence:
	default:
		return -EINVAL;
	}
	return 0;
}

static
int serialize_one_type(struct ustctl_type *ut, const struct lttng_type *lt)
{
	int ret;

	switch (lt->atype) {
	case atype_integer:
	case atype_float:
	case atype_string:
	case atype_enum:
		ret = serialize_basic_type(&ut->atype, lt->atype,
			&ut->u.basic, &lt->u.basic);
		if (ret)
			return ret;
		break;
	case atype_array:
	{
		struct ustctl_basic_type *ubt;
		const struct lttng_basic_type *lbt;
		int ret;

		ubt = &ut->u.array.elem_type;
		lbt = &lt->u.array.elem_type;
		ut->u.array.length = lt->u.array.length;
		ret = serialize_basic_type(&ubt->atype, lbt->atype,
			&ubt->u.basic, &lbt->u.basic);
		if (ret)
			return -EINVAL;
		ut->atype = ustctl_atype_array;
		break;
	}
	case atype_sequence:
	{
		struct ustctl_basic_type *ubt;
		const struct lttng_basic_type *lbt;
		int ret;

		ubt = &ut->u.sequence.length_type;
		lbt = &lt->u.sequence.length_type;
		ret = serialize_basic_type(&ubt->atype, lbt->atype,
			&ubt->u.basic, &lbt->u.basic);
		if (ret)
			return -EINVAL;
		ubt = &ut->u.sequence.elem_type;
		lbt = &lt->u.sequence.elem_type;
		ret = serialize_basic_type(&ubt->atype, lbt->atype,
			&ubt->u.basic, &lbt->u.basic);
		if (ret)
			return -EINVAL;
		ut->atype = ustctl_atype_sequence;
		break;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static
int serialize_fields(size_t *_nr_write_fields,
		struct ustctl_field **ustctl_fields,
		size_t nr_fields,
		const struct lttng_event_field *lttng_fields)
{
	struct ustctl_field *fields;
	int i, ret;
	size_t nr_write_fields = 0;

	fields = zmalloc(nr_fields * sizeof(*fields));
	if (!fields)
		return -ENOMEM;

	for (i = 0; i < nr_fields; i++) {
		struct ustctl_field *f;
		const struct lttng_event_field *lf;

		f = &fields[nr_write_fields];
		lf = &lttng_fields[i];

		/* skip 'nowrite' fields */
		if (lf->nowrite)
			continue;
		strncpy(f->name, lf->name, LTTNG_UST_SYM_NAME_LEN);
		f->name[LTTNG_UST_SYM_NAME_LEN - 1] = '\0';
		ret = serialize_one_type(&f->type, &lf->type);
		if (ret)
			goto error_type;
		nr_write_fields++;
	}

	*_nr_write_fields = nr_write_fields;
	*ustctl_fields = fields;
	return 0;

error_type:
	free(fields);
	return ret;
}

static
int serialize_enum(struct ustctl_enum *uenum,
		const struct lttng_enum *lenum)
{
	int i;
	struct ustctl_integer_type *uit;
	const struct lttng_integer_type *lit;
	struct ustctl_enum_entry *entries;

	strncpy(uenum->name, lenum->name, LTTNG_UST_SYM_NAME_LEN);
	uenum->name[LTTNG_UST_SYM_NAME_LEN - 1] = '\0';

	/* Serialize the container_type */
	uit = &uenum->container_type;
	lit = &lenum->container_type;
	uit->size = lit->size;
	uit->signedness = lit->signedness;
	uit->reverse_byte_order = lit->reverse_byte_order;
	uit->base = lit->base;
	if (serialize_string_encoding(&uit->encoding, lit->encoding))
		return -EINVAL;
	uit->alignment = lit->alignment;

	/* Serialize the entries */
	entries = zmalloc(lenum->len * sizeof(*entries));
	if (!entries)
		return -ENOMEM;
	for (i = 0; i < lenum->len; i++) {
		struct ustctl_enum_entry *uentry;
		const struct lttng_enum_entry *lentry;

		uentry = &entries[i];
		lentry = &lenum->entries[i];

		uentry->start = lentry->start;
		uentry->end = lentry->end;
		strncpy(uentry->string, lentry->string, LTTNG_UST_SYM_NAME_LEN);
		uentry->string[LTTNG_UST_SYM_NAME_LEN - 1] = '\0';
	}
	uenum->entries = entries;
	uenum->len = lenum->len;
	return 0;
}

static
int serialize_global_type_decl(size_t *_nr_write_global_type_decl,
		struct ustctl_global_type_decl **ustctl_global_type,
		size_t nr_global_type_decl,
		const struct lttng_global_type_decl *lttng_global_type)
{
	struct ustctl_global_type_decl *global_type_decl;
	int i, ret;
	size_t nr_write_global_type_decl = 0;

	global_type_decl = zmalloc(nr_global_type_decl * sizeof(*global_type_decl));
	if (!global_type_decl)
		return -ENOMEM;

	for (i = 0; i < nr_global_type_decl; i++) {
		struct ustctl_global_type_decl *f;
		const struct lttng_global_type_decl *lf;

		f = &global_type_decl[nr_write_global_type_decl];
		lf = &lttng_global_type[i];

		/* skip 'nowrite' fields */
		if (lf->nowrite)
			continue;

		/* do the serialize here */
		switch (lf->mtype) {
		case mtype_enum:
		{
			struct ustctl_enum *ue;
			const struct lttng_enum *le;

			ue = &f->u.ctf_enum;
			le = lf->u.ctf_enum;
			ret = serialize_enum(ue, le);
			if (ret)
				goto error;

			f->mtype = ustctl_mtype_enum;
			break;
		}
		default:
			ret = -EINVAL;
			goto error;
		}

		nr_write_global_type_decl++;
	}

	*_nr_write_global_type_decl = nr_write_global_type_decl;
	*ustctl_global_type = global_type_decl;
	return 0;

error:
	/* Free what has been allocated during global type serialization */
	for (i = 0; i < nr_write_global_type_decl; i++) {
		struct ustctl_global_type_decl *m;
		m = &global_type_decl[i];
		switch (m->mtype) {
		case ustctl_mtype_enum:
			free(m->u.ctf_enum.entries);
			break;
		default:
			break;
		}
	}
	free(global_type_decl);
	return ret;

}

static
int serialize_ctx_fields(size_t *_nr_write_fields,
		struct ustctl_field **ustctl_fields,
		size_t nr_fields,
		const struct lttng_ctx_field *lttng_fields)
{
	struct ustctl_field *fields;
	int i, ret;
	size_t nr_write_fields = 0;

	fields = zmalloc(nr_fields * sizeof(*fields));
	if (!fields)
		return -ENOMEM;

	for (i = 0; i < nr_fields; i++) {
		struct ustctl_field *f;
		const struct lttng_event_field *lf;

		f = &fields[nr_write_fields];
		lf = &lttng_fields[i].event_field;

		/* skip 'nowrite' fields */
		if (lf->nowrite)
			continue;
		strncpy(f->name, lf->name, LTTNG_UST_SYM_NAME_LEN);
		f->name[LTTNG_UST_SYM_NAME_LEN - 1] = '\0';
		ret = serialize_one_type(&f->type, &lf->type);
		if (ret)
			goto error_type;
		nr_write_fields++;
	}

	*_nr_write_fields = nr_write_fields;
	*ustctl_fields = fields;
	return 0;

error_type:
	free(fields);
	return ret;
}

/*
 * Returns 0 on success, negative error value on error.
 */
int ustcomm_register_event(int sock,
	int session_objd,		/* session descriptor */
	int channel_objd,		/* channel descriptor */
	const char *event_name,		/* event name (input) */
	int loglevel,
	const char *signature,		/* event signature (input) */
	size_t nr_fields,		/* fields */
	const struct lttng_event_field *lttng_fields,
	const char *model_emf_uri,
	uint32_t *id,			/* event id (output) */
	size_t nr_global_type_decl,
	const struct lttng_global_type_decl *lttng_global_types) /* global type declarations */
{
	ssize_t len;
	struct {
		struct ustcomm_notify_hdr header;
		struct ustcomm_notify_event_msg m;
	} msg;
	struct {
		struct ustcomm_notify_hdr header;
		struct ustcomm_notify_event_reply r;
	} reply;
	size_t signature_len, fields_len, model_emf_uri_len, global_type_len;
	struct ustctl_field *fields = NULL;
	struct ustctl_global_type_decl *global_type_decl = NULL;
	size_t nr_write_fields = 0;
	size_t nr_write_global_type_decl = 0;
	int ret, i;

	memset(&msg, 0, sizeof(msg));
	msg.header.notify_cmd = USTCTL_NOTIFY_CMD_EVENT;
	msg.m.session_objd = session_objd;
	msg.m.channel_objd = channel_objd;
	strncpy(msg.m.event_name, event_name, LTTNG_UST_SYM_NAME_LEN);
	msg.m.event_name[LTTNG_UST_SYM_NAME_LEN - 1] = '\0';
	msg.m.loglevel = loglevel;
	signature_len = strlen(signature) + 1;
	msg.m.signature_len = signature_len;

	/* Calculate fields len, serialize fields. */
	if (nr_fields > 0) {
		ret = serialize_fields(&nr_write_fields, &fields,
				nr_fields, lttng_fields);
		if (ret)
			return ret;
	}

	fields_len = sizeof(*fields) * nr_write_fields;
	msg.m.fields_len = fields_len;
	if (model_emf_uri) {
		model_emf_uri_len = strlen(model_emf_uri) + 1;
	} else {
		model_emf_uri_len = 0;
	}
	msg.m.model_emf_uri_len = model_emf_uri_len;
	/* Calculate global type declarations len, serialize global types. */
	if (nr_global_type_decl > 0) {
		ret = serialize_global_type_decl(&nr_write_global_type_decl, &global_type_decl,
				nr_global_type_decl, lttng_global_types);
		if (ret) {
			free(fields);
			return ret;
		}
	}
	global_type_len = sizeof(*global_type_decl) * nr_write_global_type_decl;
	msg.m.global_type_decl_len = global_type_len;

	len = ustcomm_send_unix_sock(sock, &msg, sizeof(msg));
	if (len > 0 && len != sizeof(msg)) {
		ret = -EIO;
		goto error_fields;
	}
	if (len < 0) {
		ret = len;
		goto error_fields;
	}

	/* send signature */
	len = ustcomm_send_unix_sock(sock, signature, signature_len);
	if (len > 0 && len != signature_len) {
		free(fields);
		return -EIO;
	}
	if (len < 0) {
		free(fields);
		return len;
	}

	/* send fields */
	if (fields_len > 0) {
		len = ustcomm_send_unix_sock(sock, fields, fields_len);
		free(fields);
		if (len > 0 && len != fields_len) {
			ret = -EIO;
			goto error_global_type;
		}
		if (len < 0) {
			ret = len;
			goto error_global_type;
		}
	} else {
		free(fields);
	}

	if (model_emf_uri_len) {
		/* send model_emf_uri */
		len = ustcomm_send_unix_sock(sock, model_emf_uri,
				model_emf_uri_len);
		if (len > 0 && len != model_emf_uri_len) {
			ret = -EIO;
			goto error_global_type;
		}
		if (len < 0) {
			ret = len;
			goto error_global_type;
		}
	}

	/* Send global type declarations */
	if (global_type_len > 0) {
		len = ustcomm_send_unix_sock(sock, global_type_decl, global_type_len);
		DBG("Sending global type declarations for event %s.\n", event_name);
		if (len > 0 && len != global_type_len) {
			goto error_global_type;
		}
		if (len < 0) {
			goto error_global_type;
		}
		/* Send extra global types information */
		for (i = 0; i < nr_write_global_type_decl; i++) {
			struct ustctl_global_type_decl *one_global_type;
			one_global_type = &global_type_decl[i];

			switch (one_global_type->mtype) {
			case ustctl_mtype_enum:
			{
				int entry_len = one_global_type->u.ctf_enum.len * sizeof(*one_global_type->u.ctf_enum.entries);

				/* Send the entries */
				DBG("Sending entries for global type enumeration %s.\n",
						one_global_type->u.ctf_enum.name);
				len = ustcomm_send_unix_sock(sock, one_global_type->u.ctf_enum.entries, entry_len);
				free(one_global_type->u.ctf_enum.entries);
				one_global_type->u.ctf_enum.entries = NULL;
				if (len > 0 && len != entry_len) {
					goto error_global_type;
				}
				if (len < 0) {
					goto error_global_type;
				}
				break;
			}
			default:
				break;
			}
		}
		free(global_type_decl);

	} else {
		free(global_type_decl);
	}

	/* receive reply */
	len = ustcomm_recv_unix_sock(sock, &reply, sizeof(reply));
	switch (len) {
	case 0:	/* orderly shutdown */
		return -EPIPE;
	case sizeof(reply):
		if (reply.header.notify_cmd != msg.header.notify_cmd) {
			ERR("Unexpected result message command "
				"expected: %u vs received: %u\n",
				msg.header.notify_cmd, reply.header.notify_cmd);
			return -EINVAL;
		}
		if (reply.r.ret_code > 0)
			return -EINVAL;
		if (reply.r.ret_code < 0)
			return reply.r.ret_code;
		*id = reply.r.event_id;
		DBG("Sent register event notification for name \"%s\": ret_code %d, event_id %u\n",
			event_name, reply.r.ret_code, reply.r.event_id);
		return 0;
	default:
		if (len < 0) {
			/* Transport level error */
			if (errno == EPIPE || errno == ECONNRESET)
				len = -errno;
			return len;
		} else {
			ERR("incorrect message size: %zd\n", len);
			return len;
		}
	}

error_fields:
	free(fields);
error_global_type:
	/* First free dynamically allocated content in global_type */
	for (i = 0; i < nr_write_global_type_decl; i++) {
		struct ustctl_global_type_decl *one_global_type;
		one_global_type = &global_type_decl[i];

		switch (one_global_type->mtype) {
		case ustctl_mtype_enum:
			free(one_global_type->u.ctf_enum.entries);
			break;
		default:
			break;
		}
	}
	free(global_type_decl);
	return ret;
}

/*
 * Returns 0 on success, negative error value on error.
 * Returns -EPIPE or -ECONNRESET if other end has hung up.
 */
int ustcomm_register_channel(int sock,
	int session_objd,		/* session descriptor */
	int channel_objd,		/* channel descriptor */
	size_t nr_ctx_fields,
	const struct lttng_ctx_field *ctx_fields,
	uint32_t *chan_id,		/* channel id (output) */
	int *header_type) 		/* header type (output) */
{
	ssize_t len;
	struct {
		struct ustcomm_notify_hdr header;
		struct ustcomm_notify_channel_msg m;
	} msg;
	struct {
		struct ustcomm_notify_hdr header;
		struct ustcomm_notify_channel_reply r;
	} reply;
	size_t fields_len;
	struct ustctl_field *fields = NULL;
	int ret;
	size_t nr_write_fields = 0;

	memset(&msg, 0, sizeof(msg));
	msg.header.notify_cmd = USTCTL_NOTIFY_CMD_CHANNEL;
	msg.m.session_objd = session_objd;
	msg.m.channel_objd = channel_objd;

	/* Calculate fields len, serialize fields. */
	if (nr_ctx_fields > 0) {
		ret = serialize_ctx_fields(&nr_write_fields, &fields,
				nr_ctx_fields, ctx_fields);
		if (ret)
			return ret;
	}

	fields_len = sizeof(*fields) * nr_write_fields;
	msg.m.ctx_fields_len = fields_len;
	len = ustcomm_send_unix_sock(sock, &msg, sizeof(msg));
	if (len > 0 && len != sizeof(msg)) {
		free(fields);
		return -EIO;
	}
	if (len < 0) {
		free(fields);
		return len;
	}

	/* send fields */
	if (fields_len > 0) {
		len = ustcomm_send_unix_sock(sock, fields, fields_len);
		free(fields);
		if (len > 0 && len != fields_len) {
			return -EIO;
		}
		if (len < 0) {
			return len;
		}
	} else {
		free(fields);
	}

	len = ustcomm_recv_unix_sock(sock, &reply, sizeof(reply));
	switch (len) {
	case 0:	/* orderly shutdown */
		return -EPIPE;
	case sizeof(reply):
		if (reply.header.notify_cmd != msg.header.notify_cmd) {
			ERR("Unexpected result message command "
				"expected: %u vs received: %u\n",
				msg.header.notify_cmd, reply.header.notify_cmd);
			return -EINVAL;
		}
		if (reply.r.ret_code > 0)
			return -EINVAL;
		if (reply.r.ret_code < 0)
			return reply.r.ret_code;
		*chan_id = reply.r.chan_id;
		switch (reply.r.header_type) {
		case 1:
		case 2:
			*header_type = reply.r.header_type;
			break;
		default:
			ERR("Unexpected channel header type %u\n",
				reply.r.header_type);
			return -EINVAL;
		}
		DBG("Sent register channel notification: chan_id %d, header_type %d\n",
			reply.r.chan_id, reply.r.header_type);
		return 0;
	default:
		if (len < 0) {
			/* Transport level error */
			if (errno == EPIPE || errno == ECONNRESET)
				len = -errno;
			return len;
		} else {
			ERR("incorrect message size: %zd\n", len);
			return len;
		}
	}
}

/*
 * Set socket reciving timeout.
 */
int ustcomm_setsockopt_rcv_timeout(int sock, unsigned int msec)
{
	int ret;
	struct timeval tv;

	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec * 1000 % 1000000);

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (ret < 0) {
		PERROR("setsockopt SO_RCVTIMEO");
		ret = -errno;
	}

	return ret;
}

/*
 * Set socket sending timeout.
 */
int ustcomm_setsockopt_snd_timeout(int sock, unsigned int msec)
{
	int ret;
	struct timeval tv;

	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec * 1000) % 1000000;

	ret = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (ret < 0) {
		PERROR("setsockopt SO_SNDTIMEO");
		ret = -errno;
	}

	return ret;
}
