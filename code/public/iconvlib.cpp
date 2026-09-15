#include <stdlib.h>
#include <string.h>

#include "streamlog.h"
#include "iconvlib.h"

#define BUF_CELL_LEN 102400       //512K

char g_pb[BUF_CELL_LEN+1];        //全局buffer

bool converse( const char* from, const char* to, const char* src )
{
    size_t srcLen = strlen(src);
    if ( srcLen > BUF_CELL_LEN ) 
    {
        g_pb[0] = 0;
        ErrorLog ( "被转换数据太大，转换失败![" << src << "]" );
        return false;
    }
    
    char *p_out = g_pb;
    
    iconv_t cd = 0;
    std::string ts = to;
    ts += "//TRANSLIT//IGNORE";
    
    if ( (iconv_t)-1 == (cd = iconv_open (ts.c_str(), from ) ) ) 
    {
        ErrorLog ("将串从" << from << " 转换为" << to << " 出错，iconv_open()出错");
        return false;
    }

    size_t len = 0;
    size_t dl = BUF_CELL_LEN;
    char * p = (char*)src;
    if ( (size_t)-1 == (len = iconv (cd, &p, &srcLen, &p_out, &dl ) ) )
    {
        ErrorLog ("将串从" << from << " 转换为" << to << " 出错，iconv()出错，"
            << "出错位置[" <<(p-src) << "], 出错位置串[" << std::string(p, 30) << "]" );

        g_pb[p-src] = 0;
        return false;
    }
    
    g_pb[BUF_CELL_LEN - dl] = 0;
    iconv_close ( cd );
    
    return true;
}

std::string GBK( const std::string& src )
{   
    if ( !converse ( "UTF-8", "GBK", src.c_str() )) return "";
    return g_pb;
}

std::string GBK( const char* src )
{   
    if ( !src ) return "";
    if ( !converse ( "UTF-8", "GBK", src )) return "";
    return g_pb;
}

std::string UTF8 ( const std::string& src )
{
    if ( !converse ( "GBK", "UTF-8", src.c_str() ) ) return "";
    return g_pb;
}

std::string UTF8 ( const char* src )
{
    if ( !src ) return "";
    if ( !converse ( "GBK", "UTF-8", src ) ) return "";
    return g_pb;
}

