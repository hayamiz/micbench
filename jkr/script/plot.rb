

def gnuplot_label_escape(str)
  str.gsub(/_/, "\\_").gsub(/\{/, "\\{").gsub(/\}/, "\\}")
end

def plot_distribution(config)
  dataset = config[:dataset]
  xrange_max = config[:xrange_max]
  xrange_min = config[:xrange_min] || 0
  title = gnuplot_label_escape(config[:title])
  xlabel = config[:xlabel]
  eps_file = config[:output]
  datafile = if config[:datafile]
               File.open(config[:datafile], "w")
             else
               Tempfile.new("dist")
             end

  stepnum = 500

  plot_stmt = []
  data_idx = 0
  xrange_max_local = 0
  histset = []
  dataset.each do |tx_type,data|
    hist = Array.new
    avg = data.inject(&:+) / data.size.to_f
    n90th_idx = (data.size * 0.9).to_i
    n90percent = data.sort[n90th_idx]
    xrange_max_local = [xrange_max_local, xrange_max || n90percent * 4].max
    step = xrange_max_local / stepnum
    maxidx = 0
    data.each do |num|
      idx = (num / step).to_i
      maxidx = [idx, maxidx].max
      hist[idx] = (hist[idx] || 0) + 1
    end

    0.upto(maxidx) do |idx|
      unless hist[idx]
        hist[idx] = 0
      end
    end

    histset.push({:tx_type => tx_type, :step => step, :hist => hist, :max => hist.max})

    histmax = hist.max.to_f
    datafile.puts "##{tx_type}"
    0.upto(stepnum - 1) do |i|
      x = (i + 0.5) * step
      y = hist[i] || 0
      if config[:normalize]
        y = y / histmax
      end
      datafile.puts "#{x}\t#{y}"
    end
    datafile.puts
    datafile.puts

# index data_idx+1
    datafile.puts "#{avg}\t0"
    datafile.puts "#{avg}\t#{hist.max}"
    datafile.puts
    datafile.puts

# index data_idx+1
    datafile.puts "#{n90percent}\t0"
    datafile.puts "#{n90percent}\t#{hist.max}"
    datafile.puts
    datafile.puts

    plot_stmt.push(sprintf("'%s' index %d:%d using 1:2 with linespoints title '%s'",
                           datafile.path, data_idx, data_idx, tx_type))
    if dataset.size == 1
      plot_stmt.push(sprintf("'%s' index %d:%d using 1:2 with linespoints title '%s(avg)'",
                             datafile.path, data_idx+1, data_idx+1, tx_type))
      plot_stmt.push(sprintf("'%s' index %d:%d using 1:2 with linespoints title '%s(90%%-th)'",
                             datafile.path, data_idx+2, data_idx+2, tx_type))
    end
    data_idx += 3
  end

  datafile.fsync
#   if config[:datafile]
#     File.open(config[:datafile], "w"){|f|
#       f.puts(histset.map{|histdata|
#                [histdata[:tx_type], "", ""].join("\t")
#              }.join("\t"))
#       f.puts(histset.map{|histdata|
#                ["response time[sec]", "hist", "normalized hist"].join("\t")
#              }.join("\t"))
#       stepnum.times{|i|
#         f.puts(histset.map{|histdata|
#                  x = (i + 0.5) * histdata[:step]
#                  y = histdata[:hist][i].to_f
#                  max = histdata[:max]
#                  sprintf("%f\t%f\t%f", x, y, y / max)
#                }.join("\t"))
#       }
#     }
#   end
  plot_stmt = plot_stmt.flatten.join(", ")
  plot_stmt = "plot #{plot_stmt}"

  script = <<EOS
