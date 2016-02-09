# encoding: utf-8

require 'spec_helper'

describe IoCommand do
  it "should respond to :command_name" do
    IoCommand.should respond_to(:command_name)
  end

  it "should return the right name" do
    IoCommand.command_name.should == "io"
  end

  it "should respond to :description" do
    IoCommand.should respond_to(:description)
  end

  it "should return the right desc" do
    IoCommand.description.should =~ /io benchmark/i
  end

  describe "option parser" do
    before(:each) do
      @iocommand = IoCommand.new
      @parser = @iocommand.instance_eval('@parser')
      @options = @iocommand.instance_eval('@options')
    end

    it "should have the right banner" do
      @parser.banner.should =~ /\bio\b/
    end

    it "should return right default options" do
      @iocommand.parse_args([])
      @options[:noop].should be_false
      @options[:multi].should == 1
      @options[:affinity].should == []
      @options[:timeout].should == 60
      @options[:mode].should == :read
      @options[:rwmix].should == 0.0
      @options[:pattern].should == :seq
      @options[:bogus_comp].should == 0
      @options[:direct].should == false
      @options[:async].should == false
      @options[:aio_nr_events].should == 64
      @options[:blocksize].should == 16*1024
      @options[:offset_start].should == nil
      @options[:offset_end].should == nil
      @options[:misalign].should == 0
      @options[:verbose].should == false
      @options[:debug].should == false
      @options[:json].should == false
      # @options[].should
    end

    it "should parse noop option" do
      @iocommand.parse_args(%w|-n|)
      @options[:noop].should be_true
      @iocommand.parse_args(%w|--noop|)
      @options[:noop].should be_true
      @iocommand.parse_args([])
      @options[:noop].should be_false
    end

    it "should parse --multi option" do
      @iocommand.parse_args(%w|-m 4|)
      @options[:multi].should == 4
      @iocommand.parse_args(%w|--multi 8|)
      @options[:multi].should == 8
    end

    it "should parse --affinity option" do
      @iocommand.parse_args(%w|-a 0:c0|)
      @options[:affinity].is_a?(Array).should be_true
      @options[:affinity].size.should == 1

      @iocommand.parse_args(%w|--affinity 0:c0|)
      @options[:affinity].is_a?(Array).should be_true
      @options[:affinity].size.should == 1
    end

    it "should parse --timeout option" do
      @iocommand.parse_args(%w|-t 10|)
      @options[:timeout].should == 10

      @iocommand.parse_args(%w|--timeout 20|)
      @options[:timeout].should == 20
    end

    it "should parse mode option" do
      @iocommand.parse_args(%w|--write|)
      @options[:mode].should == :write

      @iocommand.parse_args(%w|-M 0.1|)
      @options[:mode].should == :rwmix
      @options[:rwmix].should == 0.1

      @iocommand.parse_args(%w|--rwmix 0.5|)
      @options[:mode].should == :rwmix
      @options[:rwmix].should == 0.5
    end

    it "should parse --json option" do
      @iocommand.parse_args(%w|--json|)
      @options[:json].should == true
    end

    it "should parse more options" do
      pending "TODO"
    end
  end
end
