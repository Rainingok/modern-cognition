#include "packstream.h"

PackStream& PackStream::operator >> ( char* v )
{
    unsigned len = 0;
    memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );

    assert ( v && m_buf && m_offset+len <= m_len );
    
    m_offset += sizeof(unsigned);
    memcpy ( v, m_buf+m_offset, len );
    v[len] = 0;
    m_offset += len;

    return *this;
}

PackStream& PackStream::operator >> ( std::string& v )
{    
    v.clear();

    unsigned len = 0;
    memcpy ( &len, m_buf+m_offset, sizeof(unsigned) );

    assert ( m_buf && m_offset+len <= m_len );
    
    m_offset += sizeof(unsigned);
    v.append ( m_buf+m_offset, len );
    m_offset += len;

    return *this;
}

