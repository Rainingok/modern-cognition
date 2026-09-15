#ifndef __EXT_TIMER_H__
#define __EXT_TIMER_H__

#include <sys/time.h>

class Timer
{
    protected:
        struct timeval _begin;
        struct timeval _start;

    public:
        Timer () { gettimeofday (&_start, NULL); _begin = _start; };

        inline void restart()
        {
            gettimeofday (&_start, NULL);
            _begin = _start;
        };

        inline double elapsed( bool reset=false )
        {
            struct timeval _end;
            gettimeofday (&_end, NULL);
            double  f = (_end.tv_usec - _start.tv_usec)/1000000.0;
            f += _end.tv_sec - _start.tv_sec;

            if ( reset ) gettimeofday (&_start, NULL);
            
            return f;
        };

        inline double total ()
        {
            struct timeval _end;
            gettimeofday (&_end, NULL);
            double  f = (_end.tv_usec - _begin.tv_usec)/1000000.0;
            f += _end.tv_sec - _begin.tv_sec;

            return f;
        };
            
};

#endif //__EXT_TIMER_H__
