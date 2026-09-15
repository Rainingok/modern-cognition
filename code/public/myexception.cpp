#include "myexception.h"

myexception::myexception ( const std::string& info ) 
{ 
    errcode = 0; 
    errinfo = info; 
}

myexception::myexception ( int eno, const std::string& info ) 
{ 
    errcode = eno; 
    errinfo = info; 
}

const char* myexception::what() const throw ()
{
    return errinfo.c_str();
}

