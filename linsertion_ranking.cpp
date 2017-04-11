#include "linsertion_ranking.hpp"

#include <cmath>
#include <cassert>

#include <fstream>      // std::ofstream

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


static const char* error_msg[] = 
{
    "success",
    "no such key found",
    "no such value name found",
    "value index illegal",
    "no such header",
    "dumplicate header name"
};

static void raise_error( lua_State *L,int err_code )
{
    if ( err_code > 0 && (size_t)err_code < sizeof(error_msg)/sizeof(char*) )
    {
        luaL_error( L,error_msg[err_code] );
        return;
    }

    luaL_error( L,"unknow error(%d)",err_code );
}

/* 将一个lua变量转换为一个lval_t变量 */
static lir::lval_t lua_toelement( lua_State *L,int index )
{
    lir::lval_t lval;

    switch ( lua_type( L,index ) )
    {
        case LUA_TNUMBER :
        {
            if ( lua_isinteger( L,index ) )
            {
                lval._vt = lir::LVT_INTEGER;
                lval._v._int = lua_tointeger( L,index );
            }
            else
            {
                lval._vt = lir::LVT_NUMBER;
                lval._v._num = lua_tonumber( L,index );
            }
        }break;
        case LUA_TSTRING :
        {
                lval._vt = lir::LVT_STRING;

                size_t sz = 0;
                const char *str = lua_tolstring( L,index,&sz );
                lval._v._str = lir::new_string( str,sz );
        }break;
        default:
        {
            lval._vt = lir::LVT_NIL;
        }break;
    }

    return lval;
}

/* push a element to lua stack */
static void lua_pushelement( lua_State *L,const lir::lval_t &val )
{
    switch( val._vt )
    {
        case lir::LVT_INTEGER : lua_pushinteger( L,val._v._int ); break;
        case lir::LVT_NUMBER  : lua_pushnumber ( L,val._v._num ); break;
        case lir::LVT_STRING  : lua_pushstring ( L,val._v._str ); break;
        default: lua_pushnil( L );
    }
}

/* push a number as integer if possible,otherwise push a number */
static void lua_pushintegerornumber( lua_State *L,double v )
{
    if ( v == (LUA_INTEGER)v ) // std::floor(v) == v
    {
        lua_pushinteger( L,v );
    }
    else
    {
        lua_pushnumber( L,v );
    }
}

lir::~lir()
{
    for ( int i = 0;i < _cur_header;i ++ )
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
    /* check dumplicate header name */
    if ( find_header( name ) > 0 ) return 5;

    int old_size = _max_header;
    if ( _cur_header >= _max_header )
    {
        array_resize( char *,_header,_max_header,_max_header*2 );
    }

    *(_header + _cur_header) = new_string( name,sz );
    _cur_header ++;

    // 如果预留的内存不足，重新分配所有元素的内存
    if ( old_size != _max_header && _cur_size > 0 )
    {
        for ( int index = 0;index < _cur_size;index ++ )
        {
            element_t *element = *(_list + index);
            if ( !element->_val )        continue;

            const lval_t*old_val = element->_val;
            element->_val= new lval_t[_max_header];
            memset( element->_val,0,sizeof(lval_t)*_max_header ); // 预留
            memcpy( element->_val,old_val,sizeof(lval_t)*_cur_header );

            delete []old_val;
        }
    }

    return 0;
}

