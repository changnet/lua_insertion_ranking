#include "linsertion_ranking.hpp"

#include <cmath>
#include <cerrno>
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
    /* 0  */ "success",
    /* 1  */ "no such key found",
    /* 2  */ "value index too large",
    /* 3  */ "value index illegal",
    /* 4  */ "no such header",
    /* 5  */ "dumplicate header name",
    /* 6  */ "(illegal file)factor error",
    /* 7  */ "(illegal file)element size error",
    /* 8  */ "(illegal file)element value size error",
    /* 9  */ "(illegal file)element value type error",
    /* 10 */ "(illegal file)element string value error",
    /* 11 */ "(illegal file)can not update element value",
    /* 12 */ "end of file",
    /* 13 */ "ranking list must be empty when load data from file"
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

/* 将一个lua变量转换为一个lval_t变量
 * !!! 不要del_element此函数返回的lval_t
 */
static lir::lval_t lua_toelement( lua_State *L,int index )
{
    lir::lval_t lval;

    switch ( lua_type( L,index ) )
    {
        case LUA_TNIL     :
        {
            lval._vt = lir::LVT_NIL;
        }break;
        case LUA_TBOOLEAN :
        {
            lval._vt = lir::LVT_BOOLEAN;
            lval._v._int = lua_toboolean( L,index );
        }break;
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

                // please use this carefully,do not delete string
                lval._v._str = const_cast<char *>( lua_tostring( L,index ) );
        }break;
        default:
        {
            lval._vt = lir::LVT_UNDEF;
        }break;
    }

    return lval;
}

