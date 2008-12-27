#!/usr/bin/env ruby
# vim:set ts=4 sw=4 ai:
require 'io/nonblock'
begin
	require 'rubygems'
rescue
	nil
end
require 'sendfile'
require 'socket'
require 'tempfile'
require 'test/unit'
require 'zlib'

class TestSendfile < Test::Unit::TestCase
	def __fork_server
		# open pipe for backchannel
		@rd, @wr = IO.pipe
		# fork server child
		@pid = fork do
			# close read end in child
			@rd.close

			# start listening and send port back to parent
			ss = TCPServer.new @host, 0
			@wr.write( [ ss.addr[1] ].pack( "S"))
			@wr.flush

			# read what they send and push it back up the pipe
			while s = ss.accept
				data = s.read
				s.close
				@wr.write( [data.length].pack( "N"))
				@wr.write data
				@wr.flush
			end
		end
		
		# close write end in parent and get server port
		@wr.close
		@port = @rd.read( 2).unpack( "S")[0]
	end

	def setup
		@dir = File.dirname __FILE__
		@host = '127.0.0.1'
		__fork_server
		
		@smallfile = "#{@dir}/small"
		@small = File.open @smallfile
		@small_data = File.read @smallfile
		
		File.open( "#{@dir}/large.gz") do |f|
			gzipped = Zlib::GzipReader.new f
			@large_data = gzipped.read
		end
		@largefile = "/tmp/sendfiletest"
		@large = File.open @largefile, 'w+'
		@large.write @large_data
		@large.flush
	end

	def teardown
		@small.close
		@large.close
		File.unlink @largefile

		Process.kill 'KILL', @pid
		Process.wait
	end

	def __do_sendfile file, off=nil, count=nil
		s = TCPSocket.new @host, @port
		yield s if block_given?
		sent = s.sendfile file, off, count
		s.close
		len = @rd.read( 4).unpack( "N")[0]
		read = @rd.read len
		[ sent, read ]
	end
		
	def test_blocking_full_small
		sent, read = __do_sendfile( @small)
		assert_equal @small_data.size, sent
		assert_equal @small_data.size, read.size
		assert_equal @small_data, read
	end

	def test_nonblocking_full_small
		sent, read = __do_sendfile( @small) { |s| s.nonblock = true }
		assert_equal @small_data.size, sent
		assert_equal @small_data.size, read.size
		assert_equal @small_data, read
	end

	def test_blocking_full_large
		sent, read = __do_sendfile( @large)
		assert_equal @large_data.size, sent
		assert_equal @large_data.size, read.size
		assert_equal @large_data, read
	end

	def test_nonblocking_full_large
		sent, read = __do_sendfile( @large) { |s| s.nonblock = true }
		assert_equal @large_data.size, sent
		assert_equal @large_data.size, read.size
		assert_equal @large_data, read
	end
	
	def test_blocking_from_offset
		data = @large_data[4096..-1]
		sent, read = __do_sendfile( @large, 4096)
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end
	
	def test_blocking_partial_from_beginning
		data = @large_data[0, 1048576]
		sent, read = __do_sendfile( @large, 0, 1048576)
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end

	def test_blocking_partial_from_middle
		data = @large_data[2048, 1048576]
		sent, read = __do_sendfile( @large, 2048, 1048576)
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end

	def test_blocking_partial_to_end
		data = @large_data[-1048576, 1048576]
		sent, read = __do_sendfile( @large, @large_data.size - 1048576, 1048576)
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end

	def test_nonblocking_from_offset
		data = @large_data[4096..-1]
		sent, read = __do_sendfile( @large, 4096) { |s| s.nonblock = true }
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end
	
	def test_nonblocking_partial_from_beginning
		data = @large_data[0, 1048576]
		sent, read = __do_sendfile( @large, 0, 1048576) { |s| s.nonblock = true }
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end

	def test_nonblocking_partial_from_middle
		data = @large_data[2048, 1048576]
		sent, read = __do_sendfile( @large, 2048, 1048576) { |s| s.nonblock = true }
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end

	def test_nonblocking_partial_to_end
		data = @large_data[-1048576, 1048576]
		sent, read = __do_sendfile( @large, @large_data.size - 1048576, 1048576) { |s| s.nonblock = true }
		assert_equal data.size, sent
		assert_equal data.size, read.size
		assert_equal data, read
	end
end		# class TestSendfile

