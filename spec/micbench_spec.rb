# encoding: utf-8

require 'spec_helper'

describe BaseCommand do
  describe "'class methods'" do
    describe "'commands'" do
      it "should respond to :commands" do
        BaseCommand.should respond_to(:commands)
      end

      it "should return empty Hash" do
        BaseCommand.commands.is_a?(Hash).should be_true
      end

    end

    describe "'command_name'" do
      it "should respond_to :command_name" do
        BaseCommand.should respond_to(:command_name)
      end

      describe "adding custom Command" do
        it "should register a new Command to commands" do
          BaseCommand.commands.should_not include('hoge')
          class HogeCommand < BaseCommand
            command_name "hoge", "test command"
          end
          BaseCommand.commands.should include('hoge')
          BaseCommand.commands['hoge'].should == HogeCommand
          BaseCommand.commands.delete('hoge')
          BaseCommand.commands.should_not include('hoge')
        end
      end
    end

    describe "'description'" do
      before(:each) do
        class TestCommand < BaseCommand
          command_name "test", "test command"
        end
      end

      it "should respond to :description" do
        BaseCommand.should respond_to(:description)
      end

      it "should have the right desc" do
        TestCommand.description.should == "test command"
      end

      after(:each) do
        BaseCommand.commands.delete('test')
      end
    end
  end
end
