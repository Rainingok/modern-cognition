#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "mydb.h"

/*/////////////////////////////////////////// MACRO //////////////////////////////////*/

#define TRUE 1
#define FALSE 0

#define READ_LOCK 0
#define WRITE_LOCK 1

#define LT -1
#define EQ 0
#define GT 1

#define TYPE_INDEX 0x1
#define TYPE_LEAF  0x2

#define LOG_HEAD_LEN        8
#define LOG_FLAG_DIRTY      0
#define LOG_FLAG_LENGTH     1

#define LOG_STATUS_DIRTY    1
#define LOG_STATUS_OK       2
#define LOG_STATUS_FINISHED 3

#define DOLOG_TYPE_FALLBACK 0
#define DOLOG_TYPE_NEWLOG   1

#define SLICE_DIRTY    0
#define SLICE_TYPE     1
#define SLICE_LENGTH   2
#define SLICE_HEAD_LEN 4
#define SLICE_ITEM_LEN (SLICE_HEAD_LEN+g_rangesize*ITEM_LEN)


/*
#define SLICE_PARENT   3
#define PREV_SIBLING   4
#define NEXT_SIBLING   5
*/

#define UINT_SIZE sizeof(mydb_uint)
#define LOCK_SIZE 1

#define GLOBAL_LOCK 0
#define GLOBAL_HEAD_LEN 8
#define GLOBAL_HEAD_SIZE (UINT_SIZE*GLOBAL_HEAD_LEN)

#define ITEMS(slice) (slice+SLICE_HEAD_LEN)
#define ITEM_LEN (g_keylen+1)
#define ITEM_SIZE (ITEM_LEN*UINT_SIZE)
#define ITEM_AT(items,idx) (items+(idx)*ITEM_LEN)
#define ITEM_OFFSET(item) (*(item+g_keylen))

#define DATA_HEAD_LEN 2
#define DATA_DATA_LENGTH 0
#define DATA_NEXT_ADDR 1

#define SLICE_HEAD_SIZE (SLICE_HEAD_LEN*UINT_SIZE)
#define SLICE_SIZE (SLICE_HEAD_SIZE+ITEM_SIZE*g_rangesize)

//类函数宏定义
#define my_lock(fd,lock_mode,address,tried) {if(MYDB_OK!=lock(fd,lock_mode,address,LOCK_SIZE,tried))goto FATAL_ERROR;}
#define my_unlock(fd,address) {if(MYDB_OK!=unlock(fd,address,LOCK_SIZE))goto FATAL_ERROR;}

#define my_lseek(fd,offset,whence) {if(((off_t)(-1))==lseek(fd,offset,whence))goto FATAL_ERROR;}
#define my_write(fd,buf,count) {if((off_t)count!=(off_t)write(fd,buf,count))goto FATAL_ERROR;}
#define my_read(fd,buf,count) {if((off_t)count!=(off_t)read(fd,buf,count))goto FATAL_ERROR;}

#define pos_read(fd,offset,buf,count) {my_lseek(fd,offset,SEEK_SET);my_read(fd,buf,count);}
#define pos_write(fd,offset,buf,count) {my_lseek(fd,offset,SEEK_SET);my_write(fd,buf,count);}

#define allways_search(db,key,current_slice,slice,slice_address,ret){while(TRUE){\
    ret=write_search(db,key,current_slice,slice,slice_address);\
    if(MYDB_SYS_ERR==ret)goto FATAL_ERROR;\
    else if(MYDB_LOCKED==ret){my_printf("LOCKED, TRY[%d]......\n",++g_trynum);usleep(1);continue;}\
    else break;}}

#define catch_processing {my_printf("[%s][%d]:Fatal IO error[%d][%s]!\n",__FUNCTION__,__LINE__,errno,strerror(errno));return MYDB_SYS_ERR;}


//小于返回-1，等于返回0，大于返回1
#define compare(k1,k2,v) {\
    if(k1[0]<k2[0])v=LT;\
    else if(k1[0]>k2[0])v=GT;\
    else if(1==g_keylen&&k1[0]==k2[0])v=EQ;\
    else if(2==g_keylen){\
        if(k1[1]==k2[1])v=EQ;\
        else if(k1[1]<k2[1])v=LT;\
        else v=GT;}\
    else {\
        for(macro_i=2;macro_i<g_keylen;++macro_i)\
            if(k1[macro_i]<k2[macro_i]){v=LT;break;}\
            else if(k1[macro_i]>k2[macro_i]){v=GT;break;}\
        if(macro_i==g_keylen)v=EQ;\
        }}

/*/////////////////////////////////////////// MACRO //////////////////////////////////*/


int g_keylen = 0;
int g_rangesize = 0;

int g_locknum = 0;
int g_trynum = 0;

//MyDB句柄结构。
/*  注意，无论什么时候，不要直接修改此结构中的值。*/
struct stMyDB
{
    char* db_file;
    int range_size;
    int read_only;
    int db_mode;
    int key_len;
    
    int fd_data;
    int fd_log;
};

typedef mydb_uint Slice;


//一个Item包含两部分，一部分是key，一部分是offset
//但放在一个拉平的数组中。
typedef mydb_uint Item;

struct Log
{
    mydb_uint address;         //写入到哪里。
    int type;                  //数据类型
    int length;                //要写入的数据长度。
    void* data;                //要写入的数据。
    void* orig_data;           //原始数据。日志未写入前，位置所在的数据。
    struct Log* next;          //多条日志捆绑。
};

//日志，调试日志输出屏幕，非调试无日志。
#ifdef __MYDB_DBG
#define my_printf printf
#else
#define my_printf nil_printf
#endif

int nil_printf (__const char *__restrict __format, ...){return 0;}

///////////////////////////////// DEBUG FUNCTIONS ////////////////////////////////////
#include <sys/time.h>
#include <time.h>

struct timeval g_begin;
struct timeval g_start;

void restart()
{
    gettimeofday (&g_start, NULL);
    g_begin = g_start;
};

double elapsed( )
{
    struct timeval t_end;
    gettimeofday (&t_end, NULL);
    double  f = (t_end.tv_usec - g_start.tv_usec)/1000000.0;
    f += t_end.tv_sec - g_start.tv_sec;

    gettimeofday (&g_start, NULL);

    return f;
};

double total ()
{
    struct timeval t_end;
    gettimeofday (&t_end, NULL);
    double  f = (t_end.tv_usec - g_begin.tv_usec)/1000000.0;
    f += t_end.tv_sec - g_begin.tv_sec;

    return f;
};

///////////////////////////////// DEBUG FUNCTIONS ////////////////////////////////////

