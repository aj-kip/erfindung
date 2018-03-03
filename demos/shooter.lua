#!/usr/bin/lua

print('#!./erfindung')
print('set sp stack-start')

local function make_head_section(title, func)
    return function()
    print('###################################################################')
    print('# START SECTION: '..title)
    print('###################################################################\n')
    if not (func == nil) then
        func()
    end
    print('\n# END SECTION: '..title)
    print('###################################################################\n')
    end -- return value
end

function player_bullet_features(ship_instance, n)
    n = n or 3
    local bullet_data_offset = 2
    local bullet_data_size   = 2 -- words
    local bullet_data_label  = 'bullet_data'
    
    --                          128 -> 64 -> 32 -> 16 -> 8
    --                          3   ->  3 ->  3 ->  2 -> 3
    local bullet_sprite_index = '0b1001111111011'

    local function gen_init()
        print([[jump bullet_data_end
        : ]]..bullet_data_label..[[ data numbers [
        0 # bullet index
        0 # fire delay - as fp
        ]])
        for i = 1, n do
            print('    0 # x bullet number '..i)
            print('    0 # y')
        end
        print([[ ] # end bullet array
        :bullet-sprite data binary [
        # 0123
          _XX_ # 0
          X__X # 1
          X__X # 2
          X__X # 3
          X__X # 4
          _XX_ # 5
          ____ # 6
          ____ # 7
        ]])
        print([[
        ]
        :bullet_data_end
        set a bullet-sprite
        set z ]]..bullet_sprite_index..'\n'..[[
        set x 4
        set y 8
        io upload x y a z

        ]])
    end
    
    local function gen_update_routine()
        print(':update-bullets push x y z')
        for i = 1, n do
            print([[#
            set z ]]..i..[[#
            mul z ]]..bullet_data_size  ..[[#
            add z ]]..bullet_data_label ..[[#
            add z ]]..bullet_data_offset..[[#
            load y z 1
            sub  y 10
            save y z 1
            load x z
            set  z ]]..bullet_sprite_index..[[#
            io draw x y z
            ]])
        end
        -- :TODO: update frame time
        print('pop z y z pc')
    end
    
    local function gen_check_fire()

        local skip_fire_bullet = 'skip-fire-bullet'

        print([[#
        # Note that this is a proper function, and therefore should not be
        # inlined
        : check-fire-bullet
        push x y z
        # load bullet et, increment, save
        # if over 0.2, continue function otherwise return
        set z ]]..bullet_data_label..[[#
        load x z 1
        assume fp
        io read timer y
        add x x y
        save x z 1
        comp x    0.2
        skip x >=
            jump ]]..skip_fire_bullet..[[#
        # reset counter
        set  x 0.0
        save x z 1
        # read controller
        assume int
        io read controller z
        and  z    16
        comp z    16
        skip z ==
            jump ]]..skip_fire_bullet..[[#
        # get current position of ship
        # will use x, y
        ]]..ship_instance.inline_load_pos('z')..[[
        
        # load bullet index adjust to position in (words?)
        load z ]]..bullet_data_label..[[#
        mul  z ]]..bullet_data_size..[[#
        # offset for initial data
        add  z ]]..bullet_data_offset..[[#
        # offset for memory address
        add  z ]]..bullet_data_label..[[#
        
        # save ship positions on top of bullets
        save x z 0
        save y z 1
        
        # increment bullet index
        load z ]]..bullet_data_label..[[#
        plus z 1
        mod  z ]]..n..[[#
        save z ]]..bullet_data_label..[[#
        :]]..skip_fire_bullet..[[#
        pop z y x pc]])
    end

    return {
        init   = make_head_section('bullet init', gen_init),
        update = make_head_section('bullet update', gen_update_routine),
        update_function_label = function() return 'update-bullets' end,
        check_for_firing = make_head_section('bullet check for firing', gen_check_fire),
        check_for_firing_label = function() return 'check-fire-bullet' end
    }
end

function ship_features()
    local ship_sprite_label = 'ship-sprite-data'
    local ship_data_label   = 'ship-data'
    local ship_draw_label   = 'draw-ship'
    local ship_sprite_index = '0b01111111111'
    local ship_update_func_label = 'ship_update'
    
    local function inline_load_pos(reg)
        return [[ 
        # just load ship positions from memory
        set ]]..reg..' '..ship_data_label..'\n'..[[
        load x ]]..reg..[[ 0
        load y ]]..reg..' 1 \n'
    end

    local function gen_ship_init()
        print([[
        set a ]]..ship_sprite_label..'\n'..[[
        set z ]]..ship_sprite_index..'\n'..[[
        set x 16
        set y 14
        io upload x y a z
        jump end-ship-data
        :]]..ship_data_label..[[ data numbers [
            160.0 # x
            200.0 # y
            3     # lives
            0     # upgrade-level
        ]
        :]]..ship_sprite_label..[[ data [
        # 0123456789ABCDEF
          _X__X______X__X_  # 0
          _XXXX______XXXX_  # 1
          _X__X______X__X_  # 2
          _X__X______X__X_  # 3
          _X__X_XXXX_X__X_  # 4
          __X__X____X__X__  # 5
          __X__________X__  # 6
          __X____XX____X__  # 7
          ___X________X___  # 8
          __X____XX____X__  # 9
          __X___X__X___X__  # A
          __XXXXX__XXXXX__  # B
          __X___X__X___X__  # C
          __X___X__X___X__  # D
          
          ] # end
        :end-ship-data
        ]])
    end
    
    local function gen_ship_update()
        print([[#
        :]]..ship_update_func_label..[[#
            push x y z
            ]]..inline_load_pos('z')..[[
            io read controller z
            and  z    3
            comp z    1
            skip z ==  # up is pressed
               jump skip-press-up
            add y -1
            jump skip-press-down
        : skip-press-up
            io read controller z
            and  z    3
            comp z    2
            skip z ==  # down is pressed
               set pc skip-press-down
            add y 1
        : skip-press-down
            io read controller z
            and  z   12
            comp z    4 # left is pressed
            skip z ==
               set pc skip-press-left
            add x -1
            jump do-ship-update
        : skip-press-left
            io read controller z
            and  z   12
            comp z    8 # right is pressed
            skip z ==
               set pc do-ship-update
            add x 1
        : do-ship-update
            push x y
            pop y x
            # save it back
            set  z ]]..ship_data_label..'\n'..[[
            save x z 0
            save y z 1
            # draw the ship
            set z ]]..ship_sprite_index..[[

            sub x 8
            sub y 8
            io draw x y z
            pop z y x pc
    ]])
    end
    
    return {
        init   = make_head_section('ship init', gen_ship_init       ),
        update = make_head_section('ship update', gen_ship_update),
        update_function_label = function() return ship_update_func_label end,
        inline_load_pos = inline_load_pos
    }
end

local ship    = ship_features()
local bullets = player_bullet_features(ship)

bullets.init()
ship.init()
print(':main-loop')
print('call '..bullets.update_function_label())
print('call '..ship.update_function_label())
print('call '..bullets.check_for_firing_label())

print([[
io wait  x
io clear x
jump main-loop
]])
bullets.check_for_firing()
bullets.update()
ship.update()
print(':stack-start data numbers [0]')
