print("# no psuedo instructions! n.n")
print("set sp stack-start")
local music_utils = require 'music-utils'
local triangle = music_utils.triangle

-- ayyyy check out my mad algorythms
for _, x in ipairs { -50, 50, 0 } do
    triangle.set_tempo(4) 
    triangle.send_notes({ 500 + x, 375 + x, 500 + x, 
                          325 + x, 500 + x, 275 + x })
    triangle.set_tempo(8)
    triangle.send_notes({ 75 + x, 125 + x, 75 + x, 125 + x })
end

music_utils.finish_program()