void HexDump(const void *pdata, unsigned int len)
{
    int cnt = 0;
    int n = 0;
    int cnt2 = 0;
    const char *data = (const char *)pdata;
    unsigned char buffer[20];
    unsigned int rpos = 0;
    
       if(pdata == 0 || len == 0)
    {
        return;
    }

    printf ( "Address           Hexadecimal values          Printable\n" );
    printf ( "-------  -----------------------------------------------  -------------\n" );
    printf ( "\n" );

    while ( 1 )
    {
        if(len <= rpos)
        {
            break;
        }
        if(len >= rpos + 16)
        {
            memcpy(buffer, data + rpos, 16);
            rpos += 16;
            cnt = 16;
        }
        else
        {
            memcpy(buffer, data + rpos, len - rpos);
            cnt = len - rpos;
            rpos = len;
        }
        if(cnt <= 0)
        {
            return;
        }
        printf ( "%7d  ", (int)rpos );

        cnt2 = 0;
        for ( n = 0; n < 16; n++ )
        {
            cnt2 = cnt2 + 1;
            if ( cnt2 <= cnt )
            {
                printf ( "%02x", (unsigned int)buffer[n] );
            }
            else
            {
                printf("  ");
            }
            printf (" ");
        }

        printf (" ");
        cnt2 = 0;
        for ( n = 0; n < 16; n++ )
        {
            cnt2 = cnt2 + 1;
            if ( cnt2 <= cnt )
            {
                if ( buffer[n] < 32 || 126 < buffer[n] )
                {
                    printf ( "." );
                }
                else
                {
                    printf( "%c", buffer[n] );
                }
            }
        }
        printf("\n");
    }

    return;
}

void show_key(mydb_key key) 
{
    int i = 0;
    printf ( "[" );
    for ( ; i < g_keylen; ++i )
    {
        printf ( "%lu", key[i] );
        if ( i < g_keylen-1 )
            printf ( " " );
    }
    
    printf ( "]" );
}

void show_slice ( Slice* slice, mydb_uint address )
{
    mydb_uint i = 0;
    Item* items = ITEMS(slice);

    //HexDump ( slice, SLICE_SIZE );
    
    printf ( "<<0x%lx * %lu|%lu|%lu>> ", address, slice[SLICE_DIRTY], slice[SLICE_TYPE], slice[SLICE_LENGTH] );

    for ( ; i < slice[SLICE_LENGTH]; ++i )
    {
        show_key(ITEM_AT(items,i));
        printf ( "/0x%lx", *(ITEM_AT(items,i)+g_keylen) );
        if ( i != slice[SLICE_LENGTH]-1 )
            printf ( " | " );
    }
    printf ( "\n" );
    
}

void show_log ( struct Log* plog )
{
    struct Log* p = plog;

    printf ( "-------------------------------\n" );
    for ( ; p; p = p->next )
    {
        if ( p->data )
        {
            printf ( "DATA: " );
            show_slice( (Slice*)p->data, p->address );
        }
        if ( p->orig_data )
        {
            printf ( "ORIG DATA: " );
            show_slice( (Slice*)p->orig_data, p->address );
        }
        printf ( "--------\n" );
    }
    printf ( "-------------------------------\n" );
}

MyDB_Ret dbg_index ( MyDB* db, mydb_uint address, int level )
{
    mydb_uint i = 0;
    Slice slice[SLICE_ITEM_LEN];
    memset ( slice, 0, SLICE_SIZE );

    if ( 0 == address ) address = GLOBAL_HEAD_SIZE;
    
    pos_read( db->fd_data, address, slice, SLICE_SIZE );

    for ( ; i < (mydb_uint)level; ++i ) printf ( "    " );
    show_slice( slice, address );

    if ( slice[SLICE_TYPE] & TYPE_LEAF )
    {
        return MYDB_OK;
    }
    
    for ( i = 0; i < slice[SLICE_LENGTH]; ++i )
    {
        if ( MYDB_OK != dbg_index ( db, ITEM_AT(ITEMS(slice),i)[g_keylen], level+1 ) )
            break;
    }

    return MYDB_OK;

FATAL_ERROR:
    catch_processing;
}

///////////////////////////////// DEBUG FUNCTIONS ////////////////////////////////////


MyDB_Ret lock ( int fd, int lock_mode, off_t start, off_t len, int tried )
{
    int cmd = F_SETLK;
    struct flock flk;
    flk.l_start = start;
    flk.l_len = len;
    flk.l_pid = 0;
    flk.l_whence = SEEK_SET;
    if ( READ_LOCK == lock_mode ) flk.l_type = F_RDLCK;
    else flk.l_type = F_WRLCK;

    errno = 0;
    if ( TRUE != tried ) cmd = F_SETLKW;
    if ( -1 == fcntl ( fd, cmd, &flk ) )
    {
        if ( TRUE == tried && (EACCES == errno || EAGAIN == errno) )
        {
            return MYDB_LOCKED;
        }
        else
        {
            my_printf ( "Lock file [%d] failed!\n", fd );
            return MYDB_SYS_ERR;
        }
    }
    ++g_locknum;
    return MYDB_OK;
}

MyDB_Ret unlock ( int fd, off_t start, off_t len )
{
    struct flock flk;
    
    flk.l_start = start;
    flk.l_len = len;
    flk.l_pid = 0;
    flk.l_whence = SEEK_SET;
    flk.l_type = F_UNLCK;
    if ( -1 == fcntl ( fd, F_SETLKW, &flk ) )
    {
        my_printf ( "Lock file [%d] [%s] failed!\n", fd, strerror( errno ) );
        return MYDB_SYS_ERR;
    }
    --g_locknum;
    return MYDB_OK;
}



///////////////////////////////////////// LOG //////////////////////////////////////////

struct Log* append_log ( struct Log* log, mydb_uint address, const void* data, const void* orig_data, 
    int length )
{
    //struct Log* p = NULL;
    struct Log* node = (struct Log*)malloc ( sizeof(struct Log) );
    node->data = NULL;
    node->orig_data = NULL;
    
    if ( data )
    {
        node->data = malloc ( length );
        memcpy ( node->data, data, length );
    }
    
    if ( orig_data )
    {
        node->orig_data = malloc ( length );
        memcpy ( node->orig_data, orig_data, length );
    }
    
    node->address = address;
    node->length = length;
    node->next = NULL;
    
    if ( !log ) return node;

    //新日志总是插入在最前。
    node->next = log;
    return node;


    /*
    //形成日志链表。
    for ( p = log; p; p = p->next )
    {
        if ( !(p->next) ) 
        {
            p->next = node;
            break;
        }
    }
    return log;
    */
}

MyDB_Ret free_log ( struct Log* log )
{
    struct Log* p = NULL;
    struct Log* prev = NULL;
    
    for ( p = log; p; )
    {
        if ( p->data ) free ( p->data );
        if ( p->orig_data ) free ( p->orig_data );
        prev = p;
        p = p->next;
        free ( prev );
    }

    return MYDB_OK;
}

