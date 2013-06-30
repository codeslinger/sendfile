# vim:set ts=4 sw=4 ai syntax=ruby:

spec = Gem::Specification.new do |gs|
	gs.name = 'sendfile'
	gs.version = '1.2.0'
	gs.summary = 'Ruby interface to sendfile(2) system call'
	gs.description = <<-EOF
Allows Ruby programs to access sendfile(2) functionality on 
any IO object. Works on Linux, Solaris, FreeBSD and Darwin with
blocking and non-blocking sockets.
EOF
	gs.author = 'Toby DiPasquale'
	gs.email = 'toby@cbcg.net'
	gs.rubyforge_project = 'ruby-sendfile'

	gs.autorequire = 'sendfile'
	gs.files = File.read('FILES').split($/)
	gs.test_files = Dir.glob 'test/test_*.rb'
	gs.extensions << 'ext/extconf.rb'

	gs.has_rdoc = true
	gs.extra_rdoc_files = %w(README.textile)
	gs.required_ruby_version = '>= 1.8.0'
end

