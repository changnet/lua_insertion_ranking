#ifndef __LINSERTION_RANKING_H__
#define __LINSERTION_RANKING_H__

#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map    std::unordered_map
#endif

#include <iostream>     // std::streambuf, std::cout
#include <cstring>

#include <lua.hpp>
extern "C"
{
extern int luaopen_lua_insertion_ranking( lua_State *L );
}

class lir
{
public:
    const static int MAX_FACTOR = 5;  // 最大排序因子数量
    const static int MAX_PATH   = 64; // 保存文件名路径长度

    const static int DEFAULT_SIZE = 32; // 默认分配排行数组大小

    // 默认变量分配大小
    const static int MAX_VALUE = 256;
    const static int DEFAULT_VALUE = 8;

    typedef double factor_t  ; // 排序因子类型
    typedef LUA_INTEGER key_t; // key类型，如玩家pid.LUA_INTEGER = int64_t lua5.3

    // lua中传入的值类型
    typedef enum
    {
        LVT_UNDEF = 0,// undefine类型必须为0，因为我们用memeset来初始化一个element_t
        LVT_NIL      , 
        LVT_BOOLEAN  ,
        LVT_INTEGER  ,
        LVT_NUMBER   ,
        LVT_STRING
    }lvt_t;

    // 用于表示一个lua变量
    typedef struct
    {
        lvt_t _vt;
        union
        {
            char       *_str; 
            LUA_NUMBER  _num;
            LUA_INTEGER _int;
        }_v;
    }lval_t;

    // 表示一个排序元素
    typedef struct
    {
        int      _pos;
        int      _vsz;
        key_t    _key;
        lval_t  *_val; // it is a array,size is _header_size
        factor_t _factor[MAX_FACTOR];
    }element_t;

    typedef map< key_t,element_t *> kmap_t;
    typedef map< key_t,element_t *>::iterator kmap_iterator;
public:
    ~lir();
    explicit lir( const char *path );

    // 打印排行榜到std::cout或者文件
    void dump( const char *path );

    // 更新排序因子，不存在则尝试插入
    int update_factor( key_t key,factor_t *factor,int factor_cnt,int &old_pos );
    // 更新单个排序因子
    int update_one_factor( key_t key,factor_t factor,int index,int &old_pos );

    // 当前排行的数量，最大数量，设置最大数量
    inline int size() { return _cur_size; }

    // 设置一个变量
    int update_one_value( key_t key,int index,const lval_t &lval );

    // 获取排序因子
    int get_factor( key_t key,factor_t **factor );

    // 获取变量
    int get_value( key_t key,lval_t **val );

    // 根据key获取所在排名
    int get_position( const key_t &key );

    // 根据排行获取key
    key_t *get_key( int pos );

    // 删除一个元素
    int del( const key_t &key );

    // 保存到文件
    int save();

    // 从文件加载数据
    int load();

    static void  del_string( const char *str );
    static char *new_string( const char *str,size_t sz = 0 );
private:
    void del_lval( const lval_t &lval );
    void del_element( const element_t *element );
    void cpy_lval( lval_t &to,const lval_t &from );

    int shift_up  ( element_t *element );
    int shift_down( element_t *element );
    int append( key_t key,factor_t *factor );

    // 对比排序因子
    int compare( const factor_t *fsrc,const factor_t *fdest );
    int compare( const element_t *esrc,const element_t *edest )
    {
        return compare( esrc->_factor,edest->_factor );
    }

    void raw_dump( std::ostream &os );

    // 写入字符串(长度加字符串内容)
    std::ostream &write_string( std::ostream &os,const char *str )
    {
        size_t sz = strlen( str );
        os.write( (char*)&sz,sizeof(sz) );

        os.write( str,sz );
        return          os;
    }
    // 读取字符串
    int read_string( std::istream &is,char *buffer,int max )
    {
        size_t sz = 0;
        is.read( (char*)&sz,sizeof(sz) );

        if ( !is.good() || sz > size_t(max - 1) ) return -1;

        is.read( buffer,sz );
        buffer[sz] = '\0';
        return is.gcount() == (int)sz ? (int)sz : -1;
    }
private:
    bool _modify; // 是否变更
    char _path[MAX_PATH];  // 保存的文件路径

    int _cur_factor; // 当前排序因子数量

    int _cur_size;    // _list的有效大小
    int _max_size;    // _list分配的大小
    element_t **_list; // 排行数组

    kmap_t _kmap;  // 以排行key则k-v映射，方便用key直接取排名

    // LUA_NUMBER
    // LUA_INTEGER
    // LUA_NUMBER_FMT
    // LUA_INTEGER_FMT

};

#endif /* __LINSERTION_RANKING_H__ */