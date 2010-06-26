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
#include "ruby.h"
#include "rubyio.h"
#include "rubysig.h"
#include "config.h"

#if defined(RUBY_PLATFORM_FREEBSD)
# include <sys/socket.h>
# include <sys/uio.h>
#elif defined(RUBY_PLATFORM_LINUX)
# include <sys/sendfile.h>
# include <unistd.h>
#elif defined(RUBY_PLATFORM_SOLARIS)
# include <sys/sendfile.h>
#endif

#if defined(RUBY_PLATFORM_FREEBSD)
static off_t __sendfile(int out, int in, off_t off, size_t count)
{
	int rv;
	off_t written, initial = off;

	while (1) {
		TRAP_BEG;
		rv = sendfile(in, out, off, count, NULL, &written, 0);
		TRAP_END;
		off += written;
		count -= written;
		if (!rv)
			break;
		if (rv < 0 && ! rb_io_wait_writable(out))
			rb_sys_fail("sendfile");
	}
	return off - initial;
}
#else
static size_t __sendfile(int out, int in, off_t off, size_t count)
{
	ssize_t rv, remaining = count;
	
	while (1) {
		TRAP_BEG;
		rv = sendfile(out, in, &off, remaining);
		TRAP_END;
		if (rv > 0)
			remaining -= rv;
		if (!remaining)
			break;
		if (rv < 0 && ! rb_io_wait_writable(out))
			rb_sys_fail("sendfile");
	}
	return count;
}
#endif

/* call-seq:
 * 	writeIO.sendfile( readIO, offset=0, count=nil) => integer
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
	int i, o;
	size_t c;
	off_t off;
	OpenFile *iptr, *optr;
	VALUE in, offset, count;

	/* get fds for files involved to pass to sendfile(2) */
	rb_scan_args(argc, argv, "12", &in, &offset, &count);
	if (TYPE(in) != T_FILE)
		rb_raise(rb_eArgError, "invalid first argument\n");
	GetOpenFile(self, optr);
	GetOpenFile(in, iptr);
	o = fileno(optr->f);
	i = fileno(iptr->f);

	/* determine offset and count parameters */
	off = (NIL_P(offset)) ? 0 : NUM2ULONG(offset);
	if (NIL_P(count)) {
		/* FreeBSD's sendfile() can take 0 as an indication to send
		 * until end of file, but Linux and Solaris can't, and anyway 
		 * we need the file size to ensure we send it all in the case
		 * of a non-blocking fd */
		struct stat s;
		if (fstat(i, &s) == -1)
			rb_sys_fail("sendfile");
		c = s.st_size;
		c -= off;
	} else {
		c = NUM2ULONG(count);
	}

	/* now send the file */
	return INT2FIX(__sendfile(o, i, off, c));
}

/* Interface to the UNIX sendfile(2) system call. Should work on FreeBSD,
 * Linux and Solaris systems that support the sendfile(2) system call.
 */
void Init_sendfile(void)
{
	rb_define_method(rb_cIO, "sendfile", rb_io_sendfile, -1);
}

