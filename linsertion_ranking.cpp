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

    for ( int i = 0;i < _cur_size;i ++ )
    {
        del_element( *(_list + i) );
    }

    delete []_list;
}

lir::lir( const char *path )
{
    // snprintf
    size_t sz = strlen( path );
    sz = sz > (MAX_PATH - 1) ? (MAX_PATH - 1) : sz;

    _path[sz]  = 0;
    memcpy( _path,path,sz );

    _cur_size  = 0;
    _max_size  = DEFAULT_HEADER;
    _list = new element_t*[DEFAULT_HEADER];
    memset( _list,0,sizeof(element_t*)*_max_size );

    _modify = false;
    _cur_factor = 0;

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

void lir::del_element( const element_t *element )
{
    if ( element->_val )
    {
        for ( int i = 0;i < _cur_header;i ++ )
        {
            const lval_t &lval = *(element->_val + i);
            if ( lval._vt == LVT_STRING )
            {
                del_string( lval._v._str );
            }
        }

        delete []element->_val;
    }

    delete element;
}

/* 对比排序因子
 * @fsrc @fdest 是一个大小为MAX_FACTOR的数组
 */
int lir::compare( const factor_t *fsrc,const factor_t *fdest )
{
    assert( _cur_factor > 0 );

    for ( int i = 0;i < _cur_factor;i ++ )
    {
        if ( fsrc[i] > fdest[i] )
        {
            return 1;
        }
        else if ( fsrc[i] < fdest[i] )
        {
            return -1;
        }
    }

    return 0;
}

/* 向前移动元素 */
int lir::shift_up( element_t *element )
{
    /* _pos是排名，从1开始
     * element->_pos - 2是取排在element前一个元素索引(从0开始)
     */
    for ( int index = element->_pos - 2;index > 0;index -- )
    {
        element_t *element_up = *(_list + index);

        if ( compare( element,element_up ) <= 0 ) break;

        // 交换位置
        element->_pos --;
        element_up->_pos ++;
        *(_list + index + 1) = element_up;
    }

    assert( element->_pos > 0 && element->_pos <= _cur_size );

    *(_list + element->_pos - 1) = element;
    return element->_pos;
}

/* 向后移动元素 */
int lir::shift_down( element_t *element )
{
    for ( int index = element->_pos;index <= _cur_size - 1;index ++ )
    {
        element_t *element_down = *(_list + index);

        if ( compare( element,element_down ) >= 0 ) break;

        // 交换
        element->_pos ++;
        element_down->_pos --;

        *(_list + index - 1) = element_down;
    }

    assert( element->_pos > 0 && element->_pos <= _cur_size );

    *(_list + element->_pos - 1) = element;
    return element->_pos;
}

/* 添加新元素到排行 */
int lir::append( key_t key,factor_t *factor,int factor_cnt )
{
    if ( _cur_size >= _max_size )
    {
        array_resize( element_t*,_list,_max_size,_max_size*2 );
    }

    element_t *element = new element_t();
    element->_pos = 0;
    element->_key = key ;
    element->_val = NULL;
    
    /* factor必须按MAX_FACTOR初始化。必须全部拷贝，以初始化element._factor */
    memcpy( element->_factor,factor,sizeof( element->_factor ) );

    _cur_size++;
    _kmap[key]           = element;
    *(_list + _cur_size) = element;

    element->_pos = _cur_size;

    return shift_up( element );
}

/* 更新排序因子，不存在则尝试插入 */
int lir::update( key_t key,factor_t *factor,int factor_cnt )
{
    // 自动更新全局最大排序因子(必须在compare、memcpy之前更新)
    if ( factor_cnt > _cur_factor ) _cur_factor = factor_cnt;

    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() ) return append( key,factor,factor_cnt );

    element_t *element = itr->second;
    int shift = compare( element->_factor,factor );

    /* factor必须按MAX_FACTOR初始化。必须全部拷贝，以初始化element._factor */
    memcpy( element->_factor,factor,sizeof( element->_factor ) );
    return shift > 0 ? shift_up( itr->second ) : shift_down( itr->second );
}

/* ====================LUA STATIC FUNCTION======================= */
/* 设置玩家的排序因子
 * self:set_factor( key_id,factor1,factor2,... )
 */
static int set_factor( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key = luaL_checkinteger( L,2 );

    lir::factor_t factor[lir::MAX_FACTOR] = { 0 };
    
    int factor_cnt = 0;
    int top = lua_gettop( L );

    /* 可以一次性传入多个factor，但不能超过MAX_FACTOR */
    if ( top - 2 > lir::MAX_FACTOR )
    {
        return luaL_error( L, 
            "too many ranking factor,%d at most",lir::MAX_FACTOR );
    }

    // factor只能是number(integer)类型
    for ( int i = 3;i < top;i ++ )
    {
        factor[factor_cnt++] = luaL_checknumber( L,i );
    }

    // 至少需要传入一个factor，其他可以默认为0
    if ( factor_cnt <= 0 )
    {
        return luaL_error( L, "no ranking factor specify" );
    }

    return (*_lir)->update( key,factor,factor_cnt );
}

static int size( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lua_pushinteger( L,(*_lir)->size() );

    return 1;
}



/* create a C++ object and push to lua stack */
static int __call( lua_State *L )
{
    /* lua调用__call,第一个参数是该元表所属的table.取构造函数参数要注意 */
    size_t sz = 0;
    const char *path = luaL_checklstring( L,2,&sz );
    if ( sz >= (size_t)lir::MAX_PATH )
    {
        return luaL_error( L,"path(argument #1) too long" );
    }

    class lir* obj = new class lir( path );

    bool error = false;
    int top = lua_gettop( L );
    for ( int i = 3;i < top;i ++ )
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

    // lua_pushcfunction(L, max_size);
    // lua_setfield(L, -2, "max_size");

    // lua_pushcfunction(L, resize);
    // lua_setfield(L, -2, "resize");

    lua_pushcfunction(L, set_factor);
    lua_setfield(L, -2, "set_factor");

    /* metatable as value and pop metatable */
    lua_pushvalue( L,-1 );
    lua_setfield(L, -2, "__index");

    lua_newtable( L );
    lua_pushcfunction(L, __call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable( L,-2 );

    return 1;  /* return metatable */
}
