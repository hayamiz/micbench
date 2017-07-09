
require 'rspec/core/rake_task'

task :default => [:spec]

RSpec::Core::RakeTask.new(:spec) do |spec|
  spec.pattern = 'spec'
  spec.rspec_opts = ['--color']
end

desc "Build micbench core execs"
task :build_core => Dir["configure.ac", "src/*.[ch]"] do
  sh "./autogen.sh"
  sh "./configure"
  sh "make"
end