//内部malloc空间，外部释放。
char* organize_log ( struct Log* plog, int* size )
{
    //保留一个标志位。一个日志条数位。
    int total_size = UINT_SIZE*LOG_HEAD_LEN;
    int length = 0;
    int offset = 0;
    struct Log* p = NULL;
    char* buf = NULL;
    mydb_uint* up = NULL;
    
    for ( p = plog; p; p = p->next )
    {
        total_size += p->length;
        total_size += UINT_SIZE*2;
        ++length;
    }

    buf = (char*)malloc ( total_size );

    up = (mydb_uint*)buf;
    up[LOG_FLAG_DIRTY] = LOG_STATUS_DIRTY;  //脏标记。
    up[LOG_FLAG_LENGTH] = length;            //长度标记。
    offset += LOG_HEAD_LEN*UINT_SIZE;
    for ( p = plog; p; p = p->next )
    {
        up = (mydb_uint*)(buf+offset);
        up[0] = p->address;     //地址。
        up[1] = p->length;      //长度。
        offset += 2*UINT_SIZE;
        memcpy ( buf+offset, p->orig_data, p->length );
        offset += p->length;
    }

    *size = total_size;
    return buf;
}

MyDB_Ret read_log ( MyDB* db, struct Log** plog )
{
    mydb_uint log_head[LOG_HEAD_LEN];
    mydb_uint log_flag[2];
    char* pbuf = NULL;
    mydb_uint buf_len = 0;
    mydb_uint i = 0;
    
    //读取日志头。
    pos_read ( db->fd_log, 0, log_head, LOG_HEAD_LEN*UINT_SIZE );

    //未写完的日志，或已做完的日志。不管
    if ( LOG_STATUS_DIRTY == log_head[LOG_FLAG_DIRTY] 
        || LOG_STATUS_FINISHED == log_head[LOG_FLAG_DIRTY] )
    {
        return MYDB_LGC_ERR;
    }

    for ( i = 0; i < log_head[LOG_FLAG_LENGTH]; ++i )
    {
        my_read ( db->fd_log, log_flag, 2*UINT_SIZE );
        if ( buf_len < log_flag[1] )
        {
            if ( pbuf ) free (pbuf);
            pbuf = (char*)malloc(log_flag[1]);
            buf_len = log_flag[1];
        }

        my_read ( db->fd_log, pbuf, log_flag[1] );

        *plog = append_log( *plog, log_flag[0], NULL, pbuf, log_flag[1] );
    }
    
    return MYDB_OK;
    
FATAL_ERROR:
    if ( pbuf ) free ( pbuf );
    if ( *plog ) free_log ( *plog );
    
    unlock ( db->fd_log, 0, LOCK_SIZE );
    catch_processing;
    
}

MyDB_Ret exec_log ( MyDB* db, struct Log* log, int type )
{
    //这里，有一个临时方案。
    //写日志时，这里，默认了所有的日志，都是slice格式。
    //这不总是真的。
    //每次日志操作时，都会把最上层的slice置成脏。
    //这样，保证了即使日志写失败，也不至于影响数据准确性。
    Slice* slice = NULL;
    mydb_uint no_dirty = FALSE;

    struct Log* p = NULL;

    if ( !log || !db )
        goto FATAL_ERROR;
    
    for ( p = log; p; p = p->next )
    {
        if ( DOLOG_TYPE_NEWLOG == type )
        {
            if ( !p->data ) goto FATAL_ERROR;
            
            if ( p == log )
            {
                slice = (Slice*)(p->data);
                slice[SLICE_DIRTY] = TRUE;
            }
            
            pos_write ( db->fd_data, p->address, p->data, p->length );
        }
        else
        {
            if ( !p->orig_data ) goto FATAL_ERROR;
            
            if ( p == log )
            {
                slice = (Slice*)(p->orig_data);
                slice[SLICE_DIRTY] = TRUE;
            }
            
            pos_write ( db->fd_data, p->address, p->orig_data, p->length );
        }
    }

    //数据写成功，回置脏标记。
    pos_write ( db->fd_data, log->address+SLICE_DIRTY*UINT_SIZE, &no_dirty, UINT_SIZE );
    return MYDB_OK;

FATAL_ERROR:
    catch_processing;    
}

MyDB_Ret redo_log ( MyDB* db )
{
    MyDB_Ret ret;
    struct Log* plog = NULL;
    mydb_uint log_flag = LOG_STATUS_FINISHED;
    
    ret = read_log ( db, &plog );
    if ( MYDB_SYS_ERR == ret ) 
        goto FATAL_ERROR;
    else if ( MYDB_LGC_ERR == ret )
        return MYDB_OK;

    my_printf ( "Dirty data log!\n" );

    //show_log ( plog );
    
    //执行日志。
    ret = exec_log ( db, plog, DOLOG_TYPE_FALLBACK );
    if ( MYDB_OK != ret ) 
        goto FATAL_ERROR;
    
    //回置日志完成标志
    pos_write ( db->fd_log, LOG_FLAG_DIRTY*UINT_SIZE, &log_flag, UINT_SIZE );

    free_log ( plog );
    return MYDB_OK;
    
FATAL_ERROR:
    free_log ( plog );
    catch_processing;

}

//回退日志。
MyDB_Ret fallback_log ( MyDB* db )
{
    MyDB_Ret ret;
    
    //锁定日志文件。
    my_lock( db->fd_log, WRITE_LOCK, 0, FALSE );

    ret = redo_log ( db );
    if ( MYDB_SYS_ERR == ret )
        goto FATAL_ERROR;

    my_unlock ( db->fd_log, 0 );
    return MYDB_OK;

FATAL_ERROR:
    unlock ( db->fd_log, 0, LOCK_SIZE );
    catch_processing;    
}

MyDB_Ret do_log ( MyDB* db, struct Log* log )
{
    MyDB_Ret ret;
    char* pbuf = NULL;
    int log_size = 0;
    mydb_uint log_flag = LOG_STATUS_OK;

    //show_log ( log );
    
    //锁定日志文件。
    my_lock( db->fd_log, WRITE_LOCK, 0, FALSE );

    //做日志之前，先检查是否有脏日志。
    ret = redo_log ( db );
    if ( MYDB_SYS_ERR == ret )
        goto FATAL_ERROR;

    //组织日志。
    pbuf = organize_log( log, &log_size );

    //写入日志。
    pos_write ( db->fd_log, 0, pbuf, log_size );

    //更新写入日志成功标记。
    pos_write ( db->fd_log, LOG_FLAG_DIRTY*UINT_SIZE, &log_flag, UINT_SIZE );

    //做日志。
    ret = exec_log ( db, log, DOLOG_TYPE_NEWLOG  );
    if ( MYDB_OK != ret )
        goto FATAL_ERROR;

    //更新日志完成标记。
    log_flag = LOG_STATUS_FINISHED;
    pos_write ( db->fd_log, LOG_FLAG_DIRTY*UINT_SIZE, &log_flag, UINT_SIZE );

    //解锁返回。
    my_unlock ( db->fd_log, 0 );
    if ( pbuf ) free ( pbuf );
    return MYDB_OK;

FATAL_ERROR:
    if ( pbuf ) free ( pbuf );
    unlock ( db->fd_log, 0, LOCK_SIZE );
    catch_processing;     
}


