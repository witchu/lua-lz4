local function readfile(filename)
  local f = assert(io.open(filename))
  local s = f:read("*a")
  f:close()
  return s
end

return readfile
