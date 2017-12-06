local function IS_FREE() return 0 end
local function IS_FPT () return 1 end
local function IS_INT () return 2 end

local function VAR_IS_BURIED () return 0 end
local function VAR_IS_VISIBLE() return 1 end

local function make_reg_info()
    return { status = IS_FREE(), assigned_variable = nil }
end

local function make_variable_tracker()
    local vars = {}
    local registers = {
        x = make_reg_info(), y = make_reg_info(), z = make_reg_info(),
        a = make_reg_info(), b = make_reg_info(), c = make_reg_info()
    }
    --          least recent   <-->   most recent
    local lru = { 'x', 'y', 'z', 'a', 'b', 'c' }
    local stack = {}
    local self = {}
    local function find_least_used_register()
        for k, v in pairs(registers) do
            if v.status == IS_FREE() then
                return k, v
            end
        end
        -- select for smallest last_use
        return lru[1], registers[lru[1]]
    end
    local function bury(reg)
        local var = vars[register[reg].assigned_variable]
        var.status = VAR_IS_BURIED()
        registers[reg].status = IS_FREE()
        stack[#stack + 1] = registers[reg].assigned_variable
        print('push '..reg)
    end
    self.new_variable = function(name, type_, value)
        if not type_ == IS_FPT() or not type_ == IS_INT() then
            error('invalid type')
        end
        local regname, reginfo = find_least_used_register()
        if not reginfo.status == IS_FREE() then
            bury(regname)
        end
        assert(reginfo.status == IS_FREE())
        reginfo.status = type_
        reginfo.assigned_variable = name
        vars[name] = {
            assigned_register = regname,
            status = VAR_IS_VISIBLE(),
            type_ = type_
        }
        if value ~= nil then
            self.assign(name, value)
        end
    end
    self.assign = function(name, value)
        -- I can't load using negative integers, because I screwed up device
        -- addresses
        -- FML
    end
    return self
end