///////////////////////////////////////// LOG //////////////////////////////////////////


MyDB_Ret new_slice_write ( MyDB* db, const void* data, int len, mydb_uint* address )
{
    //锁定全局写标志。
    MyDB_Ret ret = lock ( db->fd_data, WRITE_LOCK, GLOBAL_LOCK, LOCK_SIZE, FALSE );
    if ( MYDB_OK != ret ) return ret;
    
    *address = lseek ( db->fd_data, 0, SEEK_END );
    if ( (mydb_uint)(-1) == *address ) 
        goto FATAL_ERROR;

    pos_write ( db->fd_data, *address, data, len );
    my_unlock( db->fd_data, GLOBAL_LOCK );
    return MYDB_OK;

FATAL_ERROR:
    unlock ( db->fd_data, GLOBAL_LOCK, LOCK_SIZE );
    catch_processing;
}

MyDB_Ret new_data_write ( MyDB* db, const void* data, int len, mydb_uint* address )
{
    char* p = (char*)malloc ( DATA_HEAD_LEN*UINT_SIZE+len );
    ((mydb_uint*)p)[DATA_DATA_LENGTH] = len;
    ((mydb_uint*)p)[DATA_NEXT_ADDR] = 0;
    memcpy ( p+DATA_HEAD_LEN*UINT_SIZE, data, len );

    //锁定全局写标志。
    MyDB_Ret ret = lock ( db->fd_data, WRITE_LOCK, GLOBAL_LOCK, LOCK_SIZE, FALSE );
    if ( MYDB_OK != ret ) 
    {
        free ( p );
        return ret;
    }
    
    *address = lseek ( db->fd_data, 0, SEEK_END );
    if ( (mydb_uint)(-1) == *address ) 
        goto FATAL_ERROR;

    pos_write ( db->fd_data, *address, p, (unsigned)(DATA_HEAD_LEN*UINT_SIZE+len) );
    my_unlock( db->fd_data, GLOBAL_LOCK );
    free ( p );
    return MYDB_OK;

FATAL_ERROR:
    free ( p );
    unlock ( db->fd_data, GLOBAL_LOCK, LOCK_SIZE );
    catch_processing;
}


//正常返回会执有锁。
//Slice的空间，在外部分配。
MyDB_Ret slice_read ( MyDB* db, mydb_uint address, Slice* slice, int lock_mode, int tried )
{
    MyDB_Ret ret = lock ( db->fd_data, lock_mode, address, LOCK_SIZE, tried );
    if ( MYDB_OK != ret ) return ret;
    
    pos_read ( db->fd_data, address, slice, SLICE_SIZE );

    //show_slice ( slice, address );
    
    return MYDB_OK;


FATAL_ERROR:
    unlock ( db->fd_data, address, LOCK_SIZE );
    catch_processing;
}

//pos: 如果找到，则返回确定节点的位置。
//如果没有找到，则返回插入位置。
MyDB_Ret slice_bisection_find ( Slice* slice, mydb_key key, int* pos )
{
    int macro_i=0;
    int v = 0, v1 = 0;
    int length = (int)(slice[SLICE_LENGTH]);
    int low = 0, high = length-1, mid=0;
    mydb_uint* items = ITEMS(slice);

    //对长度为0的特殊情况进行处理。
    if ( 0 == length )
    {
        if ( slice[SLICE_TYPE] & TYPE_INDEX )
            goto FATAL_ERROR;
        else 
        {
            *pos = -1;
            return MYDB_LGC_ERR;
        }
    }
    
    while ( low <= high )
    {
        mid = (low+high)/2;

        compare(key,ITEM_AT(items,mid),v);
        if ( slice[SLICE_TYPE] & TYPE_LEAF )        //是叶子的情况。
        {
            if ( EQ == v )
            {
                *pos = mid;
                return MYDB_OK;
            }
            else if ( GT == v ) 
                low = mid+1;
            else
                high = mid-1;
        }
        else        //是区间的情况。
        {
            if ( mid == length-1 )
            {
                if ( EQ <= v )
                {
                    *pos = mid;
                    return MYDB_OK;
                }
                
                goto FATAL_ERROR;
            }
            
            compare(key,ITEM_AT(items,mid+1),v1);
            if ( EQ <= v && LT == v1 )
            {
                *pos = mid;
                return MYDB_OK;
            }
            else if ( GT == v ) 
                low = mid+1;
            else
                high = mid-1;
        }
    }

    if ( slice[SLICE_TYPE] & TYPE_LEAF )
    {
        if ( LT == v ) *pos = mid-1;
        else *pos = mid;
        return MYDB_LGC_ERR;    //未找到。
    }
    
    //如果区间搜索不到，那就标志着区间索引被破坏了!
FATAL_ERROR:    
    my_printf ( "Range index was destoried!\n" );
    return MYDB_SYS_ERR;
}

MyDB_Ret read_single_data ( MyDB* db, mydb_uint address, void* data, int* len )
{
    if ( !data || !len )
        return MYDB_LGC_ERR;
    
    mydb_uint head[DATA_HEAD_LEN];
    pos_read ( db->fd_data, address, head, DATA_HEAD_LEN*UINT_SIZE);
    if ( (unsigned)(*len) < head[DATA_DATA_LENGTH] )
    {
        my_printf ( "Buffer is too short [%u][%lu]\n", *len, head[DATA_DATA_LENGTH] );
        *len = head[DATA_DATA_LENGTH];
        return MYDB_LGC_ERR;
    }
    pos_read ( db->fd_data, address+DATA_HEAD_LEN*UINT_SIZE, data, head[DATA_DATA_LENGTH] );
    *len = head[DATA_DATA_LENGTH];

    return MYDB_OK;
    
FATAL_ERROR:
    catch_processing;
}

