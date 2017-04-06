#include "linsertion_ranking.hpp"

#include <cassert>

#define LIB_NAME "lua_insertion_ranking"

static int size( lua_State *L )
{
    return 1;
}


/* create a C++ object and push to lua stack */
static int __call( lua_State *L )
{
    /* lua调用__call,第一个参数是该元表所属的table.取构造函数参数要注意 */
    class lir* obj = new class lir();

    lua_settop( L,1 ); /* 清除所有构造函数参数,只保留元表 */

    class lir** ptr = (class lir**)lua_newuserdata(L, sizeof(class lir*));
    *ptr = obj;

    /* 把新创建的userdata和元表交换堆栈位置 */
    lua_insert(L,1);

    /* 弹出元表,并把元表设置为userdata的元表 */
    lua_setmetatable(L, -2);

    return 1;
}

/* 元方法,__tostring */
static int __tostring( lua_State *L )
{
    class lir** ptr = (class lir**)luaL_checkudata(L, 1,LIB_NAME);
    if(ptr != NULL)
    {
        lua_pushfstring(L, "%s: %p", LIB_NAME, *ptr);
        return 1;
    }
    return 0;
}

/*  元方法,__gc */
static int __gc( lua_State *L )
{
    class lir** ptr = (class lir**)luaL_checkudata(L, 1,LIB_NAME);
    if ( *ptr != NULL ) delete *ptr;
    *ptr = NULL;

    return 0;
}

/* ====================LIBRARY INITIALISATION FUNCTION======================= */

int luaopen_lua_insertion_ranking( lua_State *L )
{
    //luaL_newlib(L, lua_parson_lib);
    if ( 0 == luaL_newmetatable( L,LIB_NAME ) )
    {
        assert( false );
        return 0;
    }

    lua_pushcfunction(L, __gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, __tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, size);
    lua_setfield(L, -2, "size");

    /* metatable as value and pop metatable */
    lua_pushvalue( L,-1 );
    lua_setfield(L, -2, "__index");

    lua_newtable( L );
    lua_pushcfunction(L, __call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable( L,-2 );

    return 1;  /* return metatable */
}
