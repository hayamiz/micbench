

def parse_result(plan, result_id)
  begin
    ret = Hash.new
    params = Marshal.load(result_file(result_id, "params.dump").read)
    ret[:params] = params
    
    ret[:assign] = params[:assign]
    ret[:pattern] = params[:pattern]
    
    start_time = params[:start_time]
    end_time = params[:end_time]
    t0 = start_time + 120
    t1 = end_time - 60
    ret[:start_time] = start_time
    ret[:end_time] = end_time
    
    response_time = nil
    throughput = nil
    size = nil

    memstress = Hash.new
    with_result_file(result_id, "micbench-mem.txt") do |f|
      f.read.lines.each do |line|
        key, val = line.strip.split
        memstress[key] = val
      end
    end
    response_time = memstress["clk_per_op"].to_f
    throughput = memstress["ops_per_sec"].to_f
    bandwidth = memstress["GB_per_sec"].to_f
    size = memstress["size"].to_i

    ret[:response_time] = response_time
    ret[:throughput] = throughput
    ret[:bandwidth] = bandwidth
    ret[:size] = size

    mpstat_data = DataUtils.read_mpstat(result_file_name(result_id, "mpstat.txt")).select{|block|
      t = block[:time]
      t0 <= t && t <= t1
    }
    if mpstat_data.size == 0
      mpstat_data = DataUtils.read_mpstat(result_file_name(result_id, "mpstat.txt")).select{|block|
        t = block[:time]
        start_time <= t && t <= end_time
      }
    end

    avg_cpuusage = mpstat_data.map{|block|
      row = block[:data].find{|row_| row_[0] == "all" }
      100.0 - row[block[:labels].index("%idle")].to_f
    }.inject(&:+) / mpstat_data.size
    ret[:cpuusage] = avg_cpuusage

    plan.params[:op_events].each{|opevent|
      opfile = result_file_name(result_id, "oprofile-#{opevent}.txt")
      cmdret = `grep memstress #{opfile}|head -1|awk '{print $2}'`.strip
      ret[opevent] = if cmdret.empty?
                       "N/A"
                     else
                       cmdret.to_i
                     end
    }
    
    ret
  rescue Errno::ENOENT
    nil
  rescue StandardError => err
    raise err
    p err
    nil
  end
end

def get_results()
  if File.exists?(common_file_name("results.marshal"))
    results = Marshal.load(File.open(common_file_name("results.marshal")))
  else
    results = resultset().map do |rid|
      print "parsing #{rid}..."
      $stdout.flush
      ret = parse_result(plan, rid)
      unless ret
        $stderr.puts("Cannot parse #{rid}")
        exit(false)
      end
      puts "done"
      ret
    end

    File.open(common_file_name("results.marshal"), "w") do |file|
      Marshal.dump(results, file)
    end
  end

  results
end
