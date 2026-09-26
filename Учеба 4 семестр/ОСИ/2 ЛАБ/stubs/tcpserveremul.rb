require 'socket.rb'

class InvalidData < Exception
end



class DspBdate
  def load(data)
    if data.size >= 4
      @day = data.slice!(0, 1).unpack("C")[0]
      @month = data.slice!(0, 1).unpack("C")[0]
      @year = data.slice!(0, 2).unpack("n")[0]    
    else
      raise InvalidData.new
    end
  end
  
  def text
    "%02d.%02d.%04d" % [@day, @month, @year]
  end

  def proto(str)
    r = Regexp.new("^(\\d{2})\\.(\\d{2})\\.(\\d{4})")
    m = r.match(str)
    raise InvalidData.new unless m
    str.slice!(0, m[0].size)
    buf = [m[1].to_i, m[2].to_i].pack("C*")
    buf << [m[3].to_i].pack("n")
    buf
  end
end


class DspBtime
  def load(data)
    if data.size >= 3
      @val = data.slice!(0, 3).unpack("C*")
    else
      raise InvalidData.new
    end
  end
  
  def text
    @val.collect {|v| "%02d" % v}.join(":")
  end

  def proto(str)
    r = Regexp.new("^(\\d{2})\\:(\\d{2})\\:(\\d{2})")
    m = r.match(str)
    raise InvalidData.new unless m
    str.slice!(0, m[0].size)
    [m[1].to_i, m[2].to_i, m[3].to_i].pack("C*")
  end
end



class DspMsgsz
  def load(data)
    @val = ""
    while data.size > 0
      substr = data.slice!(0,1)
      return if substr.unpack("C*") == [0]
      @val << substr
    end
    raise InvalidData.new
  end
  
  def text
    @val
  end

  def proto(str)
    buf = str.dup
    str.replace("")
    buf << [0].pack("C")
    buf
  end
end

$dsp = [DspBdate, DspBdate, DspBtime, DspMsgsz]


def msglog(peer, items)
  msg = "#{peer} #{items.collect {|item| item.text}.join(" ")}"
  File.open("msg.txt", "a"){|f| f.puts(msg) }
end

def shift_msg(buf, peer)
  data = buf.dup
  begin
    raise InvalidData.new if data.size < 4
    data.slice!(0,4)
    items = $dsp.collect do |d| 
      item = d.new
      item.load(data)
      item
    end
    buf.replace(data)
    msglog(peer, items)
    return items.last.text
  rescue InvalidData => e
    return nil
  end
  nil
end


def eat_space(str)
  raise "Invalid string: #{str}" if str.size == 0 || str[0..0] != " "
  str.slice!(0, 1)
end


def send_msg(cn, idx, str)
  sendbuf = "" 
  sendbuf << [idx].pack("N")

  $dsp.each do |d|
    sendbuf << d.new.proto(str)
    eat_space(str) if d != $dsp.last
  end
  
  cn.write(sendbuf)
end


def send_msgs(cn)
  File.open("msg.txt", "r") do |f|
    r = Regexp.new("^[\\d\\.\\:]+\\s(.*?)$")
    idx = 0
    while (str = f.gets) != nil
      str = str.chomp
      m = r.match(str)
      if m && !str.empty?
        send_msg(cn, idx, "#{m[1]}")
        idx = idx + 1
      end
    end
    puts "#{idx} messages sent."
  end
end


def dispatch(cn, peer)
  buf = ""
  mode = nil
  loop do
    data = cn.recv(512)
    break if data.size == 0
    buf << data

    unless mode
      if buf.size >= 3
        mode = buf.slice!(0,3)
        unless ["put", "get"].include?(mode)
          raise "Invalid client cmd: #{mode}"
        end
      end
    end

    if mode == "put"
      while text = shift_msg(buf, peer)
        cn.write("ok")

        if text == "stop"
          puts "'stop' message arrived. Terminating..."
          Kernel.exit 
        end
      end
    elsif mode == "get"
      send_msgs(cn)
      return
    end    
  end
end

def server(port)
  server = TCPServer.new("0.0.0.0", port)
  puts "Listening TCP port: #{port}"
  loop do
    Thread.fork(server.accept) do |cn|
      peer = "#{cn.peeraddr[3]}:#{cn.peeraddr[1]}"
      puts "  Peer connected  : #{peer}"
      begin
        dispatch(cn, peer)
      rescue Exception => e
        raise e if e.is_a?(SystemExit)
        puts "  Peer Exception   : #{peer}: '#{e.message}'"
        puts "#{e.backtrace.shift(3).join("\n")}"
      ensure
        puts "  Peer disconnected: #{peer}"
        cn.close
      end
    end
  end
end

server(ARGV[0] || 9000)
