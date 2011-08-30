
require 'tempfile'
require 'jkr/plot'

$real_fork = true

def myfork(&block)
  if $real_fork
    Process.fork{
      block.call()
    }
  else
    block.call()
  end
end

def iostress_parse_result(result_id)
  result = Hash.new
  result[:id] = result_id
  with_result_file(result_id, "env_dump.marshal") do |file|
    result = result.merge(Marshal.load(file))
  end

  with_result_file(result_id, "iostress.txt") do |file|
    str = file.read
    if str =~ /iops\s+(\d+\.\d*)/
      result[:iops] = $1.to_f
    else
      raise ArgumentError.new()
    end
    if str =~ /transfer_rate\s+(\d+\.\d*)/
      result[:transfer_rate] = $1.to_f
    else
      raise ArgumentError.new()
    end
    if str =~ /response_time\s+(\d+\.\d*)/
      result[:response_time] = $1.to_f
    else
      raise ArgumentError.new()
    end
  end

  start_time = result[:params][:start_time]
  end_time = result[:params][:end_time]
  # mpstat_data = DataUtils.read_mpstat(result_file_name(result_id, "mpstat.txt")) do |block|
  #   start_time <= block[:time] && block[:time] <= end_time
  # end
  # result[:mpstat_data] = mpstat_data

  result[:iostat_data] = DataUtils.read_iostat(result_file_name(result_id, "iostat.txt")).select do |record|
    start_time <= record[0] && record[0] <= end_time
  end

  result[:iostat_steady_data] = result[:iostat_data].select do |record|
    start_time + 1 <= record[0] && record[0] <= end_time - 5
  end
  
  result[:target_dev] = target_dev = nil
  if result[:params][:device_or_file] =~ /^\/dev\/([a-z]+[0-9]*)$/
    result[:target_dev] = target_dev = $1
  end
  
  if target_dev && result[:iostat_steady_data].empty?
    result[:iostat_avg] = nil
    result[:iostat_sterr] = nil
  else
    result[:iostat_avg] = Hash.new
    result[:iostat_sterr] = Hash.new
    keys = result[:iostat_steady_data][0][1][target_dev].keys
    keys.each do |key|
      vals = result[:iostat_steady_data].map do |_, record|
        record[target_dev][key]
      end
      result[:iostat_avg][key] = vals.avg
      result[:iostat_sterr][key] = vals.sterr
    end
  end

  params = result[:params]
  result[:title] = sprintf("multi:%d,%s,%s,%s",
                           params[:multiplicity],
                           params[:mode],
                           params[:pattern],
                           params[:blocksize])

  result
end

def load_results()
  cache_file_path = common_file_name("results.cache")
  if File.exists?(cache_file_path)
    Marshal.load(File.open(cache_file_path))
  else
    results = resultset.sort.map do |result_id|
      $stderr.print("parsing #{result_id}...")
      $stderr.flush
      ret = iostress_parse_result(result_id)
      $stderr.puts("done")

      ret
    end
    Marshal.dump(results, File.open(cache_file_path, "w"))
    results
  end
end

def io_analyze(plan)
  results = load_results()

  iostress_plot_all(results)
  iostress_plot_iotrace(results)
  iostress_plot_iostat(results)
end

def hton(str)
  if str.is_a? Numeric
    return str
  end

  units = {
    'k' => 1024, 'K' => 1024,
    'm' => 1024*1024, 'M' => 1024*1024,
    'g' => 1024*1024*1024, 'G' => 1024*1024*1024,
  }

  if str =~ /(\d+)([kKmMgG]?)/
    num = $1.to_i
    unit = $2
    if unit.size > 0
      num *= units[unit]
    end
  else
    raise ArgumentError.new("#{str} is not a valid number expression")
  end
  num
end

$iostress_xtics = 'set xtics ("1Ki" 2**10, "2Ki" 2**11,"4Ki" 2**12, "8Ki" 2**13, "16Ki" 2**14, "32Ki" 2**15, "64Ki" 2**16, "128Ki" 2**17, "256Ki" 2**18, "1Mi" 2**20, "4Mi" 2**22, "16Mi" 2**24, "64Mi" 2**26)' + "\n"

