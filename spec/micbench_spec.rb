# encoding: utf-8

require 'spec_helper'

describe BaseCommand do
  describe "'class methods'" do
    describe "'commands'" do
      it "should respond to :commands" do
        expect(BaseCommand).to respond_to(:commands)
      end

      it "should return empty Hash" do
        expect(BaseCommand.commands).to be_a(Hash)
      end

    end

    describe "'command_name'" do
      it "should respond_to :command_name" do
        expect(BaseCommand).to respond_to(:command_name)
      end

      describe "adding custom Command" do
        it "should register a new Command to commands" do
          expect(BaseCommand.commands).not_to include('hoge')
          class HogeCommand < BaseCommand
            command_name "hoge", "test command"
          end
          expect(BaseCommand.commands).to include('hoge')
          expect(BaseCommand.commands['hoge']).to eq(HogeCommand)
          BaseCommand.commands.delete('hoge')
          expect(BaseCommand.commands).not_to include('hoge')
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
        expect(BaseCommand).to respond_to(:description)
      end

      it "should have the right desc" do
        expect(TestCommand.description).to eq("test command")
      end

      after(:each) do
        BaseCommand.commands.delete('test')
      end
    end
  end
end
