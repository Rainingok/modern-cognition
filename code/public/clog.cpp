#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/file.h>

#ifdef __THREAD_LOG
#include <pthread.h>
#endif

#include "clog.h"

#define MAX_FILE_NAME_LEN 256    //日志路径的最大长度
const int MAX_LOG_LEN = 1024000;    //一条日志的最大长度

enum LogLevel
{
    LOG_INFO = 0,
    LOG_ERROR = 1,
    LOG_LEVEL_NUM
};

struct LogFile
{
    char base_path[MAX_FILE_NAME_LEN];
    char file_name[MAX_FILE_NAME_LEN];
    char type_name[16];                         //类型名称
    int handle;                                 //文件句柄
    unsigned log_size;                          //日志文件大小，5M
    int file_num;                               //日志文件的数量。
    int print_console;                          //是否将日志同时输出到屏幕。

#ifdef __THREAD_LOG    
    pthread_mutex_t mutex;                      //线程互斥量
#endif

    int process_id;                             //进程ID
    char process_name[256];                     //进程名
    time_t last_check;                          //最后检查时间

    char* log_buffer;
    int log_len;
};

LogFile g_logfiles[LOG_LEVEL_NUM];

int lock ( struct LogFile* log )
{
#ifdef __THREAD_LOG    
    if ( 0 != pthread_mutex_lock (&(log->mutex)) )
        return -1;
#endif

    return flock ( log->handle, LOCK_EX );
}

int unlock ( struct LogFile* log )
{
#ifdef __THREAD_LOG    
        pthread_mutex_unlock (&(log->mutex));
#endif    

    return flock ( log->handle, LOCK_UN );
}

char* getpname ( char* buf, int len )
{
    int reallen = 0;
    buf[0] = 0;
    if ( 0 >= (reallen=readlink ( "/proc/self/exe", buf, len)) )
        return NULL;

    buf[reallen] = 0;
    char* path_end = strrchr ( buf, '/' );
    if (path_end == NULL) return NULL;
    
    ++path_end;
    return path_end;
}
void writeHead ( struct LogFile* log, int flag )
{
    time_t current = time(NULL);

    lock ( log );
    log->log_len = 0;
    log->log_len += sprintf ( log->log_buffer+log->log_len, "\n\n\n         ==============================================================\n" );
    log->log_len += sprintf ( log->log_buffer+log->log_len, "                   [%s][%d] %s", log->process_name, log->process_id, ctime(&current) );
    log->log_len += sprintf ( log->log_buffer+log->log_len, "         ==============================================================\n\n\n" );
    write ( log->handle, log->log_buffer, log->log_len );
    log->log_len = 0;
    unlock ( log );

    if ( CONSOLE_OUT == log->print_console && LOG_INFO == flag) fprintf ( stderr, "%s", log->log_buffer );
}

int isBig ( struct LogFile* log )
{
    //是否需要新创建文件。
    struct stat  st;
    int status = 0;
    status = stat ( log->file_name, &st);
    
    //如果文件不存在，则重打开。
    //如果删除日志文件，在检查间隔期间，还是有可能丢失日志。
    if ( 0 != status )
    {
        if ( log->handle ) close ( log->handle );
        log->handle = open ( log->file_name, O_CREAT|O_APPEND|O_RDWR, S_IRWXU|S_IRWXG );
        return 0;
    }

    if ( st.st_size < log->log_size ) return 0;
    return 1;
}

int copyFile ( int oldfd, int newfd )
{   
    char buf[10241];
    size_t read_len = 0;
    
    if ( 0 != lseek ( oldfd, 0, SEEK_SET ) ) return 0;
    
    while ( 1 )
    {
        read_len = read ( oldfd, buf, 10240 );
        if ( 0 >= read_len ) break;

        write ( newfd, buf, read_len  );
    }

    fsync ( newfd );
    return 1;
}

int overlap ( struct LogFile* log )
{  
    char old_file[MAX_FILE_NAME_LEN];
    char new_file[MAX_FILE_NAME_LEN];
    int idx = log->file_num - 1;
    int fd = 0;
    
    //删除可能溢出的日志。
    sprintf ( new_file, "%s/%s.log.%d", log->base_path, log->type_name, log->file_num );
    if ( 0 == access ( new_file, F_OK ) ) 
    {
        remove ( new_file );    //删除该文件。
    }

    //所有日志依序号往后移。
    for ( ; idx > 0; --idx )
    {
        sprintf ( old_file, "%s/%s.log.%d", log->base_path, log->type_name, idx );
        sprintf ( new_file, "%s/%s.log.%d", log->base_path, log->type_name, idx+1 );
        if ( 0 == access ( old_file, F_OK ) ) 
        {
            rename ( old_file, new_file );
        }
    }

    //将当前日志文件往后移。
    fsync ( log->handle );
    sprintf ( new_file, "%s/%s.log.1", log->base_path, log->type_name );
    fd = open ( new_file, O_CREAT|O_TRUNC|O_RDWR, S_IRWXU|S_IRWXG );
    copyFile ( log->handle, fd );
    close ( fd );

    //清空当前文件。
    if ( -1 == ftruncate( log->handle, 0) )
    {
        if ( CONSOLE_OUT == log->print_console) fprintf( stderr, "Truncate file [%s] is failed!\n", log->file_name);
        return false;
    }

    return 1;
}

