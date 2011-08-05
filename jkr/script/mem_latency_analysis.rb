require 'jkr/plot'

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
    size = memstress["size"].to_i

    ret[:response_time] = response_time
    ret[:throughput] = throughput
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

def mem_latency_analyze(plan)
  result_groups = Hash.new{[]}

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

  results = results.map do |ret|
    ret[:label] = "#{ret[:params][:pattern]}-#{h(ret[:params][:memsize])}-#{ret[:params][:assign]}"
    ret
  end

  results.each do |ret|
    result_groups[ret[:label]] = result_groups[ret[:label]] + [ret]
  end

  aggregated_results = result_groups.map{|label, results|
    ret = Hash.new
    keys = results.first.keys
    keys.each{|key|
      if [:cpuusage, :response_time, :throughput].include?(key) ||
          plan.params[:op_events].include?(key)
        vals = results.map{|r| r[key]}
        errkey = key.to_s + "_err"
        if key.is_a? Symbol
          errkey = errkey.to_sym
        end
        begin
          ret[key] = vals.avg
          ret[errkey] = vals.sterr
        rescue StandardError => err
          puts key
          puts err
          puts err.message
          puts "----"
          ret[key] = "N/A"
          ret[errkey] = "N/A"
        end
      else
        if results.map{|r| r[key]}.uniq.size > 1
          if ! [:params, :start_time, :end_time].include?(key)
            raise StandardError.new("Inconsistent data detected for key #{key}: #{results.map{|r| [r[:label], r[key]]}.inspect}")
          end
        end
        ret[key] = results.first[key]
      end
    }

    ret
  }

  aggregated_results.sort!{|r1, r2|
    x = r1[:pattern] <=> r2[:pattern]
    y = r1[:size] <=> r2[:size]
    if x == 0
      y
    else
      x
    end
  }

  with_common_file("summary.csv", "w") do |summary|
    oplabels = plan.params[:op_events].map{|ev| [ev, ev+"_err"]}.flatten
    labels = ",size[byte],clk/ops,clk/ops sterr,"+
      "throughput[ops/sec],throughput err,cpuusage[%],cpuusage err,#{oplabels.join(',')}"
    summary.puts labels
    puts labels

    aggregated_results.each do |result|
      vals = []
      vals <<
        result[:label] <<
        result[:size] <<
        result[:response_time] <<
        result[:response_time_err] <<
        result[:throughput] <<
        result[:throughput_err] <<
        result[:cpuusage] <<
        result[:cpuusage_err]
      oplabels.each do |ev|
        vals << result[ev]
      end

      summary.puts vals.join(",")
    end
  end

  plot_size_rt(aggregated_results)
end

def plot_size_rt(results)
  serieses = Hash.new{Array.new}

  results.each do |ret|
    serieses[[ret[:pattern], ret[:assign]]] += [ret]
  end

  datafile = File.open(common_file_name("response-time.tsv"), "w")
  plot_data_spec = []
  idx = 0

  min_size = 2 ** 40
  max_size = 0
  serieses.to_a.sort_by do |series, rets|
    series.join(" ")
  end.each do |series, rets|
    datafile.puts("# #{series}")
    datafile.puts("# size[kb]\tresponse_time[clk/op]\tresponse_time_err")
    rets.sort{|x,y|
      x[:size] <=> y[:size]
    }.each{|ret|
      if ret[:size] > 0
        min_size = [ret[:size], min_size].min
        max_size = [ret[:size], max_size].max
        datafile.printf("%d\t%f\t%f\n",
                        ret[:size],
                        ret[:response_time],
                        ret[:response_time_err])
      end
    }
    datafile.puts("\n\n")

    plot_data_spec.push({
                          :title => series.to_s,
                          :index => "#{idx}:#{idx}",
                          :datafile => datafile.path,
                          :using => "1:2:3",
                          :with => "yerrorlines",
                          :other_options => ""
                        })
    idx += 1
  end
  datafile.fsync
  
  xtics = results.map{|ret| ret[:size]}.uniq.sort.select{|x| x > 0}
  xtics_str = (5..16).map{|e| 2**(2*e)}.map{|size|
    if size < 2**10
      sprintf('"%d" %d', size, size)
    elsif size < 2**20
      sprintf('"%dK" %d', size/2**10, size)
    elsif size < 2**30
      sprintf('"%dM" %d', size/2**20, size)
    else
      sprintf('"%dG" %f', size/2**30, size.to_f)
    end
  }.join(", ")

  plot_scatter(:output => common_file_name("response-time.eps"),
               :gpfile => common_file_name("response-time.gp"),
               :xlabel => "allocated memory size [byte]",
               :ylabel => "response time [clk/op]",
               :xrange => nil,
               :yrange => "[0:]",
               :title => "Memory access response time",
               :plot_data => plot_data_spec,
               :other_options => <<EOS
set key left top
set logscale x
set xtics (#{xtics_str})
EOS
               )

  if min_size < 8 * 2**20
    plot_scatter(:output => common_file_name("response-time-small.eps"),
                 :gpfile => common_file_name("response-time-small.gp"),
                 :xlabel => "allocated memory size [byte]",
                 :ylabel => "response time [clk/op]",
                 :xrange => "[:#{8 * 2**20}]",
                 :yrange => "[0:]",
                 :title => "Memory access response time",
                 :plot_data => plot_data_spec,
                 :other_options => <<EOS
set key left top
set logscale x
set xtics (#{xtics_str})
EOS
                 )
  end

  plot_scatter(:output => common_file_name("response-time-logscale.eps"),
               :gpfile => common_file_name("response-time-logscale.gp"),
               :xlabel => "allocated memory size [byte]",
               :ylabel => "response time [clk/op]",
               :xrange => nil,
               :yrange => "[1:]",
               :title => "Memory access response time",
               :plot_data => plot_data_spec,
               :other_options => <<EOS
set key left top
set logscale x
set logscale y
set xtics (#{xtics_str})
EOS
               )
end