def iostress_plot_all(results)
  datafile = File.open(common_file_name("allresults.tsv"), "w")

  plot_data_transfer = []
  plot_data_iops = []
  plot_data_await = []
  plot_data_avgqu = []
  data_idx = 0
  style_idx = 1
  results.group_by do |ret|
    [ret[:params][:mode],
     ret[:params][:pattern],
     ret[:params][:device],
     ret[:params][:multiplicity]]
  end.sort_by do |group_param, group|
    group_param.join("-")
  end.each do |group_param, group|
    group = group.sort_by{|ret| hton(ret[:params][:blocksize])}
    group.each do |ret|
      datafile.puts([hton(ret[:params][:blocksize]),
                     ret[:transfer_rate],
                     ret[:iops],
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['rkB/s'] : ret[:iostat_avg]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['rkB/s'] : ret[:iostat_sterr]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['r/s'] : ret[:iostat_avg]['w/s']), # iops
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['r/s'] : ret[:iostat_sterr]['w/s']),
                     ret[:iostat_avg]['await'], ret[:iostat_sterr]['await'],
                     ret[:iostat_avg]['avgqu-sz'], ret[:iostat_sterr]['avgqu-sz'],
                    ].join("\t"))
    end
    datafile.puts("\n\n")
    group.select do |ret|
      size = ret[:params][:blocksize]
      (size & (size - 1)) == 0
    end.each do |ret|
      datafile.puts([hton(ret[:params][:blocksize]),
                     ret[:transfer_rate],
                     ret[:iops],
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['rkB/s'] : ret[:iostat_avg]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['rkB/s'] : ret[:iostat_sterr]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['r/s'] : ret[:iostat_avg]['w/s']), # iops
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['r/s'] : ret[:iostat_sterr]['w/s']),
                     ret[:iostat_avg]['await'], ret[:iostat_sterr]['await'],
                     ret[:iostat_avg]['avgqu-sz'], ret[:iostat_sterr]['avgqu-sz'],
                    ].join("\t"))
    end
    datafile.puts("\n\n")
    datafile.fsync
    
    title = group_param.join("-").gsub(/sequential/, "seq").gsub(/random/, "rand")
    style_spec = "lt #{style_idx} lc #{style_idx}"
    transfer_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:4:5",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorbars #{style_spec}"
    }
    transfer_entry_pow2 = {
      :title => nil,
      :datafile => datafile.path,
      :using => "1:4",
      :index => "#{data_idx}:#{data_idx}",
      :with => "lines #{style_spec}"
    }
    await_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:8:9",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorbars #{style_spec}"
    }
    await_entry_pow2 = {
      :title => nil,
      :datafile => datafile.path,
      :using => "1:8:9",
      :index => "#{data_idx}:#{data_idx}",
      :with => "lines #{style_spec}"
    }
    avgqu_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:10:11",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorbars #{style_spec}"
    }
    avgqu_entry_pow2 = {
      :title => nil,
      :datafile => datafile.path,
      :using => "1:10:11",
      :index => "#{data_idx}:#{data_idx}",
      :with => "lines #{style_spec}"
    }
    style_idx += 1
    style_spec = "lt #{style_idx} lc #{style_idx}"
    iops_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:6:7",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorbars #{style_spec}"
    }
    iops_entry_pow2 = {
      :title => nil,
      :datafile => datafile.path,
      :using => "1:6",
      :index => "#{data_idx}:#{data_idx}",
      :with => "lines #{style_spec}"
    }
    plot_data_transfer.push(transfer_entry)
    plot_data_iops.push(iops_entry)
    plot_data_await.push(await_entry)
    plot_data_avgqu.push(avgqu_entry)
    data_idx += 1
    style_idx += 1

    data_idx += 1

    if ! Dir.exists?(common_file_name("linear-plot"))
      FileUtils.mkdir_p(common_file_name("linear-plot"))
    end
    blocksize_min = group.map{|ret| ret[:params][:blocksize]}.min
    blocksize_max = group.map{|ret| ret[:params][:blocksize]}.max
    blocksize_range = blocksize_max - blocksize_min
    plot_scatter(:output => common_file_name("#{title}.eps"),
                 :gpfile => common_file_name("#{title}.gp"),
                 :xlabel => "block size [byte]",
                 :ylabel => "transfer rate [MiB/sec]",
                 :xrange => "[#{blocksize_min/2}:#{blocksize_max*2}]",
                 :yrange => "[0:]",
                 :title => "Transfer rate on #{group.first[:params][:device_or_file]}",
                 :plot_data => [transfer_entry.merge({:title => "transfer rate"}),
                                iops_entry.merge({:other_options => "axis x1y2", :title => "iops"}),
                                transfer_entry_pow2.merge({:title => nil}),
                                iops_entry_pow2.merge({:other_options => "axis x1y2", :title => nil})],
                 :other_options => $iostress_xtics +
                 "set key left center\nset y2label 'iops [1/sec]'\nset y2tics nomirror\nset logscale x\nset y2range [0:]\n")
    plot_scatter(:output => common_file_name("linear-plot/#{title}.eps"),
                 :gpfile => common_file_name("linear-plot/#{title}.gp"),
                 :xlabel => "block size [byte]",
                 :ylabel => "transfer rate [MiB/sec]",
                 :xrange => "[#{blocksize_min - blocksize_range / 10}:#{blocksize_max + blocksize_range / 10}]",
                 :yrange => "[0:]",
                 :title => "Transfer rate on #{group.first[:params][:device_or_file]}",
                 :plot_data => [transfer_entry.merge({:title => "transfer rate"}),
                                iops_entry.merge({:other_options => "axis x1y2", :title => "iops"}),
                                transfer_entry_pow2.merge({:title => nil}),
                                iops_entry_pow2.merge({:other_options => "axis x1y2", :title => nil})],
                 :other_options => $iostress_xtics +
                 "set key bottom\nset y2label 'iops [1/sec]'\nset y2tics nomirror\nset y2range [0:]\n")
    plot_scatter(:output => common_file_name("linear-plot/#{title}-zoom.eps"),
                 :gpfile => common_file_name("linear-plot/#{title}-zoom.gp"),
                 :xlabel => "block size [byte]",
                 :ylabel => "transfer rate [MiB/sec]",
                 :xrange => "[#{blocksize_min - blocksize_range / 10}:#{blocksize_max + blocksize_range / 10}]",
                 :yrange => "[0:]",
                 :title => "Transfer rate on #{group.first[:params][:device_or_file]}",
                 :plot_data => [transfer_entry.merge({:title => "transfer rate"}),
                                iops_entry.merge({:other_options => "axis x1y2", :title => "iops"}),
                                transfer_entry_pow2.merge({:title => nil}),
                                iops_entry_pow2.merge({:other_options => "axis x1y2", :title => nil})],
                 :other_options => $iostress_xtics +
                 "set key bottom\nset y2label 'iops [1/sec]'\nset y2tics nomirror\nset y2range [0:]\n")
  end
  datafile.close

  plot_scatter(:output => common_file_name("transfer-rate.eps"),
               :gpfile => common_file_name("transfer-rate.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "transfer rate [MiB/sec]",
               :xrange => "[2**9:2**20]",
               :yrange => "[0:]",
               :title => "Transfer rate",
               :plot_data => plot_data_transfer,
               :other_options => $iostress_xtics +
               "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("response-time.eps"),
               :gpfile => common_file_name("response-time.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "response time [msec]",
               :xrange => "[2**9:2**20]",
               :yrange => "[0:]",
               :title => "Response time",
               :plot_data => plot_data_await,
               :other_options => $iostress_xtics +
               "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("response-time-zoom.eps"),
               :gpfile => common_file_name("response-time-zoom.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "response time [msec]",
               :xrange => "[2**9:2**16]",
               :yrange => "[0:0.1]",
               :title => "Response time",
               :plot_data => plot_data_await,
               :other_options => $iostress_xtics +
               "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("rq-queue-length.eps"),
               :gpfile => common_file_name("rq-queue-length.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "avg. # of request queued",
               :xrange => "[2**9:2**20]",
               :yrange => "[0:]",
               :title => "queue length",
               :plot_data => plot_data_avgqu,
               :other_options => $iostress_xtics +
               "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("linear-plot/transfer-rate.eps"),
               :gpfile => common_file_name("linear-plot/transfer-rate.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "transfer rate [MiB/sec]",
               :xrange => "[2**9:2**20]",
               :yrange => "[0:]",
               :title => "Transfer rate",
               :plot_data => plot_data_transfer,
               :other_options => $iostress_xtics +
               "set key top left\n")
  plot_scatter(:output => common_file_name("transfer-rate-zoom.eps"),
               :gpfile => common_file_name("transfer-rate-zoom.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "transfer rate [MiB/sec]",
               :xrange => "[2**9:2**12]",
               :yrange => "[0:]",
               :title => "Transfer rate",
               :plot_data => plot_data_transfer,
               :other_options => $iostress_xtics +
               "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("linear-plot/transfer-rate-zoom.eps"),
               :gpfile => common_file_name("linear-plot/transfer-rate-zoom.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "transfer rate [MiB/sec]",
               :xrange => "[2**9:2**14+2**12]",
               :yrange => "[0:]",
               :title => "Transfer rate",
               :plot_data => plot_data_transfer,
               :other_options => $iostress_xtics +
               "set key top left\n")
  plot_scatter(:output => common_file_name("iops.eps"),
               :gpfile => common_file_name("iops.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "iops [1/sec]",
               :xrange => "[2**9:2**20]",
               :yrange => "[0:]",
               :title => "IOPS",
               :plot_data => plot_data_iops,
               :other_options => $iostress_xtics +
               "set key top right\nset logscale x\n")
end

def iostress_plot_iostat(results)
  results.each do |result|
    datafile = File.open(result_file_name(result[:id], "iostat.tsv"), "w")
    start_time = result[:params][:start_time]
    target_dev = result[:target_dev]
    result[:iostat_data].each do |t, record|
      datafile.puts([t - start_time,
                     record[target_dev]['rkB/s'] / 1024,
                     record[target_dev]['wkB/s'] / 1024].map(&:to_s).join("\t"))
    end
    datafile.fsync

    plot_scatter(:output => result_file_name(result[:id], "iostat.eps"),
                 :gpfile => result_file_name(result[:id], "iostat.gp"),
                 :xlabel => "elapsed time [sec]",
                 :ylabel => "transfer rate [MB/sec]",
                 :xrange => "[0:]",
                 :yrange => "[0:]",
                 :title => "IO performance",
                 :plot_data => [{
                                  :title => "read",
                                  :datafile => datafile.path,
                                  :using => "1:2",
                                  :index => "0:0",
                                  :with => "lines"
                                },
                                {
                                  :title => "write",
                                  :datafile => datafile.path,
                                  :using => "1:3",
                                  :index => "0:0",
                                  :with => "lines"
                                }],
                 :other_options => "set key right top\n")
  end
end

def iostress_plot_iotrace(results)
  if ! results.all?{|ret| File.exists?(result_file_name(ret[:id], "iotrace.txt"))}
    return
  end
  datafile = File.open(common_file_name("nr_sectors.tsv"), "w")
  data_idx = 0
  plot_data = []
  results.group_by do |ret|
    [ret[:params][:mode],
     ret[:params][:pattern],
     ret[:params][:device]]
  end.each do |group_param, group|
    group = group.sort_by{|ret| hton(ret[:params][:blocksize])}
    num = 0
    group.each do |ret|
      avg_nr_sectors = `cut -f 6 #{result_file_name(ret[:id], 'iotrace.txt')}`.split.map(&:to_i).select{|x| x > 0}.avg
      $stderr.puts("calc avg nr_sectors for #{ret[:id]} done (#{num += 1}/#{group.size})")
      datafile.puts([hton(ret[:params][:blocksize]),
                     avg_nr_sectors,
                     ].join("\t"))
    end
    datafile.puts("\n\n")
    datafile.fsync

    title = group_param.join("-").gsub(/sequential/, "seq").gsub(/random/, "rand")
    entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:2",
      :index => "#{data_idx}:#{data_idx}",
      :with => "linespoints"
    }
    data_idx += 1
    plot_data.push(entry)
    $stderr.puts("calc avg nr_sectors for #{title} done")
  end

  plot_scatter(:output => common_file_name("nr_sectors.eps"),
               :gpfile => common_file_name("nr_sectors.gp"),
               :xlabel => "block size [byte]",
               :ylabel => "avg. # of sectors per request",
               :xrange => "[2**9:2**20]",
               :yrange => "[1:]",
               :title => "# of sectors per request",
               :plot_data => plot_data,
               :other_options => $iostress_xtics +
               "set key bottom right\nset logscale x\nset logscale y\n")

  results.each do |result|
    start_time = result[:params][:start_time]
    plot_scatter(:output => result_file_name(result[:id], "iotrace.eps"),
                 :gpfile => result_file_name(result[:id], "iotrace.gp"),
                 :xlabel => "elapsed time [sec]",
                 :ylabel => "IO request position [sector]",
                 :xrange => "[0:]",
                 :yrange => "[0:]",
                 :title => "IO trace",
                 :plot_data => [{
                                  :title => "read",
                                  :datafile => result_file_name(result[:id], "iotrace.txt"),
                                  :using => "($2/10**6-#{start_time.to_f}):5",
                                  :index => "0:0",
                                  :with => "points"
                                }],
                 :other_options => "set key left top\n")
  end
end
