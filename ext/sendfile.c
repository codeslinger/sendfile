/*
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2005,2008 Tobias DiPasquale
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * ------------------------------------------------------------------------
 *
 * Ruby binding for the UNIX sendfile(2) facility. Should work on FreeBSD,
 * Linux and Solaris systems that support sendfile(2).
 *
 * Original Author: Toby DiPasquale <toby@cbcg.net>
 * Current Maintainer: Toby DiPasquale <toby@cbcg.net>
 *
 * $Id: sendfile.c,v 1.4 2006/03/27 19:14:53 codeslinger Exp $
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include "ruby.h"
#ifdef HAVE_RUBY_IO_H
#  include "ruby/io.h"
#else
#  include "rubyio.h"
#endif
#include <unistd.h>
#include <fcntl.h>
#include "config.h"

#ifndef HAVE_RB_THREAD_BLOCKING_REGION
/* (very) partial emulation of the 1.9 rb_thread_blocking_region under 1.8 */
#  include <rubysig.h>
#  define RUBY_UBF_IO ((rb_unblock_function_t *)-1)
typedef void rb_unblock_function_t(void *);
typedef VALUE rb_blocking_function_t(void *);
static VALUE
rb_thread_blocking_region(
	rb_blocking_function_t *fn, void *data1,
	rb_unblock_function_t *ubf, void *data2)
{
	VALUE rv;

	TRAP_BEG;
	rv = fn(data1);
	TRAP_END;

	return rv;
}
#endif /* ! HAVE_RB_THREAD_BLOCKING_REGION */

#if defined(RUBY_PLATFORM_FREEBSD)
# include <sys/socket.h>
# include <sys/uio.h>
#elif defined(RUBY_PLATFORM_LINUX)
# include <sys/sendfile.h>
# include <unistd.h>
#elif defined(RUBY_PLATFORM_SOLARIS)
# include <sys/sendfile.h>
#endif

#define MAX_SEND_SIZE ((off_t)LONG_MAX)

static size_t count_max(off_t count)
{
	return (size_t)(count > MAX_SEND_SIZE ? MAX_SEND_SIZE : count);
}

struct sendfile_args {
	int out;
	int in;
	off_t off;
	off_t count;
};

#if ! HAVE_RB_IO_T
#  define rb_io_t OpenFile
#endif

#ifdef GetReadFile
#  define FPTR_TO_FD(fptr) (fileno(GetReadFile(fptr)))
#else
#  if !HAVE_RB_IO_T || (RUBY_VERSION_MAJOR == 1 && RUBY_VERSION_MINOR == 8)
#    define FPTR_TO_FD(fptr) fileno(fptr->f)
#  else
#    define FPTR_TO_FD(fptr) fptr->fd
#  endif
#endif

static int my_rb_fileno(VALUE io)
{
	rb_io_t *fptr;

	GetOpenFile(io, fptr);

	return FPTR_TO_FD(fptr);
}

#if defined(RUBY_PLATFORM_FREEBSD)
static VALUE nogvl_sendfile(void *data)
{
	struct sendfile_args *args = data;
	int rv;
	off_t written;
	size_t w = count_max(args->count);

	rv = sendfile(args->in, args->out, args->off, args->count,
	              NULL, &written, 0);
	args->off += written;
	args->count -= written;

	return (VALUE)rv;
}
#else
static VALUE nogvl_sendfile(void *data)
{
	ssize_t rv;
	struct sendfile_args *args = data;
	size_t w = count_max(args->count);

	rv = sendfile(args->out, args->in, &args->off, w);
	if (rv > 0)
		args->count -= rv;

	return (VALUE)rv;
}
#endif

static off_t sendfile_full(struct sendfile_args *args)
{
	ssize_t rv;
	off_t all = args->count;

	while (1) {
		rv = (ssize_t)rb_thread_blocking_region(nogvl_sendfile, args,
		                                        RUBY_UBF_IO, NULL);
		if (!args->count)
			break;
		if (rv < 0 && ! rb_io_wait_writable(args->out))
			rb_sys_fail("sendfile");
	}
	return all;
}

