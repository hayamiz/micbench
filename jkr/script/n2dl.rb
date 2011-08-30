# 1 origin
def num_to_drive_letter(num)
  alpha = ("a".."z").to_a
  base = alpha.size
  ret = ""
  if num == 0
    return "a"
  end
  while num > 0
    if ret.size > 0
      num -= 1
    end
    rem = num % base
    ret = alpha[rem] + ret
    num = (num - rem) / base
  end
  ret
end

def n2dl(num)
  num_to_drive_letter(num)
end