set term postscript enhanced color
set output "#{eps_file}"
set size 0.9,0.6
set title "#{title}"
set ylabel "Frequency"
set xlabel "#{xlabel}"
set xrange [#{xrange_min}:#{xrange_max_local}]
#{config[:other_options]}
#{plot_stmt}
EOS
  script_file = Tempfile.new("gp")
  script_file.puts script
  script_file.fsync
  system_("gnuplot #{script_file.path}")
  if config[:gpfile]
    FileUtils.copy(script_file.path, config[:gpfile])
  end
end

def plot_timeseries(config)
  [:plot_data, :output, :title, :xlabel, :ylabel].each do |key|
    unless config[key]
      $stderr.puts "key '#{key.to_s}' is required for time-series graph"
      return
    end
  end
  
  xrange = if config[:xrange]
             "set xrange #{config[:xrange]}"
           else
             ""
           end
  yrange = if config[:yrange]
             "set yrange #{config[:yrange]}"
           else
             ""
           end

  plot_stmt = "plot " + config[:plot_data].map {|plot_datum|
    [:datafile, :using, :with, :title].each do |key|
      unless plot_datum[key]
        $stderr.puts "key '#{key.to_s}' is required for a plot_datum of time-series graph"
        return
      end
    end
    index = ""
    if plot_datum[:index]
      index = " index #{plot_datum[:index]} "
    end
    "'#{plot_datum[:datafile]}' #{index} using #{plot_datum[:using]} with #{plot_datum[:with]}"+
    " title '#{gnuplot_label_escape(plot_datum[:title])}' " + plot_datum[:other_options].to_s
  }.join(", ")

  script = <<EOS
set term postscript enhanced color
set output "#{config[:output]}"
set size 0.9,0.6
set title "#{config[:title]}"
set ylabel "#{config[:ylabel]}"
set xlabel "#{config[:xlabel]}"
set rmargin 3
set lmargin 10
#{xrange}
#{yrange}
set grid
#{config[:other_options]}
#{plot_stmt}
EOS
  gp = Tempfile.new("plot_timeseries")
  gp.puts script
  gp.fsync
  if config[:gpfile]
    FileUtils.copy(gp.path, config[:gpfile])
  end
  puts script
  system_("gnuplot #{gp.path}")
end

def plot_bar(config)
  [:output, :title, :series_labels, :item_labels, :data].each do |key|
    unless config[key]
      raise StandardError.new("key '#{key.to_s}' is required for bar graph")
    end
  end

  config[:size] ||= "0.9,0.6"

  data = config[:data]

  if ! data.all?{|series| series.all?{|datum| datum[:value].is_a? Numeric}}
    raise StandardError.new("All datum must have :value key")
  end

  if config[:datafile]
    datafile = File.open(config[:datafile], "w")
  else
    datafile = Tempfile.new("plot_bar")
  end

  plot_data = []
  plot_stdev_data = []

  if data.all?{|series| series.all?{|datum| datum[:stdev].is_a? Numeric}}
    draw_stdev = true
  else
    draw_stdev = false
  end

  item_barwidth = 0.9 # 2.0 / 3.0
  num_serieses = data.size
  data_idx = 0
  config[:data].each_with_index do |series, series_idx|
    series.each_with_index do |datum, item_idx|
      xpos = item_idx - item_barwidth / 2.0 + item_barwidth / num_serieses.to_f / 2.0 + item_barwidth / num_serieses.to_f * series_idx
      barwidth = item_barwidth / num_serieses.to_f
      datafile.puts([xpos, datum[:value], barwidth, (datum[:stdev] || "")].join("\t"))
    end

    plot_data.push({
                     :title => config[:series_labels][series_idx],
                     :using => "1:2:3",
                     :index => "#{data_idx}:#{data_idx}",
                   })

    datafile.puts("\n\n")
    data_idx += 1
  end
  datafile.fsync

  ylabel = if config[:ylabel]
             "set ylabel '#{config[:ylabel]}'"
           else
             ""
           end

  yrange = if config[:yrange]
             "set yrange #{config[:yrange]}"
           else
             ""
           end

  plot_stmt = "plot " + plot_data.map do |plot_datum|
    [:using, :title].each do |key|
      unless plot_datum[key]
        raise StandardError.new("key '#{key.to_s}' is required for a plot_datum of bar graph")
        return
      end
    end
    index = ""
    if plot_datum[:index]
      index = " index #{plot_datum[:index]} "
    end
    "'#{datafile.path}' #{index} using #{plot_datum[:using]}"+
    " title '#{gnuplot_label_escape(plot_datum[:title])}' with boxes fs solid 0.7 "
  end.join(", ")

  xpos = -1
  xtics = config[:item_labels].map do |label|
    xpos += 1
    "\"#{label}\" #{xpos}"
  end.join(",")
  xtics = "(#{xtics})"
  xrange = "[-0.5:#{config[:item_labels].size - 0.5}]"

  script = <<EOS
set term postscript enhanced color
set output "#{config[:output]}"
set size #{config[:size]}
set title "#{config[:title]}"
#{ylabel}
#{yrange}
set xrange #{xrange}
set xtic rotate by -30 scale 0
set xtics #{xtics}
#{config[:other_options]}
#{plot_stmt}
EOS
  if config[:gpfile]
    gp = File.open(config[:gpfile], "w")
  else
    gp = Tempfile.new("gnuplot")
  end
  gp.puts script
  gp.fsync
  system_("gnuplot #{gp.path}")
  gp.close
end

def plot_scatter(config)
  [:plot_data, :output, :title, :xlabel, :ylabel].each do |key|
    unless config.keys.include?(key)
      $stderr.puts "key '#{key.to_s}' is required for time-series graph"
      return
    end
  end
  
  xrange = if config[:xrange]
             "set xrange #{config[:xrange]}"
           else
             ""
           end
  yrange = if config[:yrange]
             "set yrange #{config[:yrange]}"
           else
             ""
           end

  plot_stmt = "plot " + config[:plot_data].map {|plot_datum|
    unless plot_datum[:expression] ||
        [:datafile, :using, :with].every?{|key| plot_datum[key]}
      raise ArgumentError.new("key ('datafile', 'using', 'with') or 'expression' is required for a plot_datum")
    end

    plot_target = nil
    if plot_datum[:expression]
      plot_target = plot_datum[:expression]
    else
      index = ""
      if plot_datum[:index]
        index = " index #{plot_datum[:index]} "
      end

      plot_target = "'#{plot_datum[:datafile]}' #{index} using #{plot_datum[:using]}"
    end

    unless plot_datum[:title]
      title = "notitle"
    else
      title = "title '#{gnuplot_label_escape(plot_datum[:title])}'"
    end
    "#{plot_target} with #{plot_datum[:with]} #{title} " + plot_datum[:other_options].to_s
  }.join(", ")

  script = <<EOS
set term postscript enhanced color
set output "#{config[:output]}"
set size 0.9,0.7
set title "#{config[:title]}"
set ylabel "#{config[:ylabel]}"
set xlabel "#{config[:xlabel]}"
set rmargin 10
set lmargin 10
#{xrange}
#{yrange}
set grid
#{config[:other_options]}
#{plot_stmt}
EOS
  puts script
  gp = Tempfile.new("gnuplot")
  gp.puts script
  gp.fsync
  if config[:gpfile]
    FileUtils.copy(gp.path, config[:gpfile])
  end
  system_("gnuplot #{gp.path}")
end