static off_t sendfile_nonblock(struct sendfile_args *args)
{
	ssize_t rv;
	off_t before = args->count;
	int flags;

	flags = fcntl(args->out, F_GETFL);
	if (flags == -1)
		rb_sys_fail("fcntl");
	if ((flags & O_NONBLOCK) == 0) {
		if (fcntl(args->out, F_SETFL, flags | O_NONBLOCK) == -1)
			rb_sys_fail("fcntl");
	}

	rv = (ssize_t)rb_thread_blocking_region(nogvl_sendfile, args,
	                                        RUBY_UBF_IO, NULL);
	if (rv < 0)
		rb_sys_fail("sendfile");

	return before - args->count;
}

static void convert_args(int argc, VALUE *argv, VALUE self,
                         struct sendfile_args *args)
{
	VALUE in, offset, count;

	/* get fds for files involved to pass to sendfile(2) */
	rb_scan_args(argc, argv, "12", &in, &offset, &count);
	in = rb_convert_type(in, T_FILE, "IO", "to_io");
	args->out = my_rb_fileno(self);
	args->in = my_rb_fileno(in);

	/* determine offset and count parameters */
	args->off = (NIL_P(offset)) ? 0 : NUM2OFFT(offset);
	if (NIL_P(count)) {
		/* FreeBSD's sendfile() can take 0 as an indication to send
		 * until end of file, but Linux and Solaris can't, and anyway 
		 * we need the file size to ensure we send it all in the case
		 * of a non-blocking fd */
		struct stat s;
		if (fstat(args->in, &s) == -1)
			rb_sys_fail("sendfile");
		args->count = s.st_size;
		args->count -= args->off;
	} else {
		args->count = NUM2OFFT(count);
	}
}

/* call-seq:
 *	writeIO.sendfile( readIO, offset=0, count=nil) => integer
 *
 * Transfers count bytes starting at offset from readIO directly to writeIO
 * without copying (i.e. invoking the kernel to do it for you).
 *
 * If offset is omitted, transfer starts at the beginning of the file.
 *
 * If count is omitted, the full length of the file will be sent.
 *
 * Returns the number of bytes sent on success. Will throw system error
 * exception on error. (check man sendfile(2) on your platform for
 * information on what errors could result and how to handle them)
 */
static VALUE rb_io_sendfile(int argc, VALUE *argv, VALUE self)
{
	struct sendfile_args args;

	convert_args(argc, argv, self, &args);

	/* now send the file */
	return OFFT2NUM(sendfile_full(&args));
}

/* call-seq:
 *	writeIO.sendfile_nonblock(readIO, offset=0, count=nil) => integer
 *
 * Transfers count bytes starting at offset from readIO directly to writeIO
 * without copying (i.e. invoking the kernel to do it for you).
 *
 * Unlike IO#sendfile, this will set the O_NONBLOCK flag on writeIO
 * before calling sendfile(2) and will raise Errno::EAGAIN instead
 * of blocking.  This method is intended for use with non-blocking
 * event frameworks, including those that rely on Fibers.
 *
 * If offset is omitted, transfer starts at the beginning of the file.
 *
 * If count is omitted, the full length of the file will be sent.
 *
 * Returns the number of bytes sent on success. Will throw system error
 * exception on error. (check man sendfile(2) on your platform for
 * information on what errors could result and how to handle them)
 */
static VALUE rb_io_sendfile_nonblock(int argc, VALUE *argv, VALUE self)
{
	struct sendfile_args args;

	convert_args(argc, argv, self, &args);

	return OFFT2NUM(sendfile_nonblock(&args));
}

/* Interface to the UNIX sendfile(2) system call. Should work on FreeBSD,
 * Linux and Solaris systems that support the sendfile(2) system call.
 */
void Init_sendfile(void)
{
	rb_define_method(rb_cIO, "sendfile", rb_io_sendfile, -1);
	rb_define_method(rb_cIO, "sendfile_nonblock",
	                 rb_io_sendfile_nonblock, -1);
}

