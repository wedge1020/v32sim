-- tests/run_heap_tests.sh: --gc arena; marks and releases at known frames so --heap shows depth 1, 2, 1, 0
--#gc arena
local n = 0
function game_loop()
    n = n + 1
    if n == 2 then m1 = arena.mark() end
    if n == 3 then t = { 1, 2, 3, 4 } ; m2 = arena.mark(); u = { 5, 6, 7 } end
    if n == 5 then arena.release(m2) end
    if n == 6 then arena.release(m1) end
end
