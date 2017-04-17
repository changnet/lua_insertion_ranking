lua insertion ranking
================
[![Build Status](https://travis-ci.org/changnet/lua_insertion_ranking.svg?branch=master)](https://travis-ci.org/changnet/lua_insertion_ranking)

 A ranking module in game development with insertion sort algorithm.It design for
 small number of elements real time ranking,like DPS ranking.Each element is able
 to store several custom value.

Installation
------------

 * git clone https://github.com/changnet/lua_insertion_ranking.git'
 * cd lua_insertion_ranking
 * make
 * make test
 * Copy lua_insertion_ranking.so to your lua project's c module directory

or embed to your project

PS:
 * need >=lua5.3
 * if your complier support c++11(c++0x),it will be a little faster.Try make STD=c++0x
 * memory check:  
valgrind -v --leak-check=full --show-leak-kinds=all --track-origins=yes lua test.lua

Api
-----

```lua

-- require ranking library
local Lir = require "lua_insertion_ranking"

-- create a rank object with file path
-- make sure file_path is valid.it won't create any directory.
local lir = Lir( "file_path" )

-- set rank factor.factor must number(integer).5 max factor support.
-- if unique_key not exist in rank,it create a new element(the old_pos is 0)
local new_pos,old_pos = lir:set_factor( unique_key,factor1,factor2,factor3,... )
local new_pos,old_pos = lir:set_one_factor( unique_key,factor,indexN )

-- get rank factor
local factor1,factor2,factor3,... = lir:get_factor( unique_key )
local factorN = lir:get_one_factor( unique_key,indexN )

-- set a custom value
-- value can only be nil,boolean,number,string
-- if no such unique_key in rank,raise a error
lir:set_value( unique_key,value1,value2,value3,... )
lir:set_one_value( unique_key,value,indexN )

-- get custom value
-- if indexN is not specify,it return all value
local value1,value2,value3,... = lir:get_value( unique_key [,indexN] )

-- total element count
local sz = lir:size()

-- get unique key by rank position
-- if no such element in pos,return 0
local key = lir:get_key( pos )

-- get rank position by unique_key
-- if no such key in rank,return nil
local pos = lir:get_position( uinque_key )

-- save data to file in binary mode
-- if the ranking is not being modified,it do nothing unless f is true
-- the file is the file_path when you create object lir
-- if return if the file is being saved
lir:save( f )

-- load data from file,return the element load from a file
-- it the file does not exist or empty,it return 0
-- if any error occurs,it raise a error
lir:load( file_path )

-- delete a element from rank
-- if no such key in rank,return 0
local old_pos = lir:del( unique_key )

-- is there any change in rank
-- note:load data from file make this flag true
local modify = lir:modify()

-- dump current rank to std::cout(a file if file path is valid)
-- debug purpose only
lir:dump( file )
```

Example & Benchmark
-------

See 'test.lua'   

simple benchmark test sort 100000 times elapsed time: 0.07 second

