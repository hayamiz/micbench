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
      expect(@options[:blocksize]).to eq(16*1024)
      expect(@options[:offset_start]).to eq(nil)
      expect(@options[:offset_end]).to eq(nil)
      expect(@options[:misalign]).to eq(0)
      expect(@options[:verbose]).to eq(false)
      expect(@options[:debug]).to eq(false)
      expect(@options[:json]).to eq(false)
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
