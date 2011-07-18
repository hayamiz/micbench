#!/usr/bin/env ruby

require 'optparse'

def parse_args(argv)
  opt = Hash.new
  parser = OptionParser.new

  opt[:some_option] = 'default value'
  parser.on('-o', '--option VALUE') do |value|
    opt[:some_option] = value
  end

  opt[:random] = false
  parser.on('-R', '--random') do
    opt[:random] = true
  end

  parser.parse!(argv)
  opt
end

def main(argv)
  opt = parse_args(argv)

  if opt[:random]
    generate_rand($stdout)
  else
    generate($stdout)
  end
end

$regnum = 1 # 8

def generate_rand(out)
  # use r8 and r9
  out.puts <<EOS
__asm__ volatile(
"#rand inner loop\\n"
EOS
  (2 << 10).times do |idx|
    rnum = idx % $regnum + 8
    ofst = idx * 8
    out.puts <<EOS
"movq	(%%rax), %%r8\\n"
"movq	%%r8, (%%rax)\\n"
"movq	%%r8, %%rax\\n"

EOS
  end

  out.puts <<EOS
: "=a" (ptr)
: "0" (ptr)
: "%r8");
EOS

end

def generate(out)
  out.puts <<EOS
__asm__ volatile(
"#seq inner loop\\n"
EOS
  (1024 / 8).times do |idx|
    rnum = idx % $regnum + 8
    ofst = idx * 8
    out.puts <<EOS
"movq	#{ofst}(%%rax), %%r#{rnum}\\n"
"addq	$1, #{ofst}(%%rax)\\n"

EOS
  end

  destructed_regs = (0..($regnum - 1)).map{|i| sprintf('"%%r%d"', i+8)}.join(", ")
  out.puts <<EOS
"addq	$1024, %0\\n"
: "=a" (ptr)
: "0" (ptr)
: #{destructed_regs});
EOS

end

if __FILE__ == $0
  main(ARGV.dup)
end
