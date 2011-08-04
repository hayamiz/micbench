#!/usr/bin/env ruby

require 'optparse'

def parse_args(argv)
  opt = Hash.new
  parser = OptionParser.new

  opt[:short] = false
  parser.on('-s', '--short REGION_SIZE') do |region_size|
    opt[:short] = region_size.to_i
    if opt[:short] % 64 != 0
      $stderr.puts("Argument of --short must be multiple of 64.")
      exit(false)
    end
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
    if opt[:short]
      generate_seq_short($stdout, opt[:short])
    else
      generate_seq($stdout)
    end
  end
end

$regnum = 1 # 8

def generate_rand(out)
  num_ops = 2 << 7
  out.puts <<EOS
#define	MEM_INNER_LOOP_RANDOM_NUM_OPS	#{num_ops}
__asm__ volatile(
"#rand inner loop\\n"
EOS
  num_ops.times do |idx|
    out.puts <<EOS
"movq	(%%rax), %%rax\\n"

EOS

#     out.puts <<EOS
# "movq	(%%rax), %%r8\\n"
# // "movq	%%r8, (%%rax)\\n"
# "movq	%%r8, %%rax\\n"
# 
# EOS
  end

  out.puts <<EOS
: "=a" (ptr)
: "0" (ptr)
);
EOS

end

def generate_seq(out)
  region_size = 1024 # bytes
  stride_size = 16 # 128bit-wide SSE register
  num_register = 16 # xmm* SSE registers
  out.puts <<EOS
#define	MEM_INNER_LOOP_SEQ_NUM_OPS	#{(region_size / stride_size)}
#define	MEM_INNER_LOOP_SEQ_REGION_SIZE	#{(region_size)}
#define	MEM_INNER_LOOP_SEQ_STRIDE_SIZE	#{(stride_size)}
__asm__ volatile(
"#seq inner loop\\n"
EOS
  (region_size / stride_size).times do |idx|
    rnum = idx % num_register
    ofst = idx * stride_size
    out.puts <<EOS
"movdqa	#{ofst}(%%rax), %%xmm#{rnum}\\n"

EOS
  end

  destructed_regs = (0..15).map{|i| sprintf('"%%xmm%d"', i)}.join(", ")
  out.puts <<EOS
"addq	$#{region_size}, %0\\n"
: "=a" (ptr)
: "0" (ptr)
: #{destructed_regs});
EOS
end

def generate_seq_short(out, region_size = 64)
  min_total_access_size = 1024

  if region_size < min_total_access_size
    iter_count = min_total_access_size / region_size
  else
    iter_count = 1
  end

  region_size # bytes
  stride_size = 16 # 128bit-wide SSE register
  num_register = [region_size / stride_size, 16].min # xmm* SSE registers
  out.puts <<EOS
#define	MEM_INNER_LOOP_SEQ_#{region_size}_NUM_OPS	#{(region_size / stride_size) * iter_count}
#define	MEM_INNER_LOOP_SEQ_#{region_size}_REGION_SIZE	#{(region_size)}
#define	MEM_INNER_LOOP_SEQ_#{region_size}_STRIDE_SIZE	#{(stride_size)}
__asm__ volatile(
"#seq inner loop\\n"
EOS
  iter_count.times do
    (region_size / stride_size).times do |idx|
      rnum = idx % num_register
      ofst = idx * stride_size
      out.puts <<EOS
"movdqa	#{ofst}(%%rax), %%xmm#{rnum}\\n"

EOS
    end
  end

  destructed_regs = (0..(num_register - 1)).map{|i| sprintf('"%%xmm%d"', i)}.join(", ")
  out.puts <<EOS
"addq	$#{region_size}, %0\\n"
: "=a" (ptr)
: "0" (ptr)
: #{destructed_regs});
EOS
end

if __FILE__ == $0
  main(ARGV.dup)
end
