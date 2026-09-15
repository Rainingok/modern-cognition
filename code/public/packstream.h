#ifndef __PACK_STREAM_H__
#define __PACK_STREAM_H__

#include <assert.h>
#include <string.h>

#include <string>
#include <set>
#include <vector>
#include <map>
#include <list>
#include <iostream>

#include <stdexcept>

#define MY_ASSERT(len) {assert(m_buf);\
	if(m_offset+(len)+sizeof(unsigned)>=m_bufSize){\
	m_bufSize=2*m_bufSize+(len);\
	char* macro_char_pointer=new char[m_bufSize];\
	memcpy(macro_char_pointer,m_buf,m_offset);\
	delete[]m_buf;\
	m_buf=macro_char_pointer;}}

#define PACK_LENGTH(len) {MY_ASSERT ( len+sizeof(unsigned) ); \
memcpy ( m_buf+m_offset, &len, sizeof(unsigned) ); \
m_offset += sizeof(unsigned);}

#define MAX_PACK_LEN 5120        //一个包的最大长度。

//打包管理空间
//但解包不管理空间。
class PackStream
{
    protected:
        char* m_buf;
        unsigned m_offset;
        unsigned m_len;
        unsigned m_bufSize;

    public:
        PackStream( unsigned bufLen = MAX_PACK_LEN )        //打包构造
            { 
                m_bufSize = bufLen;
                
                m_len = 0;
                m_offset = 0; 
                m_buf = new char[m_bufSize]; 
            };

        //解包构造
        //buf由外部释放。
        //在处理该包的时间内，buf必须有效。
        PackStream( const char* buf, unsigned len )
            {
                m_bufSize = 0;
                m_len = len;
                m_buf = (char*)buf;
                m_offset = 0;
            };

        //如果是打包，则释放空间。
        ~PackStream()
            {
                if ( !m_len && m_buf )
                    {
                    delete [] m_buf;
                    }
            };

        inline const char* buf( unsigned& len ) const 
            { 
                len = m_offset;
                return m_buf; 
            };

        inline const char* data() const 
            { 
                return m_buf; 
            };

        inline unsigned len( ) const 
            { 
                return m_offset;
            };
        
        inline void clear()
            {
                m_offset = 0;
            };

        inline bool eof() const 
            {
            return m_offset >= m_len;
            };
		