int lir::del_header( const char *name )
{
    int index = find_header( name );
    if ( index < 0 ) return       4;

    _cur_header --;
    int mov_sz = _cur_header - index;

    // 移动所有元素val数组，保证header
    for ( int i = 0;i < _cur_size;i ++ )
    {
        element_t *element = *(_list + i);
        if ( !element->_val )    continue;

        del_lval( *(element->_val + index) );

        if ( mov_sz > 0 )
        {
            memmove( element->_val + index,
                element->_val + index + 1,sizeof(lval_t)*mov_sz );
        }
    }

    del_string( *(_header + index) );
    if ( mov_sz > 0 )
    {
        memmove( _header + index,_header + index + 1,sizeof(char*)*mov_sz );
    }

    return 0;
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

void lir::del_lval( const lval_t &lval )
{
    if ( lval._vt == LVT_STRING )
    {
        del_string( lval._v._str );
    }
}

void lir::del_element( const element_t *element )
{
    if ( element->_val )
    {
        for ( int i = 0;i < _cur_header;i ++ )
        {
            del_lval( *(element->_val + i) );
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
    for ( int index = element->_pos - 2;index >= 0;index -- )
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
int lir::append( key_t key,factor_t *factor )
{
    if ( _cur_size >= _max_size )
    {
        array_resize( element_t*,_list,_max_size,_max_size*2 );
    }

    element_t *element = new element_t();
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
int lir::update_factor( key_t key,factor_t *factor,int factor_cnt,int &old_pos )
{
    // 自动更新全局最大排序因子(必须在compare、memcpy之前更新)
    if ( factor_cnt > _cur_factor ) _cur_factor = factor_cnt;

    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() )
    {
        old_pos = 0;
        return append( key,factor );
    }

    element_t *element = itr->second;

    old_pos = element->_pos;
    int shift = compare( element->_factor,factor );

    if ( 0 == shift ) return old_pos; // no change

    /* factor必须按MAX_FACTOR初始化。必须全部拷贝，以初始化element._factor */
    memcpy( element->_factor,factor,sizeof( element->_factor ) );
    return shift > 0 ? shift_up( element ) : shift_down( element );
}

/* 更新单个排序因子，不存在则尝试插入 */
int lir::update_one_factor( key_t key,factor_t factor,int index,int &old_pos )
{
    // 自动更新全局最大排序因子(必须在compare、memcpy之前更新)
    if ( index > _cur_factor ) _cur_factor = index;

    index --; // C++ 从0开始，lua从1开始
    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() )
    {
        factor_t flist[MAX_FACTOR] = { 0 };
        flist[index] = factor;
        return append( key,flist );
    }

    element_t *element = itr->second;

    old_pos = element->_pos;
    if ( element->_factor[index] == factor )
    {
        return old_pos;
    }

    int shift = factor > element->_factor[index] ? 1 : -1;

    element->_factor[index] = factor;
    return shift > 0 ? shift_up( element ) : shift_down( element );
}

/* 打印整个排行榜数据 */
void lir::raw_dump( std::ostream &os )
{
    // print header
    os << "position";
    for ( int i = 0;i < _cur_factor;i ++ )
    {
        os << '\t' << "factor" << i;
    }
    for ( int i = 0;i < _cur_header;i ++ )
    {
        os << '\t' << *(_header + i);
    }
    os << std::endl;

    for ( int index = 0;index < _cur_size;index ++ )
    {
        const element_t *e = *(_list + index);

        // print position and key
        os << index + 1 << '\t' << e->_key;

        // print all factor
        for ( int findex = 0;findex < _cur_factor;findex ++ )
        {
            os << '\t' << e->_factor[findex];
        }

        // print all value
        if ( e->_val )
        {
            for ( int hindex = 0;hindex < _cur_header;hindex ++ )
            {
                const lval_t &lval = e->_val[hindex];
                switch ( lval._vt )
                {
                    case LVT_NIL     : os << '\t' << "nil";break;
                    case LVT_INTEGER : os << '\t' << lval._v._int;break;
                    case LVT_NUMBER  : os << '\t' << lval._v._num;break;
                    case LVT_STRING  : os << '\t' << lval._v._str;break;
                }
            }
        }

        os << std::endl;
    }
}

/* 打印到std::cout还是文件 */
void lir::dump( const char *path )
{
    if ( path )
    {
        std::ofstream ofs( path,std::ofstream::app );
        if ( ofs.good() )
        {
            raw_dump( ofs );
            ofs.close    ();

            return;
        }
    }

    raw_dump( std::cout );
}

/* 设置一个变量 */
int lir::update_one_value( key_t key,const char *name,lval_t &lval )
{
    int index = find_header( name );
    if ( index < 0 )      return 2;

    return update_one_value( key,index,lval );
}

int lir::update_one_value( key_t key,int index,lval_t &lval )
{
    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() )
    {
        return 1;
    }

    if ( index < 0 || index >= _cur_header ) return 3;

    element_t *element = itr->second;
    if ( !element->_val )
    {
        element->_val = new lval_t[_max_header];
        memset( element->_val,0,sizeof(lval_t)*_max_header ); // 预留
    }

    *(element->_val + index) = lval;

    return 0;
}

// 获取排序因子
int lir::get_factor( key_t key,factor_t **factor )
{
    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() ) return    0;

    *factor = itr->second->_factor;

    return _cur_factor;
}

// 获取变量
int lir::get_value( key_t key,lval_t **val )
{
    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() ) return    0;

    *val = itr->second->_val;

    return _cur_header;
}

// 获取变量
lir::key_t *lir::get_key( int pos )
{
    if ( pos < 0 || pos >= _cur_size ) return NULL;

    return &((*(_list + pos))->_key);
}

// 根据key获取所在排名
int lir::get_position( const key_t &key )
{
    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() ) return    0;

    return itr->second->_pos;
}

