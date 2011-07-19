#!/usr/bin/env ruby


def mem_bandwidth_analyze(plan)
  use_script :plot
  use_script :mem_common

  result_groups = Hash.new{[]}

  results = get_results

  results = results.map do |ret|
    ret[:label] = "#{ret[:params][:pattern]}-#{h(ret[:params][:memsize])}-multi#{ret[:params][:multi]}-cpunode#{ret[:params][:cpunode]}-memnode#{ret[:params][:memnode]}"
    ret
  end

  results.each do |ret|
    result_groups[ret[:label]] = result_groups[ret[:label]] + [ret]
  end

  aggregated_results = result_groups.map{|label, results|
    ret = Hash.new
    keys = results.first.keys
    keys.each{|key|
      if [:cpuusage, :response_time, :throughput, :bandwidth].include?(key) ||
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
      "throughput[ops/sec],throughput err,"+
      "bandwidth[byte/sec],bandwidth err,"+
      "cpuusage[%],cpuusage err,#{oplabels.join(',')}"
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
        result[:bandwidth] <<
        result[:bandwidth_err] <<
        result[:cpuusage] <<
        result[:cpuusage_err]
      oplabels.each do |ev|
        vals << result[ev]
      end

      summary.puts vals.join(",")
      puts vals.join(",")
    end
  end

  plot_size_rt(aggregated_results)
end

def plot_size_rt(results)
  serieses = Hash.new{Array.new}

  results.each do |ret|
    serieses[[ret[:pattern], ret[:params][:cpunode], ret[:params][:memnode]]] += [ret]
  end

  datafile = File.open(common_file_name("bandwidth.tsv"), "w")
  plot_data_spec = []
  idx = 0
  serieses.each do |series, rets|
    datafile.puts("# #{series}")
    datafile.puts("# size[kb]\ops_per_sec\tbandwidth[byte]\tbandwidth_err")
    rets.sort{|x,y|
      x[:size] <=> y[:size]
    }.each{|ret|
      if ret[:size] > 0
        datafile.printf("%d\t%f\t%f\t%f\n",
                        ret[:params][:multi],
                        ret[:throughput],
                        ret[:bandwidth],
                        ret[:bandwidth_err])
      end
    }
    datafile.puts("\n\n")

    plot_data_spec.push({
                          :title => series.to_s,
                          :index => "#{idx}:#{idx}",
                          :datafile => datafile.path,
                          :using => "1:3:4",
                          :with => "yerrorlines",
                          :other_options => ""
                        })
    idx += 1
  end
  datafile.fsync

  ytics = (1..10).map do |x|
    size = 2 ** 30 * x
    sprintf('"%s" %f', h(size), size.to_f / 2.0 ** 30)
  end.join(",")

  ytics_log = (1..3).map do |e|
    e *= 10
    size = 2 ** e
    sprintf('"%s" %d', h(size), size.to_f / 2.0 ** 30)
  end.join(",")

  plot_scatter(:output => common_file_name("bandwidth.eps"),
               :gpfile => common_file_name("bandwidth.gp"),
               :xlabel => "# threads",
               :ylabel => "bandwitdh [bytes/sec]",
               :xrange => nil,
               :yrange => "[0:]",
               :title => "Memory access bandwidth",
               :plot_data => plot_data_spec,
               :other_options => <<EOS
set key left top
EOS
               )


  plot_scatter(:output => common_file_name("bandwidth-logscale.eps"),
               :gpfile => common_file_name("bandwidth-logscale.gp"),
               :xlabel => "# threads",
               :ylabel => "bandwitdh [bytes/sec]",
               :xrange => nil,
               :yrange => "[1:]",
               :title => "Memory access bandwidth",
               :plot_data => plot_data_spec,
               :other_options => <<EOS
set key left top
set logscale y
EOS
               )
end
