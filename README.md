lua insertion ranking
================
[![Build Status](https://travis-ci.org/changnet/lua_insertion_ranking.svg?branch=master)](https://travis-ci.org/changnet/lua_insertion_ranking)

 a ranking module in game development with insertion sort algorithm.

 此库会保存所有曾经进入排行榜的数据。但仅对max_size个元素进行排序，其他排行均为0。
 主要是防止最后一名不断上榜、落榜时需要不断创建、销毁对象。


Installation
------------

 * git clone https://github.com/changnet/lua_insertion_ranking.git'
 * cd lua_insertion_ranking
 * make
 * make test
 * Copy lua_insertion_ranking.so to your lua project's c module directory

or embed to your project

Api
-----

```lua

-- create a rank object
-- make sure file_path is valid.it won't create any directory.
-- header name must be string
local Lir = require "lua_insertion_ranking"
local lir = Lir( "file_path","header1","header2","header3",... )

-- set rank factor.factor must number(integer).5 max factor support.
-- if unique_key not exist in rank,it create a new element(the old_pos is 0)
local new_pos,old_pos = lir:set_factor( unique_key,factor1,factor2,factor3,... )
local new_pos,old_pos = lir:set_one_factor( unique_key,factor,indexN )

-- get rank factor
local factor1,factor2,factor3,... = lir:get_factor( unique_key )
local factorN = lir:get_one_factor( unique_key,indexN )

-- set header value,name is header name,like "header1"
-- value can only be nil,number,string
-- if no such unique_key in rank,raise a error
lir:set_value( unique_key,value1,value2,value3,... )
lir:set_one_value( unique_key,name,value )

-- get header value,name is header name,like "header1"
local value1,value2,value3,... = lir:get_value( unique_key )
local value = lir:get_one_value( unique_key,name )

-- total element count
local sz = lir:size()

-- get unique key by rank position
-- if no such element in pos,return 0
local key = lir:get_key( pos )

-- get rank position by unique_key
-- if no such key in rank,return nil
local pos = lir:get_position( uinque_key )

save( file )
load( file )

add_header()
del_header()

-- delete a element from rank
-- if no such key in rank,return 0
local old_pos = lir:del( unique_key )

-- is there any change in rank
local modify = lir:modify()

-- dump current rank to std::cout(a file if file path is valid)
-- debug purpose only
lir:dump( file )
```
Example & Benchmark
-------

See 'test.lua'   

simple benchmark test 100000 times,encode elapsed time: 1.14 second,decode elapsed time: 2.36 second

