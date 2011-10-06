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
    
  end
end
