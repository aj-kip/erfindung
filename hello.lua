local alpha_map = {
}

local alpha_string = "abcdefghijklmnopqrstuvwxyz "

for i = 1, #alpha_string do
    local c = alpha_string:sub(i, i)
    alpha_map[c] = i + 4095
end

local target_string = "erfindung is great"--alpha_string --"hello world"
print(": do-print\n"..
      "set x 20\n")

for i = 1, #target_string do
    local c = target_string:sub(i, i)
    print("add x 8\n"..
          "set z "..alpha_map[c].."\n"..
          "io draw x y z\n")
end
print("pop pc\n")
print(": stack-start data numbers [0]")
