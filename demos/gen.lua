local function IS_FREE() return 0 end
local function IS_FPT () return 1 end
local function IS_INT () return 2 end

local function VAR_IS_BURIED () return 0 end
local function VAR_IS_VISIBLE() return 1 end

local function make_reg_info()
    return { status = IS_FREE(), assigned_variable = nil }
end

local function make_variable_tracker()
    local variables = {}
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
        local var = variables[register[reg].assigned_variable]
        var.status = VAR_IS_BURIED()
        registers[reg].status = IS_FREE()
        stack[#stack + 1] = registers[reg].assigned_variable
        print('push '..reg)
    end
	local function revive(varname)
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
        variables[name] = {
            assigned_register = regname,
            status = VAR_IS_VISIBLE(),
            type_ = type_
        }
        if value ~= nil then
            self.assign(name, value)
        end
    end
    self.assign = function(name, value)
		local var = variables[name]
		if var == nil then
			error('Variable of name "'..name..'" has not been created.')
		elseif var.status == VAR_IS_VISIBLE() then
			local regname = var.assigned_register
			if var.type_ == IS_FPT() then
				io.write('set '..regname..value)
				if value % 1.0 == 0 then
					io.write('.\n')
				end
			elseif var.type_ == IS_INT() then
				print('set '..regname..math.floor(value + 0.5))
			end
		elseif var.status == VAR_IS_BURIED() then
			revive(name)
			assert(var.status == VAR_IS_VISIBLE())
			self.assign(name, value)
		end
    end
	self.complete = function()
		print(':stack-start data numbers [0]')
	end
	print('#!./erfindung\nset sp stack-start')
    return self
end
