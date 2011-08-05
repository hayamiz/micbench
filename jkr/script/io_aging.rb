
require 'tempfile'
require 'jkr/blktrace'
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

def iostress_analyze(plan)
  use_script :io_common

  $plan = plan
  $vars = $plan.vars.keys.select do |key|
    $plan.vars[key].size > 1
  end

  results = load_results()

  iostress_plot_all(results)
#   iostress_plot_iotrace(results)
  iostress_plot_iostat(results)
  iostress_plot_blktrace(results)
end

$iostress_xtics = 'set xtics ("2Ki" 2**10,"4Ki" 2**12, "8Ki" 2**13, "16Ki" 2**14, "32Ki" 2**15, "64Ki" 2**16, "128Ki" 2**17, "256Ki" 2**18, "1Mi" 2**20, "4Mi" 2**22, "16Mi" 2**24, "64Mi" 2**26)' + "\n"

def iostress_plot_all(results, prefix = "")
  datafile = File.open(common_file_name("allresults.tsv"), "w")

  if ! Dir.exists?(common_file_name("linear-plot"))
    FileUtils.mkdir_p(common_file_name("linear-plot"))
  end

  plot_data_transfer = []
  plot_data_iops = []
  plot_data_response_time = []
  plot_data_avgqu = []
  data_idx = 0
  style_idx = 1
  results.group_by do |ret|
    [ret[:params][:mode],
     ret[:params][:blocksize],
     ret[:params][:device],
     (ret[:params][:use_blktrace] ? "blktrace" : "no-blktrace")]
  end.sort_by do |group_param, group|
    group_param.join("-")
  end.each do |group_param, group|
    group = group.sort_by{|ret| hton(ret[:params][:multiplicity])}
    group.each do |ret|
      datafile.puts([ret[:params][:multiplicity],
                     ret[:transfer_rate],
                     ret[:iops],
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['rkB/s'] : ret[:iostat_avg]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['rkB/s'] : ret[:iostat_sterr]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['r/s'] : ret[:iostat_avg]['w/s']), # iops
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['r/s'] : ret[:iostat_sterr]['w/s']),
                     ret[:iostat_avg]['await'], ret[:iostat_sterr]['await'],
                     ret[:iostat_avg]['avgqu-sz'], ret[:iostat_sterr]['avgqu-sz'],
                     ret[:response_time],
                    ].join("\t"))
    end
    datafile.puts("\n\n")
    datafile.fsync

    title = group_param.join("-")
    style_spec = "lt #{style_idx} lc #{style_idx}"
    transfer_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:4:5",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorlines #{style_spec}"
    }
    style_idx += 1
    style_spec = "lt #{style_idx} lc #{style_idx}"
    iops_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:6:7",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorlines #{style_spec}"
    }
    response_time_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:($12*1000)",
      :index => "#{data_idx}:#{data_idx}",
      :with => "linespoints #{style_spec}"
    }
    avgqu_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:10:11",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorlines #{style_spec}"
    }

    plot_data_transfer.push(transfer_entry)
    plot_data_iops.push(iops_entry)
    plot_data_response_time.push(response_time_entry)
    plot_data_avgqu.push(avgqu_entry)

    style_idx += 1

    data_idx += 1

    multi_min = group.map{|ret| ret[:params][:multiplicity]}.min
    multi_max = group.map{|ret| ret[:params][:multiplicity]}.max
    plot_scatter(:output => common_file_name("#{prefix}#{title}.eps"),
                 :gpfile => common_file_name("#{prefix}#{title}.gp"),
                 :xlabel => "multiplicity",
                 :ylabel => "transfer rate [MiB/sec]",
                 :xrange => "[#{[multi_min-1, 0.5].max}:#{multi_max+1}]",
                 :yrange => "[0:]",
                 :title => "Transfer rate on #{group.first[:params][:device_or_file]}",
                 :plot_data => [transfer_entry.merge({:title => "transfer rate"}),
                                iops_entry.merge({:other_options => "axis x1y2", :title => "iops"})],
                 :other_options => "set key left top\nset y2label 'iops [1/sec]'\nset y2tics nomirror\nset y2range [0:]\nset logscale x\n")
    plot_scatter(:output => common_file_name("linear-plot/#{prefix}#{title}.eps"),
                 :gpfile => common_file_name("linear-plot/#{prefix}#{title}.gp"),
                 :xlabel => "multiplicity",
                 :ylabel => "transfer rate [MiB/sec]",
                 :xrange => "[#{multi_min-1}:65]",
                 :yrange => "[0:]",
                 :title => "Transfer rate on #{group.first[:params][:device_or_file]}",
                 :plot_data => [transfer_entry.merge({:title => "transfer rate"}),
                                iops_entry.merge({:other_options => "axis x1y2", :title => "iops"})],
                 :other_options => "set key left top\nset y2label 'iops [1/sec]'\nset y2tics nomirror\nset y2range [0:]\n")
  end
  datafile.close

  multi_min = results.map{|ret| ret[:params][:multiplicity]}.min
  multi_max = results.map{|ret| ret[:params][:multiplicity]}.max

  plot_scatter(:output => common_file_name("#{prefix}transfer-rate.eps"),
               :gpfile => common_file_name("#{prefix}transfer-rate.gp"),
               :xlabel => "multiplicity",
               :ylabel => "transfer rate [MiB/sec]",
               :xrange => "[#{[multi_min-1, 0.5].max}:#{multi_max+1}]",
               :yrange => "[0:]",
               :title => "Transfer rate",
               :plot_data => plot_data_transfer,
               :other_options => "set key center right\nset logscale x\n")
  plot_scatter(:output => common_file_name("#{prefix}iops.eps"),
               :gpfile => common_file_name("#{prefix}iops.gp"),
               :xlabel => "multiplicity",
               :ylabel => "iops [1/sec]",
               :xrange => "[#{[multi_min-1, 0.5].max}:#{multi_max+1}]",
               :yrange => "[0:]",
               :title => "IOPS",
               :plot_data => plot_data_iops,
               :other_options => "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("#{prefix}response-time.eps"),
               :gpfile => common_file_name("#{prefix}response-time.gp"),
               :xlabel => "offset [byte]",
               :ylabel => "response time [msec]",
               :xrange => "[#{[multi_min-1, 0.5].max}:#{multi_max+1}]",
               :yrange => "[0:]",
               :title => "Response time",
               :plot_data => plot_data_response_time,
               :other_options => "set key top left\nset logscale x\n")
  plot_scatter(:output => common_file_name("#{prefix}rq-queue-length.eps"),
               :gpfile => common_file_name("#{prefix}rq-queue-length.gp"),
               :xlabel => "offset [byte]",
               :ylabel => "avg. # of request queued",
               :xrange => "[#{[multi_min-1, 0.5].max}:#{multi_max+1}]",
               :yrange => "[0:]",
               :title => "queue length",
               :plot_data => plot_data_avgqu,
               :other_options => "set key top left\nset logscale x\n")

  # linear
  plot_scatter(:output => common_file_name("linear-plot/#{prefix}transfer-rate.eps"),
               :gpfile => common_file_name("linear-plot/#{prefix}transfer-rate.gp"),
               :xlabel => "multiplicity",
               :ylabel => "transfer rate [MiB/sec]",
               :xrange => "[#{[multi_min-1, 0.5].max}:65]",
               :yrange => "[0:]",
               :title => "Transfer rate",
               :plot_data => plot_data_transfer,
               :other_options => "set key top left\n")
  plot_scatter(:output => common_file_name("linear-plot/#{prefix}iops.eps"),
               :gpfile => common_file_name("linear-plot/#{prefix}iops.gp"),
               :xlabel => "multiplicity",
               :ylabel => "iops [1/sec]",
               :xrange => "[#{[multi_min-1, 0.5].max}:65]",
               :yrange => "[0:]",
               :title => "IOPS",
               :plot_data => plot_data_iops,
               :other_options => "set key top left\n")
  plot_scatter(:output => common_file_name("linear-plot/#{prefix}response-time.eps"),
               :gpfile => common_file_name("linear-plot/#{prefix}response-time.gp"),
               :xlabel => "offset [byte]",
               :ylabel => "response time [msec]",
               :xrange => "[#{[multi_min-1, 0.5].max}:#{multi_max+1}]",
               :yrange => "[0:]",
               :title => "Response time",
               :plot_data => plot_data_response_time,
               :other_options => "set key top left\n")
