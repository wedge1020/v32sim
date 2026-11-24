-- tests/run_heap_tests.sh: a table grown one element at a time. Every time it outgrows its array part
-- (8, 16, 32 words) the old block goes to the allocator's free lists: `HEAP ... free=N blocks=N` (v32lua 20261124-dev and later)
local t = {}
local n = 0
function game_loop()
    if n < 40 then
        n = n + 1
        t[n] = n
    end
end
