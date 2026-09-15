#ifndef __MY_EXCEPTION_H__
#define __MY_EXCEPTION_H__

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

class myexception : public std::exception
{
    protected:
        int errcode;
        std::string errinfo;

    public:
        myexception() { errcode = 0; };
        myexception ( int eno ) { errcode = eno; };
        myexception ( const std::string& info );
        myexception ( int eno, const std::string& info );

        ~myexception() throw () {};

        virtual const char* what() const throw ();
        int errid() { return errcode;} ;
        
        template <typename T>
        myexception& operator << ( const T& v )
        {
            std::ostringstream os;
            os << errinfo << v;
            errinfo = os.str();
            return *this;
        };
        
};

#endif //__MY_EXCEPTION_H__

