#ifndef __ICONV_LIB_H__
#define __ICONV_LIB_H__

#include <string>

extern "C" {
    #include "iconv.h"
}

//#include "universal.h"

    std::string GBK( const std::string& src );
    std::string GBK( const char* src );
    std::string UTF8 ( const std::string& src );
    std::string UTF8 ( const char* src );
    bool converse( const char* from, const char* to, const char* src );
    
#endif //__ICONV_LIB_H__
 
