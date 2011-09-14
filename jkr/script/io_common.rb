$iohub_latency_xtics = 'set xtics ("2Ki" 2**10,"4Ki" 2**12, "8Ki" 2**13, "16Ki" 2**14, "32Ki" 2**15, "64Ki" 2**16, "128Ki" 2**17, "256Ki" 2**18, "1Mi" 2**20, "4Mi" 2**22, "16Mi" 2**24, "64Mi" 2**26)' + "\n"


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
  if result[:params][:device_or_file] =~ /^\/dev\/((?:[a-z]+[0-9]*\/?)+)$/
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

def plot_iostat(results)
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