//返回数据的偏移位置。
//该函数在要读数据的情况下，会返回锁。
MyDB_Ret read_search( MyDB* db, mydb_key key, mydb_uint* data_addr, mydb_uint* lockup )
{
    MyDB_Ret ret;
    int pos = 0;
    mydb_uint lock_addr = 0;    //锁位置

    mydb_uint address = GLOBAL_HEAD_SIZE;
    Slice slice[SLICE_ITEM_LEN];
    

    //执有当前读的slice的锁，使当前slice不会被改变。
    //这样，就保证了当前内存中的数据的完整性。
    //也保证了子树数据的完整性。
    //由于是向下搜索，所以，上层数据是否变化，不关注。
    while ( TRUE )
    {
        ret = slice_read (db, address, slice, READ_LOCK, TRUE);

        if ( MYDB_SYS_ERR == ret ) goto FATAL_ERROR;
        if ( MYDB_LOCKED == ret ) goto SLICE_LOCKED;

        //检查是否有脏数据
        if ( slice[SLICE_DIRTY] )
        {
            my_printf ( "Dirty data [0x%lx]!\n", address );

            //以只读方式打开，无法处理。
            if ( db->read_only )
                goto FATAL_ERROR;

            //回滚脏日志。
            if ( MYDB_OK != fallback_log(db) )
                goto FATAL_ERROR;

            //释放锁资源
            my_unlock( db->fd_data, address );
            
            //再一次尝试。
            continue;
        }

        //释放上一层锁，执有当前slice锁。
        if ( lock_addr )
        {
            //这个时候，是持有两个锁的。
            if ( MYDB_OK != unlock( db->fd_data, lock_addr, LOCK_SIZE ) )
            {
                unlock( db->fd_data, address, LOCK_SIZE );
                return MYDB_SYS_ERR;
            }
        }
        
        //置锁。
        lock_addr = address;
        
        if ( MYDB_OK != slice_bisection_find ( slice, key, &pos ) ) 
        {
            my_unlock(db->fd_data, lock_addr);
            if ( slice[SLICE_TYPE] & TYPE_INDEX )
                return MYDB_SYS_ERR;
            else
                return MYDB_LGC_ERR;     //没有找到。NO FOUND
        }
        
        
        //取到下一级索引的地址。
        address = ITEM_OFFSET(ITEM_AT(ITEMS(slice),pos));
        
        if ( slice[SLICE_TYPE] & TYPE_INDEX )
            continue;
        
        //已找到。
        *data_addr = address;
        //不能释放锁。
        //因为，slice不被锁定，那么，数据就有可能在被读之前，被删除被修改。
        //my_unlock(db->fd_data, lock_addr);    
        *lockup = lock_addr;    //返回锁。
        return MYDB_OK;
    }

SLICE_LOCKED:
    //释放锁资源。
    if ( lock_addr );
        unlock( db->fd_data, lock_addr, LOCK_SIZE );
    return MYDB_LOCKED;

FATAL_ERROR:
    if ( lock_addr );
        unlock( db->fd_data, lock_addr, LOCK_SIZE );
    catch_processing;
}

MyDB_Ret get( MyDB* db, mydb_key key, void* data, int* len )
{
    mydb_uint data_addr=0, lockup=0;
    MyDB_Ret ret;

    while ( TRUE )
    {
        ret = read_search ( db, key, &data_addr, &lockup );
        //锁失败，释放锁，重新读。
        if ( MYDB_LOCKED == ret )
        {
            usleep ( 1 );
            continue;
        }
        
        if ( MYDB_OK != ret ) 
            return ret;

        ret = read_single_data( db,data_addr, data, len );
        my_unlock( db->fd_data, lockup );
        return ret;
    }

FATAL_ERROR:
    catch_processing;
}


//current_slice: 如果current_slice值不为0，则是搜索current_slice的父节点。
//返回搜索到的slice 的地址。
//本函数在返回值不为MYDB_SYS_ERR的情况下，
//会执有slice_address的写锁。
MyDB_Ret write_search( MyDB* db, mydb_key key, mydb_uint current_slice, Slice* slice, mydb_uint* slice_address )
{
    MyDB_Ret ret;
    mydb_uint lock_addr = 0;    //锁位置
    int pos = 0;
    
    mydb_uint address = GLOBAL_HEAD_SIZE;

    //执有当前读的slice的锁，使当前slice不会被改变。
    //这样，就保证了当前内存中的数据的完整性。
    //也保证了子树数据的完整性。
    //由于是向下搜索，所以，上层数据是否变化，不关注。
    while ( TRUE )
    {
        ret = slice_read (db, address, slice, WRITE_LOCK, TRUE);

        if ( MYDB_SYS_ERR == ret ) goto FATAL_ERROR;
        if ( MYDB_LOCKED == ret ) goto SLICE_LOCKED;

        //检查是否有脏数据
        if ( slice[SLICE_DIRTY] )
        {
            my_printf ( "Dirty data [0x%lx]!\n", address );

            //回滚脏日志。
            if ( MYDB_OK != fallback_log(db) )
                goto FATAL_ERROR;
            
            //释放锁资源
            my_unlock( db->fd_data, address );
            
            //再一次尝试。
            continue;
        }
        
        //释放上一层锁，执有当前slice锁。
        if ( lock_addr )
        {
            //这个时候，是持有两个锁的。
            if ( MYDB_OK != unlock( db->fd_data, lock_addr, LOCK_SIZE ) )
            {
                unlock( db->fd_data, address, LOCK_SIZE );
                return MYDB_SYS_ERR;
            }
        }
        
        //置锁。
        lock_addr = address;
        
        if ( MYDB_OK != slice_bisection_find ( slice, key, &pos ) ) 
        {   
            if ( slice[SLICE_TYPE] & TYPE_INDEX )
            {
                unlock(db->fd_data, lock_addr, LOCK_SIZE);
                return MYDB_SYS_ERR;
            }
            else
            {
                *slice_address = address;
                return MYDB_LGC_ERR;     //没有找到。NO FOUND
            }
        }
        
        
        //取到下一级索引的地址。
        address = ITEM_OFFSET(ITEM_AT(ITEMS(slice),pos));

        if ( address == current_slice 
            || slice[SLICE_TYPE] & TYPE_LEAF )
        {
            *slice_address = lock_addr;
            return MYDB_OK;
        }
    }

SLICE_LOCKED:
    //释放锁资源。
    if ( lock_addr )
        unlock( db->fd_data, lock_addr, LOCK_SIZE );
    return MYDB_LOCKED;

FATAL_ERROR:
    if ( lock_addr )
        unlock( db->fd_data, lock_addr, LOCK_SIZE );
    catch_processing;
}

MyDB_Ret insert ( Slice* slice, Item* item )
{
    int pos;
    Slice new_slice[SLICE_ITEM_LEN];

    if ( MYDB_SYS_ERR == slice_bisection_find ( slice, item, &pos ) )
        return MYDB_SYS_ERR;

    if ( ((int)(slice[SLICE_LENGTH]))-1 == pos )
    {
        memcpy ( ITEMS(slice)+slice[SLICE_LENGTH]*ITEM_LEN, item, ITEM_SIZE );
    }
    else if ( -1 == pos )
    {
        memcpy ( new_slice, ITEMS(slice), slice[SLICE_LENGTH]*ITEM_SIZE );
        memcpy ( ITEMS(slice), item, ITEM_SIZE);
        memcpy ( ITEMS(slice)+ITEM_LEN, new_slice, slice[SLICE_LENGTH]*ITEM_SIZE );
    }
    else
    {
        memcpy ( new_slice, ITEMS(slice)+(pos+1)*ITEM_LEN, (slice[SLICE_LENGTH]-pos-1)*ITEM_SIZE );
        memcpy ( ITEMS(slice)+(pos+1)*ITEM_LEN, item, ITEM_SIZE );
        memcpy ( ITEMS(slice)+(pos+2)*ITEM_LEN, new_slice, (slice[SLICE_LENGTH]-pos-1)*ITEM_SIZE );
    }

    ++slice[SLICE_LENGTH];
    return MYDB_OK;
}

