# 
# Copyright (c) 2005 Tobias DiPasquale
#
# Permission is hereby granted, free of charge, to any person obtaining a 
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the 
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included 
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
# DEALINGS IN THE SOFTWARE.
#

#
# $Id: extconf.rb,v 1.1 2006/03/25 01:42:02 codeslinger Exp $
#
require 'mkmf'

$config_h = ""
case RUBY_PLATFORM
when /solaris/
	have_header( "sys/sendfile.h")
	have_library( "sendfile", "sendfile")
	$config_h << "#define RUBY_PLATFORM_SOLARIS"
when /linux/
	have_header( "sys/sendfile.h")
	have_library( "c", "sendfile")
	$config_h << "#define RUBY_PLATFORM_LINUX"
when /freebsd/
	have_library( "c", "sendfile")
	$config_h << "#define RUBY_PLATFORM_FREEBSD"
end

File.open( "config.h", "w") do |f|
f.print <<EOF
#ifndef CONFIG_H
#define CONFIG_H
#{$config_h}
#endif /* CONFIG_H */
EOF
end

create_makefile( "sendfile")