/* push a element to lua stack */
static void lua_pushelement( lua_State *L,const lir::lval_t &val )
{
    switch( val._vt )
    {
        case lir::LVT_BOOLEAN : lua_pushboolean( L,val._v._int ); break;
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
    for ( int i = 0;i < _cur_size;i ++ )
    {
        del_element( *(_list + i) );
    }

    delete []_list;
    _list = NULL;

    _kmap.clear();
}

lir::lir( const char *path )
{
    // snprintf
    size_t sz = strlen( path );
    sz = sz > (MAX_PATH - 1) ? (MAX_PATH - 1) : sz;

    _path[sz]  = 0;
    memcpy( _path,path,sz );

    _cur_size  = 0;
    _max_size  = DEFAULT_VALUE;
    _list = new element_t*[DEFAULT_VALUE];
    memset( _list,0,sizeof(element_t*)*_max_size );

    _modify = false;
    _cur_factor = 0;
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

void lir::cpy_lval( lval_t &to,const lval_t &from )
{
    if ( to._vt == LVT_STRING )
    {
        if ( from._vt == LVT_STRING )
        {
            size_t sz = strlen(to._v._str);
            if ( sz >= strlen(from._v._str) )
            {
                memcpy( to._v._str,from._v._str,sz );
                return;
            }
            else
            {
                del_string( to._v._str );
                to._v._str = new_string( from._v._str );

                return;
            }
        }
        del_string( to._v._str );
    }

    to._vt = from._vt;

    if ( from._vt != LVT_STRING )
    {
        to._v  = from._v ;
    }
    else
    {
        to._v._str = new_string( from._v._str );
    }
}

void lir::del_element( const element_t *element )
{
    if ( element->_val )
    {
        for ( int i = 0;i < element->_vsz;i ++ )
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
    assert( _cur_size <= _max_size );
    if ( _cur_size == _max_size )
    {
        array_resize( element_t*,_list,_max_size,_max_size*2 );
    }

    element_t *element = new element_t();
    element->_vsz = 0   ;
    element->_key = key ;
    element->_val = NULL;
    
    /* factor必须按MAX_FACTOR初始化。必须全部拷贝，以初始化element._factor */
    memcpy( element->_factor,factor,sizeof( element->_factor ) );

    _kmap[key]           = element;
    *(_list + _cur_size) = element;

    _cur_size++;
    element->_pos = _cur_size;

    return shift_up( element );
}

/* 更新排序因子，不存在则尝试插入 */
int lir::update_factor( key_t key,factor_t *factor,int factor_cnt,int &old_pos )
{
    _modify = true;

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
    _modify = true;

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
        os << '\t' << "factor" << i + 1;
    }

    os << '\t' << "values ..." << std::endl;

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
        for ( int hindex = 0;hindex < e->_vsz;hindex ++ )
        {
            const lval_t &lval = e->_val[hindex];
            switch ( lval._vt )
            {
                case LVT_UNDEF   : os << '\t';break;
                case LVT_NIL     : os << '\t' << "nil";break;
                case LVT_BOOLEAN : 
                    os << "\t" << (lval._v._int ? "true" : "false");break;
                case LVT_INTEGER : os << '\t' << lval._v._int;break;
                case LVT_NUMBER  : os << '\t' << lval._v._num;break;
                case LVT_STRING  : os << '\t' << lval._v._str;break;
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

/*
 * !!!! @lval should not need to delete mmemory here
 */
int lir::update_one_value( key_t key,int index,const lval_t &lval )
{
    _modify = true;

    kmap_iterator itr = _kmap.find( key );
    if ( itr == _kmap.end() )
    {
        return 1;
    }

    if ( index < 0 || index >= MAX_VALUE ) return 3;

    element_t *element = itr->second;
    if ( !element->_val )
    {
        int sz = DEFAULT_VALUE;
        while ( sz < index ) sz *=2;

        element->_vsz = sz;
        element->_val = new lval_t[sz];
        memset( element->_val,0,sizeof(lval_t)*sz ); // 预留
    }
    else if ( element->_vsz <= index )
    {
        int sz = element->_vsz;
        while ( sz <= index ) sz *=2;

        array_resize( lval_t,element->_val,element->_vsz,sz );
    }

    cpy_lval( *(element->_val + index),lval );// delete old value memory

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

    return itr->second->_vsz;
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
    _modify = true;

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
// @f 是否强制保存文件(force)
int lir::save( int f )
{
    if ( !f && !_modify ) return 0; // no need to save

    std::ofstream ofs( _path,std::ofstream::trunc | std::ofstream::binary );
    if ( !ofs.good() ) return -1;

    ofs.write( (char*)&_cur_factor,sizeof(_cur_factor) );

    ofs.write( (char*)&_cur_size,sizeof(_cur_size) );
    for ( int i = 0;i < _cur_size;i ++ )
    {
        const element_t *element = *(_list + i);

        ofs.write( (char*)&(element->_key),sizeof(element->_key) );
        for ( int findex = 0;findex < _cur_factor;findex ++ )
        {
            ofs.write( (char*)&(element->_factor[findex]),sizeof(factor_t) );
        }

        ofs.write( (char*)&element->_vsz,sizeof(element->_vsz) );

        for ( int vindex = 0;vindex < element->_vsz;vindex ++ )
        {
            const lval_t &lval = element->_val[vindex];

            ofs.write( (char*)&lval._vt,sizeof(lval._vt) );
            switch ( lval._vt )
            {
                case LVT_UNDEF   : // fall through
                case LVT_NIL     : break;
                case LVT_BOOLEAN : // fall through
                case LVT_INTEGER :
                    ofs.write( (char*)&lval._v._int,sizeof(lval._v._int) );break;
                case LVT_NUMBER  :
                    ofs.write( (char*)&lval._v._num,sizeof(lval._v._num) );break;
                case LVT_STRING  : write_string( ofs,lval._v._str ); break;
            }
        }
    }

    _modify = false;

    ofs.close();
    return    1;
}

int lir::load()
{
    enum step
    {
        ST_FCNT = 0,  // 读取排序因子数量
        ST_ECNT    ,  // 读取元素数量
        ST_EKEY    ,  // element key
        ST_EFCT    ,  // element factor
        ST_EVSZ    ,  // element value size
        ST_EVAL    ,  // element value
        ST_FCHK    ,  // finish check
        ST_DONE       // 完成
    };

    if ( 0 != _cur_size ) return 13;

    std::ifstream ifs( _path,std::ifstream::in );

    int step = ST_FCNT;
    int cur_size   = 0;

    key_t key;

    int cur_factor  = 0;
    int factor_size = 0;
    factor_t factor[MAX_FACTOR];

    int vsz = 0;
    int cur_vsz = 0;

    int _errno = 0;

    while( ifs.good() && 0 == _errno )
    {
        switch( step )
        {
        case ST_FCNT: // 读取排序因子数量
        {
            step ++;
            ifs.read( (char*)&cur_factor,sizeof(cur_factor) );
            if ( cur_factor < 0 || cur_factor > MAX_FACTOR ) _errno = 6;
        }break;
        case ST_ECNT: // 读取元素数量
        {
            ifs.read( (char*)&cur_size,sizeof(cur_size) );
            if ( cur_size < 0 )
            {
                _errno = 7;
                continue  ;
            }

            step = cur_size > 0 ? step + 1 : ST_DONE;
        }break;
        case ST_EKEY: // 读取key
        {
            step  ++;
            factor_size = 0; // reset factor size for current element
            ifs.read( (char*)&key,sizeof(key) );
        }break;
        case ST_EFCT: // 读取元素排序因子
        {
            ifs.read( (char*)(factor + factor_size),sizeof(factor_t) );

            if ( ++factor_size >= cur_factor ) // 所有排序因子都读取完成
            {
                step ++;

                int old_pos = 0;
                update_factor( key,factor,factor_size,old_pos );
            }
        }break;
        case ST_EVSZ: // 读取元素变量数量
        {
            cur_vsz = 0; // reset value size for current element
            ifs.read( (char*)&vsz,sizeof(vsz) );

            if ( vsz < 0 ) _errno = 8;

            step = vsz > 0 ? ST_EVAL : ST_FCHK;
        }break;
        case ST_EVAL: // 读取变量值
        {
            lval_t lval;
            ifs.read( (char*)&lval._vt,sizeof(lval._vt) );
            if ( !ifs.good() )
            {
                _errno = 9;
                continue  ;
            }

            switch( lval._vt )
            {
                case LVT_UNDEF   :
                case LVT_NIL     : break;
                case LVT_BOOLEAN : 
                case LVT_INTEGER : 
                    ifs.read( (char*)&lval._v._int,sizeof(lval._v._int) );break;
                case LVT_NUMBER  :
                    ifs.read( (char*)&lval._v._num,sizeof(lval._v._num) );break;
                case LVT_STRING  :
                {
                    static const int max = 256;
                    char buffer[max];

                    int sz = read_string( ifs,buffer,max );
                    if ( sz < 0 )
                    {
                        _errno = 10;
                        continue   ;
                    }

                    lval._v._str = buffer;//not new_string( buffer,sz );
                }break;
            }

            int err = update_one_value( key,cur_vsz,lval );
            if ( err )
            {
                _errno = 11;
                continue   ;
            }

            // 检查当前元素的变量是否读取完成
            if ( ++cur_vsz >= vsz ) step ++;
        }break;
        case ST_FCHK: // 检查是否还有下一个元素
        {
            step = _cur_size >= cur_size ? ST_DONE : ST_EKEY;
        }break;
        case ST_DONE:
        {
            ifs.close();
            return    0;
        }break;
        // end of switch
        }
    }

    if ( !ifs.good() && ST_DONE != step ) _errno = 12;

    ifs.close()  ;
    return _errno;   
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
        // !!!! this lval DO NOT need to delete and CAN ONT
        const lir::lval_t lval = lua_toelement( L,i );
        if ( lir::LVT_UNDEF == lval._vt )
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
    int index        = luaL_checkinteger( L,3 );

    // !!!! this lval DO NOT need to delete and CAN ONT
    const lir::lval_t lval = lua_toelement( L,4 );
    if ( lir::LVT_UNDEF == lval._vt )
    {
        return luaL_error( L,
            "unsouport value type %s",lua_typename(L, lua_type(L, 3)) );
    }

    int err = (*_lir)->update_one_value( key,index - 1,lval );
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

    int index = 0;
    if ( !lua_isnoneornil( L,3 ) )
    {
        index = luaL_checkinteger  ( L,3 );
        if ( index <= 0 || index > lir::MAX_VALUE )
        {
            return luaL_error( L, "argument #3 illegal" );
        }
    }

    lir::factor_t *factor = NULL;
    int factor_cnt = (*_lir)->get_factor( key,&factor );
    assert( factor_cnt >= 0 && factor_cnt <= lir::MAX_FACTOR );

    // get one factor
    if ( index > 0 )
    {
        if ( index >= factor_cnt ) return 0;

        lua_pushintegerornumber( L,*(factor + index - 1) );
        return 1;
    }

    if ( !lua_checkstack( L,factor_cnt ) )
    {
        return luaL_error( L,"stack overflow" );
    }

    for ( int i = 0;i < factor_cnt;i ++ )
    {
        lua_pushintegerornumber( L,*(factor + i) );
    }

    return factor_cnt;
}

/* 获取排序变量 */
static int get_value( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lir::key_t key   = luaL_checkinteger( L,2 );

    int index = 0;
    if ( !lua_isnoneornil( L,3 ) )
    {
        index = luaL_checkinteger  ( L,3 );
        if ( index <= 0 || index > lir::MAX_VALUE )
        {
            return luaL_error( L, "argument #3 illegal" );
        }
    }

    lir::lval_t *val = NULL;
    int val_cnt = (*_lir)->get_value( key,&val );
    assert( val_cnt >= 0 );

    // get one element
    if ( index > 0 )
    {
        if ( index >= val_cnt ) return 0;

        lua_pushelement( L,*(val + index - 1) );
        return 1;
    }

    if ( !lua_checkstack( L,val_cnt ) )
    {
        return luaL_error( L,"stack overflow" );
    }

    // get all element
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

/* 保存到文件 */
static int save( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    int f = lua_toboolean( L,2 );

    int s = (*_lir)->save( f );
    if ( s < 0  )
    {
        return luaL_error( L,strerror(errno) );
    }

    // return is file acturely save
    lua_pushboolean( L,s );

    return 1;
}

/* 保存到文件 */
static int load( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    int _errno = (*_lir)->load();
    if ( 0 != _errno )
    {
        raise_error( L,_errno );
    }

    return 0;
}

/* 排行榜是有变化 */
static int modify( lua_State *L )
{
    class lir** _lir = (class lir**)luaL_checkudata( L, 1, LIB_NAME );
    if ( _lir == NULL || *_lir == NULL )
    {
        return luaL_error( L, "argument #1 expect" LIB_NAME );
    }

    lua_pushboolean( L,(*_lir)->is_modify() );

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

    lua_pushcfunction(L, get_value);
    lua_setfield(L, -2, "get_value");

    lua_pushcfunction(L, get_key);
    lua_setfield(L, -2, "get_key");

    lua_pushcfunction(L, get_position);
    lua_setfield(L, -2, "get_position");

    lua_pushcfunction(L, del);
    lua_setfield(L, -2, "del");

    lua_pushcfunction(L, save);
    lua_setfield(L, -2, "save");

    lua_pushcfunction(L, load);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, modify);
    lua_setfield(L, -2, "modify");

    /* metatable as value and pop metatable */
    lua_pushvalue( L,-1 );
    lua_setfield(L, -2, "__index");

    lua_newtable( L );
    lua_pushcfunction(L, __call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable( L,-2 );

    return 1;  /* return metatable */
}