end

def iostress_plot_iostat(results)
  results.each do |result|
    title = $vars.map{|v| v.to_s+":"+result[:params][v].to_s}.join(",")

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
                 :title => "IO performance: #{title}",
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

def iostress_plot_blktrace(results)
  results = results.select do |ret|
    File.exists?(result_file_name(ret[:id], "blktrace"))
  end

  return if results.empty?

  rt_blktrace_series = []
  rt_blktrace_item_labels = []
  max_cpuid = 0

  results.each_with_index do |ret,ret_idx|
    blktrace = Blktrace.new(result_file_name(ret[:id], "blktrace/#{ret[:params][:device]}"))

    max_cpuid = blktrace.map{|rec| rec[0]}.max
    ioloc_datafiles = (0..(max_cpuid)).map do |cpuid|
      File.open(result_file_name(ret[:id], "blktrace/ioloc#{cpuid}.tsv"), "w")
    end
    blktrace.each do |record|
      cpu,seqno,time,action,rwbs,pos_sec,sz_sec,rt = *record
      ioloc_datafiles[cpu].puts([time, pos_sec].map(&:to_s).join("\t"))
    end
    plot_data_ioloc = []
    ioloc_datafiles.each_with_index do |datafile, idx|
      plot_data_ioloc.push({
                             :title => "cpu#{idx}",
                             :index => "0:0",
                             :datafile => datafile.path,
                             :using => "1:2",
                             :with => "points",
                           })
      datafile.close
    end

    plot_scatter(:output => result_file_name(ret[:id], "blktrace-ioloc.eps"),
                 :gpfile => result_file_name(ret[:id], "blktrace-ioloc.gp"),
                 :xlabel => "elapsed time [sec]",
                 :ylabel => "location of IO issued [sector]",
                 :xrange => "[0:0.01]",
                 :yrange => "[0:]",
                 :title => "IO requiests",
                 :plot_data => plot_data_ioloc,
                 :other_options => "set key outside\nunset rmargin\n"
                 )

    rt = blktrace.map{|record| record[7]}
    rt_blktrace_series.push({
                              :value => rt.avg * 10**6,
                              :stdev => rt.sterr * 10**6
                            })
    rt_blktrace_item_labels.push(ret[:id])
  end

  plot_bar(:output => cname("response-time-blktrace.eps"),
           :gpfile => cname("response-time-blktrace.gp"),
           :datafile => cname("response-time-blktrace.tsv"),
           :series_labels => ["response time"],
           :item_labels => rt_blktrace_item_labels,
           :title => "Response time measured by blktrace",
           :yrange => "[0:]",
           :ylabel => "response time [usec]",
           :size => "1.0,1.0",
           :data => [rt_blktrace_series],
           :other_options => "#unset key\n")
  plot_bar(:output => cname("response-time-blktrace-small.eps"),
           :gpfile => cname("response-time-blktrace-small.gp"),
           :datafile => cname("response-time-blktrace-small.tsv"),
           :series_labels => ["response time"],
           :item_labels => rt_blktrace_item_labels,
           :title => "Response time measured by blktrace",
           :yrange => "[0:100]",
           :ylabel => "response time [usec]",
           :size => "1.0,1.0",
           :data => [rt_blktrace_series],
           :other_options => "#unset key\n")
end
