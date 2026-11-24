-- tests/run_heap_tests.sh: allocate until the heap is full: the runtime reaches __oom_handler
local t
function game_loop()
    while true do
        t = { 1, 2, 3, 4, 5, 6, 7, 8 }
    end
end
