-- tests/run_heap_tests.sh: tables made in a loop (the heap only grows)
local t
function game_loop()
    for i = 1, 5 do
        t = { i, i + 1, i + 2 }
    end
end