void writePrefix ( struct LogFile* log, int type )
{  
    struct timeval tval;
    gettimeofday ( &tval, NULL );
    time_t current = tval.tv_sec;
    struct tm* ptm = localtime(&current);

    if ( 600 < current - log->last_check && isBig ( log ) )
    {
        overlap ( log );
    }

#ifdef __THREAD_LOG
    if ( CONSOLE_CGI == log->print_console )
    {
        
        if ( LOG_INFO == type )
        { 
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][INFO  ][%6d][%6lu][%s] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, pthread_self(), log->process_name );
        }
        else if ( LOG_ERROR == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][ERROR ][%6d][%6lu][%s] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, pthread_self(), log->process_name );
        }
        else
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][UNKNOW][%6d][%6lu][%s] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, pthread_self(), log->process_name );
        }        
    }
    else
    {
        if ( LOG_INFO == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][INFO  ][%6d][%6lu] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, pthread_self() );
            if ( CONSOLE_OUT == log->print_console ) fprintf ( stderr, "[INFO  ][%-6d][%-6lu] - ", (int)tval.tv_usec, pthread_self() );
        }
        else if ( LOG_ERROR == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][ERROR ][%6d][%6lu] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, pthread_self() );
            if ( CONSOLE_OUT == log->print_console ) fprintf ( stderr, "[ERROR ][%-6d][%-6lu] - ", (int)tval.tv_usec, pthread_self() );
        }
        else
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][UNKNOW][%6d][%6lu] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, pthread_self() );
            if ( CONSOLE_OUT == log->print_console ) fprintf ( stderr, "[UNKNOW][%-6d][%-6lu] - ", (int)tval.tv_usec, pthread_self() );
        }
    }
    
#else        

    if ( CONSOLE_CGI == log->print_console )
    {
        
        if ( LOG_INFO == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][INFO  ][%6d][%s] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, log->process_name );
        }
        else if ( LOG_ERROR == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][ERROR ][%6d][%s] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, log->process_name );
        }
        else
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][UNKNOW][%6d][%s] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id, log->process_name );
        }        
    }
    else
    {
        if ( LOG_INFO == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][INFO  ][%6d] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id );
            if ( CONSOLE_OUT == log->print_console ) fprintf ( stderr, "[INFO  ][%-6d] - ", (int)tval.tv_usec );
        }
        else if ( LOG_ERROR == type )
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][ERROR ][%6d] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id );
            if ( CONSOLE_OUT == log->print_console ) fprintf ( stderr, "[ERROR ][%-6d] - ", (int)tval.tv_usec );
        }
        else
        {
            log->log_len += sprintf ( log->log_buffer+log->log_len, "[%02d/%02d %02d:%02d:%02d/%-6d][UNKNOW][%6d] - ", ptm->tm_mon+1, ptm->tm_mday, 
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)tval.tv_usec, log->process_id );
            if ( CONSOLE_OUT == log->print_console ) fprintf ( stderr, "[UNKNOW][%-6d] - ", (int)tval.tv_usec );
        }
    }
#endif        
}


int initLog( const char* base_path, int print_console, unsigned log_size, int file_num )
{
    int i = 0;
    
    for ( i = 0; i < LOG_LEVEL_NUM; ++i )
    {
        g_logfiles[i].log_size = 1024*1024*log_size;
        g_logfiles[i].file_num = file_num;
        g_logfiles[i].print_console = print_console;
        g_logfiles[i].process_id = getpid();
        g_logfiles[i].last_check = 0;
        g_logfiles[i].log_buffer = (char*)malloc ( MAX_LOG_LEN );
        g_logfiles[i].log_len = 0;

#ifdef __THREAD_LOG        
        if ( 0 != pthread_mutex_init (&g_logfiles[i].mutex, NULL) )
        {
            if ( CONSOLE_OUT == g_logfiles[i].print_console ) 
                fprintf ( stderr, "Init thread mutex failed.\n" );
            return 0;
        }
#endif        
        
        getpname(g_logfiles[i].process_name,256);
        
        if ( LOG_INFO == i )
        {
            strcpy ( g_logfiles[i].type_name, "info" );
        }
        else if ( LOG_ERROR == i )
        {
            strcpy ( g_logfiles[i].type_name, "error" );
        }
        else
        {
            strcpy ( g_logfiles[i].type_name, "unknow" );
        }
        
        strcpy ( g_logfiles[i].base_path, base_path );
        if ( '/' == g_logfiles[i].base_path[strlen(g_logfiles[i].base_path)-1] )
        {
            g_logfiles[i].base_path[strlen(g_logfiles[i].base_path)-1] = 0;
        }

        sprintf ( g_logfiles[i].file_name, "%s/%s.log", g_logfiles[i].base_path, g_logfiles[i].type_name );

        if ( g_logfiles[i].handle ) continue;

        g_logfiles[i].handle = open ( g_logfiles[i].file_name, O_CREAT|O_APPEND|O_RDWR, S_IRWXU|S_IRWXG );
        if ( !g_logfiles[i].handle )
        {
            if ( CONSOLE_OUT == g_logfiles[i].print_console ) 
                fprintf ( stderr, "Open log file [%s] failed, maybe permission denied.\n", g_logfiles[i].file_name );
            return 0;
        }

        writeHead( g_logfiles+i, i );
    }

    return 1;
}

