#!/usr/bin/lua
local command = '/usr/bin/time 2>&1 -f "%U" ./erfindung-cli > /dev/null demos/pref-test.efas'
--command = 'do-stuff'
local max_concurrency = 4
local max_tests       = 12
local function do_cleanup()
    os.execute('make clean ; rm erfindung-cli')
end
local function do_benchmark(handle_result)
    local function do_clean_and_build()
        do_cleanup();
        if not os.execute('make -j '..max_concurrency) then
            assert(false)
        end
    end
    do_clean_and_build()
    for i = 1, math.floor(max_tests / max_concurrency) do
        local product_command = ''
        for j = 1, max_concurrency do
            product_command = product_command..command..' & '
        end
        product_command = string.sub(product_command, 1, -4)
        local f = io.popen(product_command)
        handle_result(f)
    end
    local product_command = ''
    for i = 1, max_tests % max_concurrency do
        product_command = product_command..command..' & '
    end
    product_command = string.sub(product_command, 1, -4)
    local f = io.popen(product_command)
    handle_result(f)
end

local function do_profile()
    do_cleanup()
    if not os.execute('make profile -j '..max_concurrency) then
        assert(false)
    end
    os.execute(command)
    os.execute('gprof erfindung-cli > profout.txt')
    do_cleanup()
end

local function get_stats_on(stats, name)
    assert(#stats ~= 0)
    name = name or 'value'
    local avg = 0
    for _, v in ipairs(stats) do
        avg = avg + v
    end
    avg = avg / #stats
    local accum = 0
    for _, v in ipairs(stats) do
        accum = accum + (avg - v)*(avg - v)
    end
    local stddev = math.sqrt(accum / #stats)
    print('The average '..name..' was '..avg..' with a '..
          'standard deviation of '..stddev)
end

local function do_benchmark_runs()
	local inst_per_sec = {}
	do_benchmark(function(benchmark_res)
	    for line in benchmark_res:lines() do
	        local et = tonumber(line)
	        inst_per_sec[#inst_per_sec + 1] = ((10000000) / et)*21
	    end
	end)
	get_stats_on(inst_per_sec, 'instructions per second')
end

do_benchmark_runs()
--do_profile()
