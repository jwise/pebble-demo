for i=0,256 do
print (math.floor(math.atan(i/256) / math.pi * 0x8000)..", ")
end
