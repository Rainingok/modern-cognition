#ifndef _MY_C_LOG_H__
#define _MY_C_LOG_H__

/*******************************************
  线程安全
*******************************************/

#define CONSOLE_QUIET 0
#define CONSOLE_OUT 1
#define CONSOLE_CGI 2

int initLog( const char* base_path, int print_console, unsigned log_size, int file_num );

int infoLog ( const char *format, ... );

int errorLog ( const char *format, ... );

int dataInfoLog ( const char* data, size_t len );

int dataErrorLog ( const char* data, size_t len );

#endif //_MY_C_LOG_H__

