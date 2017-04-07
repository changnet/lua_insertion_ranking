#ifndef __LINSERTION_RANKING_H__
#define __LINSERTION_RANKING_H__

#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map    std::unordered_map
#endif

#include <lua.hpp>
extern "C"
{
extern int luaopen_lua_insertion_ranking( lua_State *L );
}

class lir
{
public:
    ~lir();
    explicit lir( const char *path,int max_size );

    int add_header( const char *name,size_t sz = 0 );

    inline int size() { return _cur_size; }
    inline int max_size() { return _max_size; }
    inline int resize( int sz ){ return _max_size = sz; };
private:
    void  del_string( const char *str );
    char *new_string( const char *str,size_t sz = 0 );
public:
    const static int MAX_FACTOR = 5;
    const static int MAX_PATH   = 64;
private:
    const static int DEFAULT_HEADER = 8;

    typedef double factor_t  ;
    typedef LUA_INTEGER key_t; // int64_t lua5.3

    typedef enum
    {
        LVT_NIL    ,
        LVT_INTEGER,
        LVT_NUMBER ,
        LVT_STRING
    }lvt_t;

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

    typedef struct
    {
        key_t    _key;
        lval_t  *_val; // it is a array,size is _header_size
        factor_t _factor[MAX_FACTOR];
    }element_t;

    typedef map< key_t,element_t > kmap_t;
    typedef map< key_t,element_t >::iterator kmap_iterator;
private:
    char _path[MAX_PATH];

    int _max_size;
    int _cur_size;

    kmap_t _kmap;
    element_t *_list;

    char **_header ;
    int _cur_header;
    int _max_header;
    // LUA_NUMBER
    // LUA_INTEGER
    // LUA_NUMBER_FMT
    // LUA_INTEGER_FMT

};

#endif /* __LINSERTION_RANKING_H__ */