// 删除一个元素
int lir::del( const key_t &key )
{
    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() )
    {
        return 0;
    }

    // 当前元素后的都往前移动一个位置
    int pos = itr->second->_pos;
    for ( int index = pos;index < _cur_size;index ++ )
    {
        (*(_list + index))->_pos --;
        *(_list + index - 1) = *(_list + index);
    }

    *(_list + _cur_size - 1) = NULL;

    --_cur_size;
    del_element( itr->second );
    _kmap.erase( itr         );

    return pos;
}

// 保存到文件
int lir::save()
{
    std::ofstream ofs( _path,std::ofstream::trunc );
    if ( !ofs.good() ) return -1;

    // 为了方便修改，以text保存而不是bianry
    // text模式下，需要处理特殊字符

    /* 第一行是排行榜信息，行以\n标识
     * 第一个数字是排序因子数量
     * 往后分别是header名
     * 以tab分隔
     */
    ofs << _cur_factor;
    for ( int i = 0;i < _cur_header;i ++ )
    {
        ofs << '\t';
        write_string( ofs,*(_header + i) );
    }
    ofs << "\n";

    // 第二行开始，每一行是一个元素
    // 各个值分别是key、排序因子factor、变量，以tab分隔
    for ( int i = 0;i < _cur_size;i ++ )
    {
        const element_t *element = *(_list + i);

        ofs << element->_key;
        for ( int findex = 0;findex < _cur_factor;findex ++ )
        {
            ofs << '\t' << element->_factor[findex];
        }

        if ( element->_val )
        {
            for ( int vindex = 0;vindex < _cur_header;vindex ++ )
            {
                ofs << '\t';
                const lval_t &lval = element->_val[vindex];
                switch ( lval._vt )
                {
                    case LVT_NIL     : break;
                    case LVT_INTEGER : ofs << lval._v._int; break;
                    case LVT_NUMBER  : ofs << lval._v._num; break;
                    case LVT_STRING  : write_string( ofs,lval._v._str ); break;
                }
            }
        }
        ofs << "\n";
    }

    ofs.close();
    return    0;
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
    for ( int i = 3;i <= top;i ++ )
    {
        factor[factor_cnt++] = luaL_checknumber( L,i );
    }

    // 至少需要传入一个factor，其他可以默认为0
    if ( factor_cnt <= 0 )
    {
        return luaL_error( L, "no ranking factor specify" );
    }

    int old_pos = 0;
    int new_pos = (*_lir)->update_factor( key,factor,factor_cnt,old_pos );

    lua_pushinteger( L,new_pos );
    lua_pushinteger( L,old_pos );
    return 2;
}

/* 设置玩家单个排序因子
 * self:set_factor( key_id,factor1,factor2,... )
 */
static int set_one_factor( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key = luaL_checkinteger( L,2 );
    lir::factor_t factor = luaL_checknumber( L,3 );
    int index = luaL_checkinteger( L,4 );

    if ( index > lir::MAX_FACTOR )
    {
        return luaL_error( L, 
            "too many ranking factor,%d at most",lir::MAX_FACTOR );
    }

    int old_pos = 0;
    int new_pos = (*_lir)->update_one_factor( key,factor,index,old_pos );

    lua_pushinteger( L,new_pos );
    lua_pushinteger( L,old_pos );
    return 2;
}

/* 打印整个排行榜 */
static int dump( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    const char *path = luaL_optstring( L,2,NULL );

    (*_lir)->dump( path );
    return 0;
}

/* 获取当前排行榜数量 */
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

/* 设置变量值 */
static int set_value( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key   = luaL_checkinteger( L,2 );
    
    int top = lua_gettop( L );
    for ( int i = 3;i <= top;i ++ )
    {
        lir::lval_t lval = lua_toelement( L,i );
        if ( lir::LVT_NIL == lval._vt )
        {
            return luaL_error( L,
                "unsouport value type %s",lua_typename(L, lua_type(L, i)) );
        }

        int err = (*_lir)->update_one_value( key,i - 3,lval );

        if ( err ) raise_error( L,err );
    }

    return 0;
}

/* 设置单个变量值 */
static int set_one_value( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key   = luaL_checkinteger( L,2 );
    const char *name = luaL_checkstring( L,3 ) ;
    lir::lval_t lval = lua_toelement( L,4 )    ;
    if ( lir::LVT_NIL == lval._vt )
    {
        return luaL_error( L,
            "unsouport value type %s",lua_typename(L, lua_type(L, 3)) );
    }

    int err = (*_lir)->update_one_value( key,name,lval );
    if ( err )
    {
        raise_error( L,err );
    }

    return 0;
}

/* 获取排序因子 */
static int get_factor( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key   = luaL_checkinteger( L,2 );

    lir::factor_t *factor = NULL;
    int factor_cnt = (*_lir)->get_factor( key,&factor );
    assert( factor_cnt >= 0 && factor_cnt <= lir::MAX_FACTOR );

    for ( int i = 0;i < factor_cnt;i ++ )
    {
        lua_pushintegerornumber( L,*(factor + i) );
    }

    return factor_cnt;
}

