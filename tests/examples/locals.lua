-- locals.lua -- Lua locals for `locals` (V32DBG section 4): numbers, a table, a string, booleans, nil, a closure, and a
-- local captured by a closure (boxed). tests/run_harness_tests.sh builds it with v32lua -g + v32as -g.
local function counter(start)
  local n = start
  local label = "count"
  return function()
    n = n + 1
    return n
  end
end

local function work(a, b)
  local t = {1, 2, 3}
  local s = "hello"
  local ok = true
  local f = counter(10)
  local v = f()
  local nothing = nil
  return a + b + v
end

function main()
  local r = work(2, 3)
  ioports.gpu.clear("black")
end
