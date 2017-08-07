# encoding: utf-8

require 'spec_helper'

describe IoCommand do
  it "should respond to :command_name" do
    expect(IoCommand).to respond_to(:command_name)
  end

  it "should return the right name" do
    expect(IoCommand.command_name).to eq("io")
  end

  it "should respond to :description" do
    expect(IoCommand).to respond_to(:description)
  end

  it "should return the right desc" do
    expect(IoCommand.description).to match(/io benchmark/i)
  end

  describe "option parser" do
    before(:each) do
      @iocommand = IoCommand.new
      @parser = @iocommand.instance_eval('@parser')
      @options = @iocommand.instance_eval('@options')
    end

    it "should have the right banner" do
      expect(@parser.banner).to match(/\bio\b/)
    end

    it "should return right default options" do
      @iocommand.parse_args([])
      expect(@options[:noop]).to be_falsey
      expect(@options[:multi]).to eq(1)
      expect(@options[:affinity]).to eq([])
      expect(@options[:timeout]).to eq(60)
      expect(@options[:mode]).to eq(:read)
      expect(@options[:rwmix]).to eq(0.0)
      expect(@options[:pattern]).to eq(:seq)
      expect(@options[:bogus_comp]).to eq(0)
      expect(@options[:direct]).to eq(false)
      expect(@options[:async]).to eq(false)
      expect(@options[:aio_nr_events]).to eq(64)
      expect(@options[:blocksize]).to eq(4*1024)
      expect(@options[:offset_start]).to eq(nil)
      expect(@options[:offset_end]).to eq(nil)
      expect(@options[:misalign]).to eq(0)
      expect(@options[:verbose]).to eq(false)
      expect(@options[:debug]).to eq(false)
      expect(@options[:json]).to eq(true)
      # @options[].should
    end

    it "should parse noop option" do
      @iocommand.parse_args(%w|-n|)
      expect(@options[:noop]).to be_truthy
      @iocommand.parse_args(%w|--noop|)
      expect(@options[:noop]).to be_truthy
      @iocommand.parse_args([])
      expect(@options[:noop]).to be_falsey
    end

    it "should parse --multi option" do
      @iocommand.parse_args(%w|-m 4|)
      expect(@options[:multi]).to eq(4)
      @iocommand.parse_args(%w|--multi 8|)
      expect(@options[:multi]).to eq(8)
    end

    it "should parse --affinity option" do
      @iocommand.parse_args(%w|-a 0:c0|)
      expect(@options[:affinity]).to be_a(Array)
      expect(@options[:affinity].size).to eq(1)

      @iocommand.parse_args(%w|--affinity 0:c0|)
      expect(@options[:affinity]).to be_a(Array)
      expect(@options[:affinity].size).to eq(1)
    end

    it "should parse --timeout option" do
      @iocommand.parse_args(%w|-t 10|)
      expect(@options[:timeout]).to eq(10)

      @iocommand.parse_args(%w|--timeout 20|)
      expect(@options[:timeout]).to eq(20)
    end

    it "should parse mode option" do
      @iocommand.parse_args(%w|--write|)
      expect(@options[:mode]).to eq(:write)

      @iocommand.parse_args(%w|-M 0.1|)
expect(@options[:mode]).to eq(:rwmix)
expect(@options[:rwmix]).to eq(0.1)

      @iocommand.parse_args(%w|--rwmix 0.5|)
expect(@options[:mode]).to eq(:rwmix)
expect(@options[:rwmix]).to eq(0.5)
    end

    it "should parse --json option" do
      @iocommand.parse_args(%w|--json|)
expect(@options[:json]).to eq(true)
    end

  end
end

describe "io subcommand" do
  before(:all) do
    @test_file = `mktemp`.strip
    system("dd if=/dev/zero of=#{@test_file} bs=32MB count=1")
  end

  after(:all) do
    FileUtils.rm(@test_file)
  end

  describe "JSON output format check: with --noop option" do
    before(:each) do
      run("#{micbench_bin} io --noop #{@test_file}")
    end

    it do
      expect(last_command_started).to be_successfully_executed

      expected_json = <<EOS
{
  "params": {
    "threads": 1,
    "mode": "read",
    "pattern": "sequential",
    "blocksize_byte": 4096,
    "offset_start_blk": -1,
    "offset_end_blk": -1,
    "direct": false,
    "aio": false,
    "aio_nr_events": 64,
    "timeout_sec": 60,
    "bogus_comp": 0,
    "iosleep": 0,
    "files": ["#{@test_file}"]
  }
}
EOS
      expect(last_command_started.stdout.to_s).to eq(expected_json)
    end
  end

  describe "JSON output format check: with actual run" do
    before(:each) do
      run("#{micbench_bin} io -t 1 #{@test_file}")
    end

    it do
      expect(last_command_started).to be_successfully_executed

      expected_json_regexp = Regexp.compile(<<EOS)
{
  "params": {
    "threads": 1,
    "mode": "read",
    "pattern": "sequential",
    "blocksize_byte": 4096,
    "offset_start_blk": -1,
    "offset_end_blk": -1,
    "direct": false,
    "aio": false,
    "aio_nr_events": 64,
    "timeout_sec": 1,
    "bogus_comp": 0,
    "iosleep": 0,
    "files": \\["#{Regexp.quote(@test_file)}"\\]
  },
  "counters": {
    "io_count": \\d+,
    "io_bytes": \\d+
  },
  "metrics": {
    "start_time_unix": \\d+\\.\\d+,
    "exec_time_sec": \\d+\\.\\d+,
    "iops": \\d+\\.\\d+,
    "transfer_rate_mbps": \\d+\\.\\d+,
    "response_time_msec": \\d+\\.\\d+,
    "accum_io_time_sec": \\d+\\.\\d+
  }
}
EOS
      expect(last_command_started.stdout.to_s).to match(expected_json_regexp)
    end
  end
end
