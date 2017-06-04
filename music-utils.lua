local function mk_audio_device(chan_func)
    local apu_address   = -4
    local apu_note      = 0
    local apu_tempo     = 1
    local frames        = 0
    local current_tempo = 0
        
    local function send_x() 
        print("save x "..apu_address)
    end
    local function spec_channel()
        print("set  x "..chan_func())
        send_x()
    end
    local self = {
        set_tempo = function(x)
            current_tempo = x
            spec_channel()
            print("set  x "..apu_tempo)
            send_x()
            print("set  x "..x)
            send_x()
        end,
        send_note = function(x)
            if current_tempo == 0 then
                error("Got to set that tempo first!")
            end
            frames = frames + (60.0 / current_tempo)
            spec_channel()
            print("set  x "..apu_note)
            send_x()
            print("set  x "..x)
            send_x()
        end 
    }
    self.send_notes = function(arr)
        for i = 1, #arr do
            self.send_note(arr[i])
        end
    end
    self.length_in_frames = function()
        return math.floor(frames)
    end
    
    return self
end

local triangle  = mk_audio_device(function() return 0 end)
local pulse_one = mk_audio_device(function() return 1 end)
local pulse_two = mk_audio_device(function() return 2 end)

local function setup_wait(audio_devs)
    local cur_max = 0
    local function max(a, b)
        if a > b then return a else return b end
    end
    for k, dev in pairs(audio_devs) do
        cur_max = max(dev.length_in_frames(), cur_max)
    end
    print("set x "..cur_max)
    print("call sleep")
end

local function gen_sleep_function()
    print([[
:sleep
    push x
    comp a x    0
    skip a   !=
        jump sleep-end
    sub x 1
    io wait a
    call sleep
:sleep-end pop x pc
    ]])
end

return {
    triangle           = triangle          ,
    pulse_one          = pulse_one         ,
    pulse_two          = pulse_two         ,
    finish_program     = function()
        setup_wait({ triangle, pulse_one, pulse_two })
        print("save pc -9")

        gen_sleep_function()
        print(":stack-start data numbers [0]")
    end
}
