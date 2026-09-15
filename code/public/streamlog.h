#ifndef __STREAM_LOG_H__
#define __STREAM_LOG_H__

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>

#include "clog.h"

#define InfoLog(dbgEvent) {std::ostringstream my_log_os_unique;my_log_os_unique<<dbgEvent<<std::endl;dataInfoLog(my_log_os_unique.str().c_str(),my_log_os_unique.str().length());}
#define Prompt(dbgEvent) {std::ostringstream my_log_os_unique;my_log_os_unique<<dbgEvent<<std::endl;dataInfoLog(my_log_os_unique.str().c_str(),my_log_os_unique.str().length());}
#define ErrorLog(dbgEvent) {std::ostringstream my_log_os_unique;my_log_os_unique<<dbgEvent<<std::endl;dataErrorLog(my_log_os_unique.str().c_str(),my_log_os_unique.str().length());}

template < typename T >
std::ostream& operator << (std::ostream& os, const std::vector< T >& n )
{
    for ( size_t i = 0; i < n.size(); i++ ) os << n[i] << "\n";
    return os;
};

template <typename T>
std::ostream& operator<< (std::ostream& os, const std::set< T >& n )
{
    for ( typename std::set< T >::const_iterator it = n.begin(); it != n.end(); ++it ) 
        os << *it << "\n";
    return os;
};

template <typename T>
std::ostream& operator<< (std::ostream& os, const std::multiset< T >& n )
{
    for ( typename std::multiset< T >::const_iterator it = n.begin(); it != n.end(); ++it ) os << *it << "\n";
    return os;
};

template <typename T1, typename T2>
std::ostream& operator<< (std::ostream& os, const std::map< T1, T2 >& n )
{
    for ( typename std::map< T1, T2 >::const_iterator it = n.begin(); it != n.end(); ++it ) 
        os << "    [" << it->first << "] : [" << it->second << "]\n";
    return os;
};

template <typename T1, typename T2>
std::ostream& operator<< (std::ostream& os, const std::multimap< T1, T2 >& n )
{
    for ( typename std::multimap< T1, T2 >::const_iterator it = n.begin(); it != n.end(); ++it ) 
        os << "    [" << it->first << "] : [" << it->second << "]\n";
    return os;
};

#endif //__STREAM_LOG_H__