int infoLog ( const char *format, ... )
{
    int loglen = 0;
    va_list ap;
    struct LogFile* log = g_logfiles + LOG_INFO;
    if ( 0 == log->handle ) return false;
    
    //这里，锁定文件，也相当于锁定了log_buffer
    if ( 0 == lock ( log ) )
	{
		log->log_len = 0;
		writePrefix( log, LOG_INFO );

		loglen = log->log_len;
		va_start (ap, format );
		log->log_len += vsnprintf ( log->log_buffer+log->log_len, MAX_LOG_LEN-log->log_len, format, ap );
		va_end ( ap );
		
		log->log_buffer[log->log_len++] = '\n';
		log->log_buffer[log->log_len] = 0;

		write ( log->handle, log->log_buffer, log->log_len );
		if ( CONSOLE_OUT == log->print_console) fprintf ( stderr, "%s", log->log_buffer+loglen );
		
		unlock ( log );
	}
	
    return 1;
}

int errorLog ( const char *format, ... )
{
    int loglen = 0;
    va_list ap;
    struct LogFile* log = g_logfiles + LOG_ERROR;
    if ( 0 == log->handle ) return false;
    
    //这里，锁定文件，也相当于锁定了log_buffer
    if ( 0 == lock ( log ) )
	{
		log->log_len = 0;
		writePrefix( log, LOG_ERROR );

		loglen = log->log_len;
		va_start (ap, format );
		log->log_len += vsnprintf ( log->log_buffer+log->log_len, MAX_LOG_LEN-log->log_len, format, ap );
		va_end ( ap );
		
		log->log_buffer[log->log_len++] = '\n';
		log->log_buffer[log->log_len] = 0;		
		
		write ( log->handle, log->log_buffer, log->log_len );
		
		//同时写入到低级别的日志中去。
		if ( 0 == lock ( g_logfiles+LOG_INFO ) )
		{
			write ( g_logfiles[LOG_INFO].handle, log->log_buffer, log->log_len );
			if ( CONSOLE_OUT == log->print_console) fprintf ( stderr, "%s", log->log_buffer+loglen );
			unlock ( g_logfiles+LOG_INFO );
		}
		
		unlock ( log );
    }
	
    return 1;
}

int dataInfoLog ( const char* data, size_t len )
{   
    struct LogFile* log = g_logfiles + LOG_INFO;
    if ( 0 == log->handle ) return false;
    
    if ( 0 == lock ( log ) )
	{
		log->log_len = 0;
		writePrefix( log, LOG_INFO );
		write ( log->handle, log->log_buffer, log->log_len );
		write ( log->handle, data, len );
		if (CONSOLE_OUT==log->print_console) fprintf ( stderr, "%s", data );
		unlock ( log );
	}
	
    return 1;
}

int dataErrorLog ( const char* data, size_t len )
{    
    struct LogFile* log = g_logfiles + LOG_ERROR;
    if ( 0 == log->handle ) return false;

    //flock不是线程互斥的。
    //if ( 0 == flock ( log->handle, LOCK_EX ) )
    if ( 0 == lock ( log ) )
	{
		log->log_len = 0;
		writePrefix( log, LOG_ERROR );
		write ( log->handle, log->log_buffer, log->log_len );
		write ( log->handle, data, len );

		//写入info 日志
		
		if ( 0 == lock ( g_logfiles+LOG_INFO ) )
		{
			write ( g_logfiles[LOG_INFO].handle, log->log_buffer, log->log_len );
			write ( g_logfiles[LOG_INFO].handle, data, len );
			unlock ( g_logfiles+LOG_INFO );
		}
		
		if (CONSOLE_OUT==log->print_console) fprintf ( stderr, "%s", data );
		unlock ( log );
	}
	
    return 1;
}



