#include "linsertion_ranking.hpp"

#include <cassert>
#include <cstring>

#define LIB_NAME "lua_insertion_ranking"


#define array_resize(type,base,cur,size)            \
    do{                                             \
        type *tmp = new type[size];                 \
        memset( tmp,0,sizeof(type)*size );          \
        memcpy( tmp,base,sizeof(type)*cur );        \
        delete []base;                              \
        base = tmp;                                 \
        cur = size;                                 \
    }while(0)

lir::~lir()
{
    for ( int i = 0;i < _max_header;i ++ )
    {
        if ( *(_header + i) ) del_string( *(_header + i) );
    }

    delete []_header;
}

lir::lir( const char *path,int max_size )
{
    // snprintf
    size_t sz = strlen( path );
    sz = sz > (MAX_PATH - 1) ? (MAX_PATH - 1) : sz;

    memcpy( _path,path,sz );

    _path[sz]  = 0;
    _cur_size  = 0;
    _max_size  = max_size;

    _cur_header = 0;
    _max_header = DEFAULT_HEADER;
    _header     = new char*[DEFAULT_HEADER];

    memset( _header,0,sizeof(char*)*_max_header );
}

int lir::add_header( const char *name,size_t sz )
{
    if ( _cur_header >= _max_header )
    {
        array_resize( char *,_header,_max_header,_max_header*2 );
    }

    *(_header + _cur_header) = new_string( name,sz );
    _cur_header ++;

    return _cur_header;
}

char *lir::new_string( const char *str,size_t sz )
{
    sz = 0 == sz ? strlen(str) : sz;

    char *new_str = new char[sz + 1];

    memcpy( new_str,str,sz );
    *(new_str + sz) = '\0'  ;

    return new_str;
}
void lir::del_string( const char *str )
{
    delete []str;
}

static int size( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #2 expect" LIB_NAME );
    }

    lua_pushinteger( L,(*_lir)->size() );

    return 1;
}

static int max_size( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #2 expect" LIB_NAME );
    }

    lua_pushinteger( L,(*_lir)->max_size() );

    return 1;
}

static int resize( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #2 expect" LIB_NAME );
    }

    int sz = luaL_checkinteger( L,2 );
    if ( sz < 0 )
    {
        return luaL_error( L, "resize to negetive size" );
    }
    lua_pushinteger( L,(*_lir)->resize( sz ) );

    return 1;
}


/* create a C++ object and push to lua stack */
static int __call( lua_State *L )
{
    /* lua调用__call,第一个参数是该元表所属的table.取构造函数参数要注意 */
    size_t sz = 0;
    const char *path = luaL_checklstring( L,2,&sz );
    if ( sz >= lir::MAX_PATH )
    {
        return luaL_error( L,"path(argument #1) too long" );
    }

    int max_size = luaL_checkinteger( L,3 );

    class lir* obj = new class lir( path,max_size );

    bool error = false;
    int top = lua_gettop( L );
    for ( int i = 4;i < top;i ++ )
    {
        if ( !lua_isstring( L,i ) )
        {
            error = true;
            break       ;
        }

        obj->add_header( lua_tostring( L,i ) );
    }

    if ( error )
    {
        delete obj;
        return luaL_error( L,"header must be string" );
    }

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

    lua_pushcfunction(L, max_size);
    lua_setfield(L, -2, "max_size");

    lua_pushcfunction(L, resize);
    lua_setfield(L, -2, "resize");

    /* metatable as value and pop metatable */
    lua_pushvalue( L,-1 );
    lua_setfield(L, -2, "__index");

    lua_newtable( L );
    lua_pushcfunction(L, __call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable( L,-2 );

    return 1;  /* return metatable */
}
