
require 'tempfile'

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

def iohub_latency_analyze(plan)
  use_script :plot
  use_script :io_common

  results = load_results()

  iohub_latency_plot_all(results)
#   iohub_latency_plot_iotrace(results)
  plot_iostat(results)
end

def iohub_latency_plot_all(results, prefix = "")
  datafile = File.open(common_file_name("devices.tsv"), "w")

  plot_data_iops = []
  plot_data_response_time = []
  data_idx = 0

  serieses = results.group_by do |ret|
    [ret[:params][:mode],
     ret[:params][:blocksize],
     ret[:params][:multiplicity]]
  end.map do  |series_param, series|
    [series_param.join("-"), series.sort_by{|ret| ret[:params][:device]}]
  end.sort_by do |series_param, series|
    series_param
  end

  num_serieses = serieses.size
  if ! serieses.all?{|_,se| se.size == serieses.first[1].size}
    raise StandardError.new("All serieses must have the same # of items.")
  end
  num_items = serieses.first[1].size

  item_labels = serieses.first[1].map{|ret| ret[:params][:device]}
  series_labels = serieses.map{|series_param, series| series_param }

  iops_serieses_data = serieses.map do |series_param, series|
    series.map do |ret|
      {
        :value => (ret[:params][:mode] == :read ? ret[:iostat_avg]['r/s'] : ret[:iostat_avg]['w/s']),
        :stdev => (ret[:params][:mode] == :read ? ret[:iostat_sterr]['r/s'] : ret[:iostat_sterr]['w/s'])
      }
    end
  end

  rt_serieses_data = serieses.map do |series_param, series|
    series.map do |ret|
      {
        :value => ret[:response_time] * 10**6
      }
    end
  end
  rt_serieses_data += serieses.map do |series_param, series|
    series.map do |ret|
      {
        :value => ret[:iostat_avg]['await'] * 1000,
        :stdev => ret[:iostat_sterr]['await'] * 1000,
      }
    end
  end
  rt_series_labels = series_labels + ["iostat await"]

  plot_bar(:output => common_file_name("#{prefix}iops.eps"),
           :gpfile => common_file_name("#{prefix}iops.gp"),
           :datafile => common_file_name("#{prefix}iops.tsv"),
           :series_labels => series_labels,
           :item_labels => item_labels,
           :ylabel => "IOPS",
           :yrange => "[0:]",
           :title => "IOPS",
           :data => iops_serieses_data)

  plot_bar(:output => common_file_name("#{prefix}response-time.eps"),
           :gpfile => common_file_name("#{prefix}response-time.gp"),
           :datafile => common_file_name("#{prefix}response-time.tsv"),
           :series_labels => rt_series_labels,
           :item_labels => item_labels,
           :ylabel => "response time [usec]",
           :yrange => "[0:]",
           :title => "response time",
           :data => rt_serieses_data)

  return


  serieses.each do |group_param, group|
    group = group.sort_by{|ret| hton(ret[:params][:device])}
    group.each do |ret|
      datafile.puts([ret[:params][:device],
                     ret[:transfer_rate],
                     ret[:iops],
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['rkB/s'] : ret[:iostat_avg]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['rkB/s'] : ret[:iostat_sterr]['wkB/s']) / 1024, # transfer rate
                     (ret[:params][:mode] == :read ? ret[:iostat_avg]['r/s'] : ret[:iostat_avg]['w/s']), # iops
                     (ret[:params][:mode] == :read ? ret[:iostat_sterr]['r/s'] : ret[:iostat_sterr]['w/s']), # iops
                     ret[:iostat_avg]['await'], ret[:iostat_sterr]['await'],
                     ret[:iostat_avg]['avgqu-sz'], ret[:iostat_sterr]['avgqu-sz'],
                     ret[:response_time],
                    ].join("\t"))
    end
    datafile.puts("\n\n")
    datafile.fsync

    title = group_param.join("-")
    iops_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:6:7",
      :index => "#{data_idx}:#{data_idx}",
      :with => "yerrorbars"
    }
    response_time_entry = {
      :title => title,
      :datafile => datafile.path,
      :using => "1:($12*1000)",
      :index => "#{data_idx}:#{data_idx}",
      :with => "points"
    }

    plot_data_iops.push(iops_entry)
    plot_data_response_time.push(response_time_entry)

    style_idx += 1
    data_idx += 1

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

def iohub_latency_plot_iotrace(results)
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
