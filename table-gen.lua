function gen_table(root, size, nest_func)
    local rv = '<table>'
    rv = rv..'<tr><th>'..root..'</th><th>'..size..'px</th><th>'..size..'px</th></tr>'
    rv = rv..'<tr><th>'..size..'px</th><td>'..nest_func(0)..'</td><td>'..nest_func(1)..'</td></tr>'
    rv = rv..'<tr><th>'..size..'px</th><td>'..nest_func(2)..'</td><td>'..nest_func(3)..'</td></tr>'
    return rv..'</table>'
end

print([[
<!DOCTYPE html>
<html>
<head>
<style>
th { text-align: center; }
td { text-align: left  ; }
td, th {
    border: 1px solid black;
    padding: 2px;
}
table { border-collapse: collapse; }
</style>
</head>
<body>
]])
--[[
print(gen_table(' ', 128, function(pos)
    return gen_table(pos, 64, function(pos)
        return pos
    end)
end))
]]--
print(gen_table(' ', 128, function(pos)
    return gen_table(pos, 64, function(pos)
        return gen_table(pos, 32, function(pos)
            return gen_table(pos, 16, function(pos)
                return gen_table(pos, 8, function(pos)
                    return pos
                end)
            end)
        end)
    end)
end))
print('</body></html>')