MyDB_Ret divide ( MyDB* db, Slice* slice, Item* item, Slice* update_slice )
{
    int macro_i=0;
    MyDB_Ret ret;
    mydb_uint address;
    
    int v = 0;
    Slice new_slice[SLICE_ITEM_LEN];

    memcpy ( update_slice, slice, SLICE_SIZE );
    
    new_slice[SLICE_DIRTY] = FALSE;
    new_slice[SLICE_TYPE] = slice[SLICE_TYPE];
    new_slice[SLICE_LENGTH] = slice[SLICE_LENGTH]/2;
    memcpy ( ITEMS(new_slice), ITEMS(slice)+(slice[SLICE_LENGTH]-slice[SLICE_LENGTH]/2)*ITEM_LEN, 
        (slice[SLICE_LENGTH]/2)*ITEM_SIZE );

    
    update_slice[SLICE_LENGTH] -= slice[SLICE_LENGTH]/2;
    
    compare(item,ITEM_AT(ITEMS(new_slice),0),v);
    if ( GT == v )
        ret = insert ( new_slice, item );
    else
        ret = insert ( update_slice, item );

    if ( MYDB_OK != ret )
    {
        return ret;
    }

    ret = new_slice_write( db,new_slice,SLICE_SIZE, &address );
    if ( MYDB_OK != ret )
    {
        return ret;
    }

    //show_slice(slice);
    //show_slice(update_slice);
    //show_slice(new_slice);

    memcpy ( item, ITEMS(new_slice), g_keylen*UINT_SIZE );
    item[g_keylen] = address;

    return MYDB_OK;
}

MyDB_Ret top_divide ( MyDB* db, Slice* slice, Item* item, Slice* update_slice )
{
    int macro_i=0;
    MyDB_Ret ret;
    mydb_uint addr1, addr2;
    
    int v = 0;
    Slice slice1[SLICE_ITEM_LEN];
    Slice slice2[SLICE_ITEM_LEN];
    
    if ( !slice1 || !slice2 || !update_slice ) 
        return MYDB_SYS_ERR;

    slice1[SLICE_DIRTY] = FALSE;
    slice1[SLICE_TYPE] = slice[SLICE_TYPE];
    slice1[SLICE_LENGTH] = slice[SLICE_LENGTH]/2;
    memcpy ( ITEMS(slice1), ITEMS(slice), slice1[SLICE_LENGTH]*ITEM_SIZE );

    slice2[SLICE_DIRTY] = FALSE;
    slice2[SLICE_TYPE] = slice[SLICE_TYPE];
    slice2[SLICE_LENGTH] = slice[SLICE_LENGTH] - slice1[SLICE_LENGTH];
    memcpy ( ITEMS(slice2), ITEMS(slice)+slice1[SLICE_LENGTH]*ITEM_LEN, slice2[SLICE_LENGTH]*ITEM_SIZE );
    
    compare(item,ITEM_AT(ITEMS(slice2),0),v);
    if ( GT == v )
        ret = insert ( slice2, item );
    else
        ret = insert ( slice1, item );

    if ( MYDB_OK != ret )
    {
        return ret;
    }

    ret = new_slice_write( db,slice1,SLICE_SIZE, &addr1 );
    if ( MYDB_OK != ret )
    {
        return ret;
    }
    else
    {
        ret = new_slice_write( db,slice2,SLICE_SIZE, &addr2 );
        if ( MYDB_OK != ret )
        {
            return ret;
        }
    }
    
    update_slice[SLICE_DIRTY] = FALSE;
    update_slice[SLICE_TYPE] = slice[SLICE_TYPE];
    update_slice[SLICE_LENGTH] = 2;
    memcpy ( ITEMS(update_slice), ITEMS(slice1), g_keylen*UINT_SIZE);
    memcpy ( ITEM_AT(ITEMS(update_slice),1), ITEMS(slice2), g_keylen*UINT_SIZE);

    ITEMS(update_slice)[g_keylen] = addr1;
    ITEMS(update_slice)[ITEM_LEN+g_keylen] = addr2;

    //show_slice ( slice1, 0 );
    //show_slice ( slice2, 0 );
    //show_slice ( update_slice, 0 );
    
    return MYDB_OK;
}


MyDB_Ret put ( MyDB* db, mydb_key key, const void* data, int len )
{
    struct Log* plog = NULL;
    
    mydb_uint current_slice = 0;
    mydb_uint slice_address = 0;
    MyDB_Ret ret;
    mydb_uint address;
    
    Slice slice[SLICE_ITEM_LEN];
    Slice update_slice[SLICE_ITEM_LEN];
    Item item[ITEM_LEN];

    allways_search ( db, key, current_slice, slice, &slice_address, ret );

    if ( MYDB_OK == ret && (slice[SLICE_TYPE] & TYPE_LEAF) )
    {
        //键值已存在。
        my_unlock( db->fd_data, slice_address );    //释放锁。
        return MYDB_LGC_ERR;
    }

    //先将数据写入。
    ret = new_data_write( db, data, len, &address );
    if ( MYDB_SYS_ERR == ret )
        goto FATAL_ERROR;

    memcpy ( item, key, g_keylen*UINT_SIZE );
    item[g_keylen] = address;
    
    while ( TRUE )
    {   
        //释放底层锁。
        if ( current_slice )
        {
            my_unlock( db->fd_data,current_slice );
            current_slice = 0;
        }
        
        if ( (int)slice[SLICE_LENGTH] < g_rangesize )
        {
            //不需要分裂。
            memcpy ( update_slice, slice, SLICE_SIZE );
            if ( MYDB_OK != insert( update_slice,item ) )
                goto FATAL_ERROR;

            plog = append_log( plog, slice_address, update_slice, slice, SLICE_SIZE );
            break;
        }

        if ( GLOBAL_HEAD_SIZE == slice_address )
            ret = top_divide( db, slice, item, update_slice );
        else
            ret = divide( db, slice, item, update_slice );

        if ( MYDB_OK != ret )
            goto FATAL_ERROR;

        plog = append_log( plog, slice_address, update_slice, slice, SLICE_SIZE );
        
        //持有上层锁。
        current_slice = slice_address;
        slice_address = 0;
        
        if ( GLOBAL_HEAD_SIZE == current_slice )
            break;
        
        allways_search ( db, key, current_slice, slice, &slice_address, ret );
    }

    //写日志。
    if ( MYDB_OK != do_log ( db, plog ) )
        goto FATAL_ERROR;

    //释放资源。    
    if ( current_slice )
            my_unlock( db->fd_data, current_slice );

    if ( slice_address )
            my_unlock( db->fd_data, slice_address );
    
    free_log( plog );
    plog = NULL;
    return MYDB_OK;
    
FATAL_ERROR:
    free_log( plog );
    plog = NULL;
    
    if ( current_slice )
            unlock( db->fd_data, current_slice, LOCK_SIZE );

    if ( slice_address )
            unlock( db->fd_data, slice_address, LOCK_SIZE );
            
    catch_processing;        
}

