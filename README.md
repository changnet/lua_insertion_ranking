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

exist( pid ) -- 用get_position替代

set_factor()
set_one_factor()

get_factor()
get_one_factor()

set_value()
set_one_value()

get_value()
get_one_value()

size()

local key = get_key( pos )
local pos = get_position( key )

save( file )
load( file )

add_header()
del_header()

del( pid )

dump( file )
```
Example & Benchmark
-------

See 'test.lua'   

simple benchmark test 100000 times,encode elapsed time: 1.14 second,decode elapsed time: 2.36 second

