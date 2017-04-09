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
    const static int MAX_FACTOR = 5;  // 最大排序因子数量
    const static int MAX_PATH   = 64; // 保存文件名路径长度

    // 默认变量名头部分配大小
    // 比如排行榜中包含玩家的名字，那一列叫"name"，那这个name就称头部
    const static int DEFAULT_HEADER = 8;

    typedef double factor_t  ; // 排序因子类型
    typedef LUA_INTEGER key_t; // key类型，如玩家pid.LUA_INTEGER = int64_t lua5.3

    // lua中传入的值类型
    typedef enum
    {
        LVT_NIL    ,
        LVT_INTEGER,
        LVT_NUMBER ,
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
        key_t    _key;
        lval_t  *_val; // it is a array,size is _header_size
        factor_t _factor[MAX_FACTOR];
    }element_t;

    typedef map< key_t,element_t *> kmap_t;
    typedef map< key_t,element_t *>::iterator kmap_iterator;
public:
    ~lir();
    explicit lir( const char *path,int max_size );

    // 增加一个变量头部
    int add_header( const char *name,size_t sz = 0 );
    // 更新排序因子，不存在则尝试插入
    int update( key_t key,factor_t *factor,int factor_cnt );

    // 当前排行的数量，最大数量，设置最大数量
    inline int size() { return _cur_size; }
private:
    void  del_string( const char *str );
    char *new_string( const char *str,size_t sz = 0 );

    int shift_up  ( const element_t *element );
    int shift_down( const element_t *element );
    int append( key_t key,factor_t *factor,int factor_cnt );

    // 对比排序因子
    int compare( factor_t *fsrc,factor_t *fdest );
private:
    bool _modify; // 是否变更
    char _path[MAX_PATH];  // 保存的文件路径

    int _cur_factor; // 当前排序因子数量

    int _cur_size;    // _list的有效大小
    int _max_size;    // _list分配的大小
    element_t **_list; // 排行数组

    kmap_t _kmap;  // 以排行key则k-v映射，方便用key直接取排名

    char **_header ; // 变量名头部
    int _cur_header; // 当前头部数量
    int _max_header; // 最大部数量

    // LUA_NUMBER
    // LUA_INTEGER
    // LUA_NUMBER_FMT
    // LUA_INTEGER_FMT

};

#endif /* __LINSERTION_RANKING_H__ */