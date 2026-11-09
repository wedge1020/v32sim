local total = 0
local function add(a, b)
  local sum = a + b
  sum = sum + 1
  total = total + sum
  return sum
end
function main()
  local r = add(3, 4)
  local dead = 0
  for i = 1, 4 do dead = dead + i; r = r + i end
  total = r
end
