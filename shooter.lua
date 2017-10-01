
function ship_features()
    local ship_sprite_label = 'ship-sprite-data'
    local ship_data_label   = 'ship-data'
    local ship_draw_label   = 'draw-ship'
    local ship_sprite_index = '0b01111111111'
    local function inline_load_pos(reg)
        return [[
        # just load positions from memory
        set ]]..reg..' '..ship_data_label..'\n'..[[
        load x ]]..reg..[[ 0
        load y ]]..reg..' 1 \n'
    end
    
    local function gen_ship_sprite_func()
        print([[
        :]]..ship_draw_label..[[
        
        push x y z
        ]]..inline_load_pos('z')..[[
        set z ]]..ship_sprite_index..[[
        
        sub x 8
        sub y 8
        io draw x y z
        jump ship-sprite-end
        :]]..ship_sprite_label..[[ data [
        # 0123456789ABCDEF
          ________________  # 0
          ________________  # 1
          _X__X______X__X_  # 2
          _XXXX______XXXX_  # 3
          _X__X______X__X_  # 4
          _X__X_XXXX_X__X_  # 5
          __X__X____X__X__  # 6
          __X__________X__  # 7 
          __X____XX____X__  # 8
          ___X________X___  # 9
          __X___X__X___X__  # A
          __XXXXX__XXXXX__  # B
          __X___X__X___X__  # C
          __X___X__X___X__  # D
          ________________  # E
          ________________  # F
          ] # end
        :ship-sprite-end pop z y x
        ]])
    end
    local function gen_ship_init()
        print([[
            push x y a z
            set a ]]..ship_sprite_label..'\n'..[[
            set z ]]..ship_sprite_index..'\n'..[[
            set x 16
            set y 16
            io upload x y a z
            jump end-ship-data
            :]]..ship_data_label..[[ data numbers [
                160 # x
                320 # y
                3   # lives
                0   # upgrade-level
            ]
            :end-ship-data
            pop x y a z
        ]])
    end
    local function gen_ship_update()
        print([[
           push x y z
           ]]..inline_load_pos('z')..[[
           io read controller z
           comp z    1
           skip z ==  # up is pressed
               jump skip-press-up
           add x -1
           jump skip-press-down
    : skip-press-up
           io read controller a
           comp z    2
           skip z ==  # down is pressed
               set pc skip-press-down
           add x 1
    : skip-press-down
           io read controller a
           comp z    4 # left is pressed
           skip z ==
               set pc skip-press-left
           add y -1
           jump do-ship-update
    : skip-press-left
           io read controller a
           comp z    8 # right is pressed
           skip z ==
               set pc do-ship-update
           add y 1
    : do-ship-update
           # save it back
           set  z ]]..ship_data_label..'\n'..[[
           save x z 0
           save y z 1
           pop z y x
        ]])
    end
    return {
        ship_draw   = gen_ship_sprite_func,
        ship_init   = gen_ship_init       ,
        ship_update = gen_ship_update
    }
end

ship_features().ship_init()
print(':main-loop')
--ship_features().ship_update()
ship_features().ship_draw()

print([[
io wait  x
io clear x
jump main-loop
]])