MyDB_Ret db_close ( MyDB** db )
{
    MyDB* pdb = *db;
    
    if ( !pdb )
    {
        my_printf ( "[db_colse] Error Parameters!\n" );
        return MYDB_LGC_ERR;
    }
    
    my_printf ( "Now close db [%s]......\n", pdb->db_file );
    if ( -1 != pdb->fd_data ) close ( pdb->fd_data );
    if ( -1 != pdb->fd_log ) close ( pdb->fd_log );
    pdb->fd_data = -1;
    pdb->fd_log = -1;
    free (pdb->db_file);
    free ( pdb );

    *db = NULL;
    
    return MYDB_OK;
}

MyDB_Ret init ( MyDB* db )
{
    mydb_uint log_head[LOG_HEAD_LEN];
    
    int init_size = GLOBAL_HEAD_SIZE+2*SLICE_SIZE;
    
    //初始化数据文件。
    mydb_uint* index = NULL;
    mydb_uint* leaf = NULL;
    mydb_uint* buf = (mydb_uint*)malloc(init_size);

    my_printf ( "init db......\n" );
    
    memset ( buf, 0, init_size );
    index = buf + GLOBAL_HEAD_LEN;
    leaf = index + SLICE_HEAD_LEN + g_rangesize*ITEM_LEN;

    buf[0] = g_rangesize;
    buf[1] = g_keylen;
    
    index[SLICE_DIRTY] = FALSE;
    index[SLICE_TYPE] = TYPE_INDEX;
    index[SLICE_LENGTH] = 1;
    index[SLICE_HEAD_LEN+g_keylen] = (leaf-buf)*UINT_SIZE;

    leaf[SLICE_DIRTY] = FALSE;
    leaf[SLICE_TYPE] = TYPE_LEAF;
    leaf[SLICE_LENGTH] = 0;

    pos_write ( db->fd_data, 0, buf, init_size );

    //初始化日志文件。
    memset ( log_head, 0, LOG_HEAD_LEN*UINT_SIZE );
    log_head[LOG_FLAG_DIRTY] = LOG_STATUS_FINISHED;
    log_head[LOG_FLAG_LENGTH] = 0;
    
    pos_write ( db->fd_log, 0, log_head, LOG_HEAD_LEN*UINT_SIZE );

    free (buf);
    return MYDB_OK;

FATAL_ERROR:
    free ( buf );
    catch_processing;            
}

MyDB_Ret db_open ( MyDB** db, const char* db_file, int range_size, int key_len, int read_only )
{
    MyDB* pdb = NULL;
    mydb_uint tmp_range = 0;
    mydb_uint tmp_keylen = 0;
    int flags = 0;
    mydb_uint offset = 0;
    int lock_type = WRITE_LOCK;
    int locked = FALSE;

    *db = (MyDB*)malloc(sizeof(MyDB));
    pdb = *db;
    
    if ( !pdb || !db_file || 5 > range_size )
    {
        my_printf ( "[db_open] Error Parameters!\n" );
        return MYDB_LGC_ERR;
    }
    
    char* log_file = (char*)malloc ( strlen(db_file)+16 );

    pdb->fd_data = -1;
    pdb->fd_log = -1;
    
    pdb->read_only = read_only;
    pdb->range_size = range_size;
    pdb->key_len = key_len;

    g_rangesize = pdb->range_size;
    g_keylen = pdb->key_len;
    
    pdb->db_file = (char*)malloc( strlen(db_file)+1 );
    strcpy ( pdb->db_file, db_file );
    strcpy ( log_file, db_file );
    strcat ( log_file, ".log.db" );
    
    //打开数据库和日志文件句柄。
    if ( read_only )
    {
        flags = O_LARGEFILE|O_RDONLY;
        pdb->fd_data = open ( pdb->db_file, flags, S_IWUSR );
        pdb->fd_log = open ( log_file, flags, S_IWUSR );
    }
    else
    {
        flags = O_LARGEFILE|O_CREAT|O_RDWR;
        //pdb->fd_data = open ( db_file, flags, S_IRWXU|S_IRWXG );
        pdb->fd_data = open ( db_file, flags, S_IRWXU|S_IRWXG|S_IRWXO );
        //if ( SAFE_MODE == pdb->mode ) flags = flags | O_SYNC;
        //pdb->fd_log = open ( log_file, flags, S_IRWXU|S_IRWXG );
        pdb->fd_log = open ( log_file, flags, S_IRWXU|S_IRWXG|S_IRWXO );
    }
        
    if ( -1 == pdb->fd_data || -1 == pdb->fd_log )
    {
        my_printf ( "Open db file [%s][%d] [%s][%d] failed!\n", db_file, pdb->fd_data, log_file, pdb->fd_log );
        goto FATAL_ERROR;
    }

    //锁定文件，避免竞争初始化。
    if ( pdb->read_only ) 
        lock_type = READ_LOCK;
    my_lock( pdb->fd_log, lock_type, 0, FALSE );

    locked = TRUE;
    
    //判断系统是否已初始化。
    offset = lseek ( pdb->fd_log, 0, SEEK_END );
    if ( LOG_HEAD_LEN*UINT_SIZE > offset )
    {
        //未初始化，初始化它。
        if ( pdb->read_only )
        {
            my_printf ( "The db no inited!\n" );
            goto FATAL_ERROR;
        }

        if ( MYDB_OK != init ( pdb ) )
            goto FATAL_ERROR;

        chmod ( pdb->db_file, 0777 );
        chmod ( log_file, 0777 );
    }
    else
    {
        //使用db保存的数据。
        pos_read ( pdb->fd_data, 0, &tmp_range, UINT_SIZE );
        pos_read ( pdb->fd_data, UINT_SIZE, &tmp_keylen, UINT_SIZE );

        g_rangesize = tmp_range;
        g_keylen = tmp_keylen;
        pdb->range_size = g_rangesize;
        pdb->key_len = g_keylen;
        
        //检查脏日志。
        if ( !pdb->read_only && MYDB_OK != fallback_log( pdb ) )
            goto FATAL_ERROR;
    }
    
    //解锁文件。
    my_unlock( pdb->fd_log, 0 );
    
    free ( log_file );
    return MYDB_OK;

FATAL_ERROR:
    free ( log_file );
    if ( locked ) 
        unlock( pdb->fd_log, 0, LOCK_SIZE );
    
    db_close ( db );
    
    catch_processing;            
}


