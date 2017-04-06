#ifndef __LINSERTION_RANKING_H__
#define __LINSERTION_RANKING_H__

#include <lua.hpp>
extern "C"
{
extern int luaopen_lua_insertion_ranking( lua_State *L );
}

class lir
{
public:
private:
    typedef enum
    {
        LVT_NONE   ,
        LVT_INTEGER,
        LVT_NUMBER ,
        LVT_STRING ,
    }lval_t;

    typedef struct
    {
        lval_t _vt;
        union
        {
            char       *_str; 
            LUA_NUMBER  _num;
            LUA_INTEGER _int;
        }_v;
    }lval_t;

    typedef struct
    {
        int64_t _key;
        lval_t  _val;
    }element_t;
private:
    int _max_size;
    int _cur_size;

    kmap_t _kmap;
    element_t _list;
    // LUA_NUMBER
    // LUA_INTEGER
    // LUA_NUMBER_FMT
    // LUA_INTEGER_FMT

};

#endif /* __LINSERTION_RANKING_H__ */