/* 获取单个排序因子 */
static int get_one_factor( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key = luaL_checkinteger( L,2 );
    int index      = luaL_checkinteger( L,3 );

    if ( index <= 0 || index > lir::MAX_FACTOR )
    {
        return luaL_error( L, "argument #2 illegal" );
    }

    lir::factor_t *factor = NULL;
    int factor_cnt = (*_lir)->get_factor( key,&factor );
    assert( factor_cnt >= 0 && factor_cnt <= lir::MAX_FACTOR );

    if ( index > factor_cnt ) return 0;

    lua_pushintegerornumber( L,*(factor + index - 1) );

    return 1;
}

/* 获取排序因子 */
static int get_value( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key   = luaL_checkinteger( L,2 );

    lir::lval_t *val = NULL;
    int val_cnt = (*_lir)->get_value( key,&val );
    assert( val_cnt >= 0 );

    for ( int i = 0;i < val_cnt;i ++ )
    {
        lua_pushelement( L,*(val + i) );
    }

    return val_cnt;
}

/* 根据排名获取唯一key */
static int get_position( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key = luaL_checkinteger( L,2 );

    lua_pushinteger( L,(*_lir)->get_position( key ) );

    return 1;
}

/* 根据唯一key获取排名 */
static int get_key( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    int pos = lua_tointeger( L,2 );
    if ( pos <= 0 )
    {
        return luaL_error( L,"illegal rank position" );
    }

    lir::key_t *key = (*_lir)->get_key( pos - 1 );

    if ( !key ) return 0;

    lua_pushinteger( L,*key );
    return                  1;
}

/* 删除一个元素 */
static int del( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key = luaL_checkinteger( L,2 );

    int pos = (*_lir)->del( key );

    lua_pushinteger( L,pos );
    return                 1;
}

/* 增加一个header */
static int add_header( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    size_t sz = 0;
    const char *name = luaL_checklstring( L,2,&sz );
    if ( sz <= 0 )
    {
        return luaL_error( L,"illegal header name" );
    }

    int err = (*_lir)->add_header( name,sz );
    if ( err )
    {
        raise_error( L,err );
        return             0;
    }

    return 0;
}

/* 删除一个header */
static int del_header( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    const char *name = luaL_checkstring( L,2 );

    int err = (*_lir)->del_header( name );
    if ( err )
    {
        raise_error( L,err );
        return             0;
    }

    return 0;
}

/* 保存到文件 */
static int save( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    if ( 0 != (*_lir)->save() )
    {
        return luaL_error( L,strerror(errno) );
    }

    return 0;
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
    for ( int i = 3;i <= top;i ++ )
    {
        if ( !lua_isstring( L,i ) )
        {
            error = true;
            break       ;
        }

        int err = obj->add_header( lua_tostring( L,i ) );
        if ( err )
        {
            delete obj;
            raise_error( L,err );
            return             0;
        }
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

    lua_pushcfunction(L, dump);
    lua_setfield(L, -2, "dump");

    lua_pushcfunction(L, set_one_factor);
    lua_setfield(L, -2, "set_one_factor");

    lua_pushcfunction(L, set_factor);
    lua_setfield(L, -2, "set_factor");

    lua_pushcfunction(L, set_value);
    lua_setfield(L, -2, "set_value");

    lua_pushcfunction(L, set_one_value);
    lua_setfield(L, -2, "set_one_value");

    lua_pushcfunction(L, get_factor);
    lua_setfield(L, -2, "get_factor");

    lua_pushcfunction(L, get_one_factor);
    lua_setfield(L, -2, "get_one_factor");

    lua_pushcfunction(L, get_value);
    lua_setfield(L, -2, "get_value");

    lua_pushcfunction(L, get_key);
    lua_setfield(L, -2, "get_key");

    lua_pushcfunction(L, get_position);
    lua_setfield(L, -2, "get_position");

    lua_pushcfunction(L, del);
    lua_setfield(L, -2, "del");

    lua_pushcfunction(L, add_header);
    lua_setfield(L, -2, "add_header");

    lua_pushcfunction(L, del_header);
    lua_setfield(L, -2, "del_header");

    lua_pushcfunction(L, save);
    lua_setfield(L, -2, "save");

    /* metatable as value and pop metatable */
    lua_pushvalue( L,-1 );
    lua_setfield(L, -2, "__index");

    lua_newtable( L );
    lua_pushcfunction(L, __call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable( L,-2 );

    return 1;  /* return metatable */
}
