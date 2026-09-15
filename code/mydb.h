#ifndef __MYDB_H__
#define __MYDB_H__

#include <sys/types.h>


//定义长整型。mydb的key是基于长整形的。
//长整形的优势在于:常用的整型key，可以直接使用。
//md5值，可直接转换为两个64位长整型。
//字符串型可直接转为长整形指针。

//兼容64位机器和32位机器
#  if __WORDSIZE == 64
typedef long unsigned mydb_uint;
#  elif __GLIBC_HAVE_LONG_LONG
__extension__ typedef long long unsigned mydb_uint;
#  else
typedef unsigned mydb_uint;
#  endif



//MyDB键值类型。
//是一个长整形的指针。
//MyDB的键值是定长的，且其长度是sizeof(mydb_uint)的整数倍。
//键值长度在打开数据库的时候指定，且以后不可变更。
typedef mydb_uint* mydb_key;



//MyDB接口函数返回值。
typedef enum
{
    MYDB_OK = 0,                //返回成功
    MYDB_SYS_ERR = -1,     //io级或系统级的错。
    MYDB_LGC_ERR = -2,      //正常的逻辑错误。
    MYDB_LOCKED = -3        //记录被锁。
}MyDB_Ret;

struct stMyDB;

typedef struct stMyDB MyDB;

MyDB_Ret db_open ( MyDB** db, const char* db_file, int range_size, int key_len, int read_only );

MyDB_Ret db_close ( MyDB** db );

MyDB_Ret get( MyDB* db, mydb_key key, void* data, int* len );

MyDB_Ret put ( MyDB* db, mydb_key key, const void* data, int len );


#endif //__MYDB_H__