		//
        //枚举简单数据结构
		inline PackStream& operator <<  ( bool v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( char v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( unsigned char v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( short v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( unsigned short v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( int v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( unsigned v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( float v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( double v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( long double v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( long v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( unsigned long v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( long long v ) { return packBaseType(v); };
		inline PackStream& operator <<  ( unsigned long long v ) { return packBaseType(v); };
		
		inline PackStream& operator << ( const char* v)
            {
                unsigned len = 0;
                if ( v ) len = strlen(v);
				PACK_LENGTH (len);
                if ( v ) memcpy ( m_buf+m_offset, v, len );
                m_offset += len;
                return *this;
            };
		
		inline PackStream& operator << ( const std::pair<const char*, int>& v)
			{
				return packBinary ( v );
			};
		inline PackStream& operator << ( const std::pair<const char*, unsigned>& v)
			{
				return packBinary ( v );
			};
		inline PackStream& operator << ( const std::pair<const char*, size_t>& v)
			{
				return packBinary ( v );
			};
		template <class K, class V>
        PackStream& operator << ( const std::pair<K,V>& v)
			{
				(*this) << v.first << v.second;
				return *this;
			};
		
        template <class T>
        PackStream& operator << ( const std::vector<T>& v)
            {
                unsigned len = v.size();
				PACK_LENGTH (len);
                for ( unsigned i = 0; i < len; ++i )
                    (*this) << v[i];
                return *this;
            };

        template <class T>
        PackStream& operator << ( const std::set<T>& v)
            {
                unsigned len = v.size();
                PACK_LENGTH (len);
                typename std::set<T>::const_iterator end = v.end();
                for ( typename std::set<T>::const_iterator it = v.begin(); it != end; ++it )
                    (*this) << *it;
                return *this;
            };
		
		template <class T>
        PackStream& operator << ( const std::list<T>& v )
            {
                unsigned len = v.size();
                PACK_LENGTH (len);
                typename std::list<T>::const_iterator end = v.end();
                for ( typename std::list<T>::const_iterator it = v.begin(); it != end; ++it )
                    (*this) << *it;
                return *this;
            };
			
        template <class K, class V>
        PackStream& operator << ( const std::map<K,V>& v)
            {
                unsigned len = v.size();
                PACK_LENGTH (len);
                typename std::map<K,V>::const_iterator end = v.end();
                for ( typename std::map<K,V>::const_iterator it = v.begin(); it != end; ++it )
                    (*this) << *it;
                return *this;
            };

        template <class K, class V>
        PackStream& operator << ( const std::multimap<K,V>& v)
            {
                unsigned len = v.size();
                PACK_LENGTH (len);
                typename std::multimap<K,V>::const_iterator end = v.end();
                for ( typename std::multimap<K,V>::const_iterator it = v.begin(); it != end; ++it )
                    (*this) << *it;
                return *this;
            };
        
        PackStream& operator << ( const std::string& v)
            {
                unsigned len = v.size();
                PACK_LENGTH (len);
                memcpy ( m_buf+m_offset, v.data(), len );
                m_offset += len;
                return *this;
            };
		
		
		///////////////////////////////////////////////////////
		//解包函数
		///////////////////////////////////////////////////////
		inline PackStream& operator >>  ( bool& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( char& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( unsigned char& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( short& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( unsigned short& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( int& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( unsigned& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( float& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( double& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( long double& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( long& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( unsigned long& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( long long& v ) { return unpackBaseType(v); };
		inline PackStream& operator >>  ( unsigned long long& v ) { return unpackBaseType(v); };

        template <class T>
        PackStream& operator >> ( std::vector<T>& v )
            {  
                v.clear();
                unsigned len = ((unsigned*)(m_buf+m_offset))[0];
                //memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );
                m_offset += sizeof(unsigned);

                v.reserve ( len );
                
                T t;
                for ( unsigned i = 0; i < len; ++i )
                {
                    (*this) >> t;
                    v.push_back ( t );
                }

                return *this;
            };
		
		template <class T>
        PackStream& operator >> ( std::list<T>& v )
            {  
                v.clear();
                unsigned len = ((unsigned*)(m_buf+m_offset))[0];
                //memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );
                m_offset += sizeof(unsigned);
                
                T t;
                for ( unsigned i = 0; i < len; ++i )
                {
                    (*this) >> t;
                    v.push_back ( t );
                }

                return *this;
            };
			
        template <class T>
        PackStream& operator >> ( std::set<T>& v )
            {                
                v.clear();
                unsigned len = ((unsigned*)(m_buf+m_offset))[0];
                //memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );
                m_offset += sizeof(unsigned);

                T t;
                for ( unsigned i = 0; i < len; ++i )
                {
                    (*this) >> t;
                    v.insert ( t );
                }
                return *this;
            };

        template <class K, class V>
        PackStream& operator >> ( std::map<K,V>& v )
            {
                v.clear();
                unsigned len = ((unsigned*)(m_buf+m_offset))[0];
                //memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );
                m_offset += sizeof(unsigned);

                std::pair<K,V> pa;
                for ( unsigned i = 0; i < len; ++i )
                {
                    (*this) >> pa;
                    v.insert ( pa );
                }
                return *this;
            };

        template <class K, class V>
        PackStream& operator >> ( std::multimap<K,V>& v )
            {
                v.clear();
                //unsigned len = 0;
                unsigned len = ((unsigned*)(m_buf+m_offset))[0];
                //memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );
                m_offset += sizeof(unsigned);

                std::pair<K,V> pa;
                for ( unsigned i = 0; i < len; ++i )
                {
                    (*this) >> pa;
                    v.insert ( pa );
                }

                return *this;
            };
        
		inline PackStream& operator >> ( std::pair<const char*, int>& v)
			{
				return unpackBinary ( v );
			};
		inline PackStream& operator >> ( std::pair<const char*, unsigned>& v)
			{
				return unpackBinary ( v );
			};
		inline PackStream& operator >> ( std::pair<const char*, size_t>& v)
			{
				return unpackBinary ( v );
			};
		template <class K, class V>
		inline PackStream& operator >> ( std::pair<K,V>& v )
			{
				(*this) >> v.first >> v.second;
				return *this;
			};
		
        inline PackStream& operator >> ( char* v );
        
        PackStream& operator >> ( std::string& v );

    protected:
		//解包二进制内容。
		template <class T>
        inline PackStream& unpackBinary ( std::pair<const char*,T>& v )
            {
                //得到长度。
                (*this) >> v.second;
                assert ( m_offset+v.second <= m_len );
                
                v.first = m_buf+m_offset;
                m_offset += v.second;

                return *this;
            };
			
		//打包二进制数据
		template <class T>
        inline PackStream& packBinary ( const std::pair<const char*, T>& v)
            {
                //assert ( v.first );       //应该允许空指针
                //assert ( m_buf && m_offset+sizeof(unsigned) + v.second  < m_bufSize );
				MY_ASSERT ( sizeof(unsigned) + v.second );
        
                memcpy ( m_buf+m_offset, &(v.second), sizeof(unsigned) );
                m_offset += sizeof(unsigned);
                if ( 0 < v.second )     //有可能是空指针
                {
                    memcpy ( m_buf+m_offset, v.first, v.second );
                    m_offset += v.second;
                }

                return *this;
            };
			
        //不能采用这种定义。
		//对未能识别的数据结构，比如：struct，默认会不检查地走到这里来，
		//这是不正确的，而且不返回错误。
		//template <class T>
        //inline PackStream& operator <<  ( const T& v )
        template <class T>
        inline PackStream& packBaseType ( const T& v )
            {
				MY_ASSERT ( sizeof(T) );
                
                //memcpy ( m_buf+m_offset, &v, sizeof(T) );
                T* p = (T*)(m_buf+m_offset);
                *p = v;
                m_offset += sizeof(T); 

                return *this;
            };
			
		template <class T>
        inline PackStream& unpackBaseType ( T& v )
            {
                assert ( m_buf && m_offset+sizeof(T) <= m_len );
                
                //memcpy ( &v, m_buf+m_offset, sizeof(T) );
                v = *((T*)(m_buf+m_offset));
                m_offset += sizeof(T);

                return *this;
            };
			
        void setUnpackData ( const char* data, unsigned len )
            {
                m_bufSize = 0;
                m_len = len;
                m_buf = (char*)data;
                m_offset = 0;
            };

};

#endif //__PACK_STREAM_H__

