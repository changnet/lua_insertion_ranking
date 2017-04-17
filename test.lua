-- lua_flatbuffers test


function var_dump(data, max_level, prefix)
    if type(prefix) ~= "string" then
        prefix = ""
    end

    if type(data) ~= "table" then
        print(prefix .. tostring(data))
    else
        print(data)
        if max_level ~= 0 then
            local prefix_next = prefix .. "    "
            print(prefix .. "{")
            for k,v in pairs(data) do
                io.stdout:write(prefix_next .. k .. " = ")
                if type(v) ~= "table" or (type(max_level) == "number" and max_level <= 1) then
                    print(v)
                else
                    if max_level == nil then
                        var_dump(v, nil, prefix_next)
                    else
                        var_dump(v, max_level - 1, prefix_next)
                    end
                end
            end
            print(prefix .. "}")
        end
    end
end

--[[
    eg: local b = {aaa="aaa",bbb="bbb",ccc="ccc"}
]]
function vd(data, max_level)
    var_dump(data, max_level or 20)
end

local boolean = true
local str     = "abcdefghijklmnopqrstuvwxyz0123456789./*-+~!@#$%^&*()_+ABCEDFAZUZ"

local max_strlen = string.len( str )
function random_value()
    local srand = math.random( 1,4 )
    if 1 == srand then -- nil
        return nil
    elseif 2 == srand then -- boolean
        if boolean then
            boolean = false
            return     true
        end

        boolean = true
        return   false
    elseif 3 == srand then -- integer
        return math.random( 1,8888888 )
    elseif 4 == srand then -- number
        return math.random( 1,10000000 )*0.98735
    else  -- string
        local len = math.random( 0,max_strlen )
        if 0 == len then return "" end

        return string.sub( str,len )
    end

    return nil
end

local MAX_FCNT = 4
local MIN_RAND = 0
local MAX_RAND = 20000
local MAX_EMET = 400
local MAX_VIDX = 256
math.randomseed( os.time() )


local Lir = require "lua_insertion_ranking"

local lir = Lir( "test.lir" )

local factor_tbl = {}
for key_id = 1,MAX_EMET do
    for index = 1,MAX_FCNT do
        local factor = math.random( MIN_RAND,MAX_RAND ) * 0.9846
        factor_tbl[index] = factor
    end
    lir:set_factor( key_id,table.unpack(factor_tbl) );
    lir:set_value ( key_id,nil,true,123456,987654.123,"test string" )
end

for i = 1,MAX_EMET/2 do
    local key_id  = math.random( 1,MAX_EMET )
    local v_index = math.random( 1,MAX_VIDX )
    local value   = random_value()

    lir:set_one_value( key_id,value,v_index )
end

for i = 1,math.floor(MAX_EMET/2) do
    local key_id  = math.random( 1,MAX_EMET )
    local f_index = math.random( 1,MAX_FCNT )
    local factor = math.random( MIN_RAND,MAX_RAND ) * 1.9846

    lir:set_one_factor( key_id,factor,f_index )
end

for i = 1,math.floor(MAX_EMET/3) do
    local key_id  = math.random( 1,MAX_EMET )

    lir:del( key_id )
end

lir:dump()
lir:dump( "test.dmp" )

assert( true == lir:save() )
assert( false == lir:save() )
assert( true == lir:save( true ) )

print( lir:size() )

for i = 1,math.floor(MAX_EMET/3) do
    local key_id = math.random( 1,MAX_EMET )
    print( lir:get_factor( key_id ) )
    print( lir:get_value( key_id ) )

    local f_index = math.random( 1,MAX_FCNT )
    print( lir:get_factor( key_id,f_index ) )

    local v_index = math.random( 1,MAX_VIDX )
    print( lir:get_value ( key_id,v_index) )


    print( lir:get_key( i ) )
    print( lir:get_position( key_id ) )
end

local llir = Lir( "test.lir" )

print( "load from file",llir:load() )
print( "is any modify",llir:modify() )
llir:dump( "test.dmp" )

local MAX_TS = 100000
local lb = Lir( "benchmark.lir" )
local sx = os.clock()

for i = 1,MAX_TS do
    local key_id  = math.random( 1,MAX_EMET )
    local f_index = math.random( 1,MAX_FCNT )
    local factor = math.random( MIN_RAND,MAX_RAND ) * 1.9846

    lb:set_one_factor( key_id,factor,f_index )
end

local sy = os.clock()


print( string.format("simple benchmark test %d times elapsed time: %.2f second",
     MAX_TS,sy - sx))
