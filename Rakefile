
require 'rspec/core/rake_task'

task :default => [:spec]

RSpec::Core::RakeTask.new(:spec) do |spec|
  spec.pattern = 'spec'
  spec.rspec_opts = ['--color']
end
