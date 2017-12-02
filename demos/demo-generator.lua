local music_utils = require 'music-utils.lua'

local gen = (function()
    local loop_num = 0
    local self = {}
    local free_regs = {
        x = true, y = true, z = true,
        a = true, b = true, c = true
        -- sp and pc are never 'free'
    }
    local gen_started = false
    local gen_done = false
    local function call_twice(f)
        return f(), f()
    end

    local function gen_member_fn(func)
        return function(...)
            if not gen_started then
                gen_started = true
                print('set sp stack-start')
            end
            func(...)
        end
    end
    local function choose_free_register()
        for k, v in pairs(free_regs) do
            if v then
                free_regs[k] = false
                return k
            end
        end
        return nil
    end

    local function free_up_register(k)
        free_regs[k] = true
    end
    self.loop = gen_member_fn(function(b, e, step, func)
        if type(step) == 'function' then
            func = step
            step = 1
        end
        if free_regs[b] ~= nil or free_regs[e] ~= nil then
            error('register ranges not supported yet')
        end
        local loop_reg, counter = call_twice(choose_free_register)
        print('# generated loop (number '..loop_num..')'..
              '\tset '..counter..' '..b..'\n'..
              ':l'..loop_num..'-start\n'..
              '\tcomp '..counter..' '..e)
        loop_num = loop_num + 1
        return self
    end)
    self.finish = gen_member_fn(function()
        -- also want to finish up static data
        print('set pc -9\n'.. -- halts the program
              ':stack-start data numbers [0]\n')
        -- only member function that does not return self (as no further work
        -- can really be done)
    end)
    return self
end)();
gen.data('sprites'        , { x = 'int', y = 'int' }, 10).
    data('text-scroll-pos', { x = 'int' }).
    data('music-pos'      , { time = 'fp' }).
    from('music-pos', 0, function(gen, frame)
        frame.on('time').set(3.0) -- however long the theme is
    end).loop_until(function()
        -- establish a condition
        gen.read('controller', function(gen, frame)
            frame._and()
        end)
    end, function()
        -- usual loop
        gen.loop(0, 10, function(gen)
            gen.from('sprites', gen.loop_value(), function(gen, frame)
                frame.on('x').inc().on('y').inc(10).mod(320)
            end)
        end).
        from('text-scroll-pos', 0, function(gen, frame)
            frame.on('x').inc(10).mod(480)
        end).
        frame_refresh()
    end).
finish()