/*

///////////////////////////// TEST ////////////////////////////////

int test_comapre ()
{
    int macro_i=0;
    int v = 0;
    mydb_uint k1[3];
    mydb_uint k2[3];

    k1[0] = 10;
    k1[1] = 10;
    k1[2] = 10;
    
    k2[0] = 10;
    k2[1] = 10;
    k2[2] = 10;
    
    g_keylen = 3;

    compare ( k1, k2, v );
    printf ( "----[%d]\n", v );
    return 0;
}

int test_open()
{
    MyDB* db = (MyDB*)malloc(sizeof(MyDB));
    MyDB_Ret ret = db_open( db, "./test.db", 8, 1, 0 );

    printf ( "-----[%d]\n", ret );
    return 0;
}

int test_bisection()
{
    g_rangesize = 8;
    g_keylen = 1;
    Slice* slice = (Slice*)malloc ( SLICE_SIZE );
    slice[SLICE_TYPE] = TYPE_LEAF;
    slice[SLICE_LENGTH] = 2;

    Item* items = ITEMS(slice);
    items[0] = 10;
    items[2] = 88888888;
    items[4] = 100;

    mydb_uint key[1];
    int pos = 0;

    key[0] = 100;
    
    MyDB_Ret ret = slice_bisection_find ( slice, key, &pos );

    printf ( "-------------RET: [%d][%d]\n", ret, pos );
    return 0;
}

int test_divide()
{
    g_rangesize = 5;
    g_keylen = 1;

    Slice* slice = (Slice*)malloc ( SLICE_SIZE );
    Slice* update_slice = (Slice*)malloc ( SLICE_SIZE );
    
    slice[SLICE_TYPE] = TYPE_LEAF;
    slice[SLICE_LENGTH] = 5;

    Item* items = ITEMS(slice);
    items[0] = 10;
    items[2] = 15;
    items[4] = 100;
    items[6] = 200;
    items[8] = 300;

    Item item[2];
    item[0] = 5;
    MyDB_Ret ret = divide( NULL, slice, item, update_slice );

    printf ( "----------[%d]\n", ret );
    return 0;
}

int test_put( mydb_uint key, mydb_uint data )
{
    MyDB* db = (MyDB*)malloc(sizeof(MyDB));
    MyDB_Ret ret = db_open( db, "./test.db", 8, 1, FALSE );
    
    if ( MYDB_OK != ret )
        return ret;

    //mydb_uint key = 88888888;
    //mydb_uint data = 1000000;
    ret = put( db, &key, &data, UINT_SIZE );

    printf ( "----------[%d]\n", ret );
    return 0;
}

int test_get( mydb_uint key )
{
    MyDB* db = (MyDB*)malloc(sizeof(MyDB));
    MyDB_Ret ret = db_open( db, "./test.db", 8, 1, FALSE );
    
    if ( MYDB_OK != ret )
        return ret;

    //dbg_index( db, 0, 0 );
    //return 0;
    
    //mydb_uint key = 88888888;
    mydb_uint data = 1000000;
    int len = 8;
    ret = get( db, &key, &data, &len );

    printf ( "----------[%d][%lu][%d]\n", ret, data, len );
    return ret;
}

int test_db_divide ()
{
    int i = 0;
    for ( i = 0; i < 37; ++i )
    {
        test_put ( i*100, i*100 );
    }

    printf ( "=====================\n" );

    MyDB* db = (MyDB*)malloc(sizeof(MyDB));
    MyDB_Ret ret = db_open( db, "./test.db", 8, 1, TRUE );
    
    if ( MYDB_OK != ret )
        return ret;
    
    dbg_index( db, GLOBAL_HEAD_SIZE, 0 );


    return 0;
}


//增序测试
int test_asc ( int low, int high )
{
    mydb_uint i, data;
    int size = 8;
    MyDB* db = (MyDB*)malloc(sizeof(MyDB));
    MyDB_Ret ret = db_open( db, "./test.db", 8, 1, FALSE );
    
    if ( MYDB_OK != ret )
        return ret;


restart();

    for ( i = low; i < high; ++i )
    {
        if ( 0 == i%10000 )
            printf ( "---------WRITE [%lu]\n", i );

        data = i;
        size = 8;
        if ( MYDB_SYS_ERR == put ( db, &i, &data, size ) )
        {
            printf ( "ERROR WRITE DATA [%lu]\n", i );
            return -1;
        }
    }
printf ( "*******[%f][%d]\n", elapsed(),g_locknum );    

    //dbg_index( db, GLOBAL_HEAD_SIZE, 0 );
return 0;


restart();

    for ( i = low; i < high; ++i )
    {
        if ( 0 == i%10000 )
            printf ( "---------READ [%lu]\n", i );

        size = 8;
        if ( MYDB_OK != get ( db, &i, &data, &size ) )
        {
            printf ( "---------- ERROR READ DATA[%lu][%lu]\n", i, data );
            return -1;
        }

        if ( i != data )
        {
            printf ( "---------- ERROR READ DATA[%lu][%lu]\n", i, data );
        }
    }

printf ( "*******READ[%f]\n", elapsed() );    
    
    printf ( "****** OK [%d]!\n", g_locknum );
    
    return 0;
}


int test_random ( int t, int v )
{
    mydb_uint key[2];
    mydb_uint i, data;
    int size = 8;
    MyDB* db = (MyDB*)malloc(sizeof(MyDB));
    MyDB_Ret ret = db_open( db, "./test.db", 500, 2, FALSE );
    
    if ( MYDB_OK != ret )
        return ret;


restart();
    for ( i = 20000; i > 0; --i )
    {
        if ( v != i%t ) continue;

        data = i;
        size = 8;
        key[0] = i;
        key[1] = i;
        if ( MYDB_SYS_ERR == put ( db, key, &data, size ) )
        {
            printf ( "ERROR WRITE DATA [%lu]\n", i );
            return -1;
        }
    }
printf ( "*******[%f][%d]\n", elapsed(),g_locknum );    

    //dbg_index( db, GLOBAL_HEAD_SIZE, 0 );
return 0;


restart();

    for ( i = 1; i < 2000000; ++i )
    {
        if ( 0 == i%10000 )
            printf ( "---------READ [%lu]\n", i );

        size = 8;
        key[0] = i;
        key[1] = i;
        if ( MYDB_OK != get ( db, key, &data, &size ) )
        {
            printf ( "---------- ERROR READ DATA[%lu][%lu]\n", i, data );
            return -1;
        }

        if ( i != data )
        {
            printf ( "---------- ERROR READ DATA[%lu][%lu]\n", i, data );
        }
    }

printf ( "*******READ[%f]\n", elapsed() );    
    
    printf ( "****** OK [%d]!\n", g_locknum );
    
    return 0;
}

int main ( int argc, char** argv )
{    
    return test_random ( atoi(argv[1]), atoi(argv[2]) );
    //return test_asc ( atol(argv[1]), atol(argv[2]) );
    
    if ( 3 == argc )
        return test_put( atol(argv[1]), atol(argv[2]) );
    else
        return test_get( atol(argv[1]) );
    
    return 0;
}

*/



