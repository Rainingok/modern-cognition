#include "ntree.h"
#include "timer.h"
#include "iconvlib.h"
#include "myexception.h"

 map<string,unsigned> NTree::m_wordMap;
 string* NTree::m_wordArray = new string[MAX_WORD_NUM];
 unsigned NTree::m_arrayLen = MAX_WORD_NUM;
 unsigned NTree::m_arraySeq = 0;
 NTree* NTree::m_swap = new NTree[100000];

const NTree* NTree::lfind ( const vector<string>& path ) const
{
    if ( path.empty() ) return NULL;

    const NTree* p = this;
    for ( int i = (int)(path.size()-1); i >= 0; --i )
    {
        p = p->find( path[i] );
        if ( !p ) return NULL;
    }
    
    return p;
}

bool NTree::lpath ( const Path& rpath, Path& lpath ) const
{
    lpath.clear();
    if ( rpath.empty() ) return false;

    const NTree* p = this;
    for ( int i = (int)(rpath.size()-1); i >= 0; --i )
    {
        p = p->find( rpath[i]->word() );
        if ( !p ) return false;
        lpath.push_back ( p );
    }
    return true;
}

bool NTree::rpath ( const Path& path, Path& dpath ) const
{
    dpath.clear();
    const NTree* p = this;
    for ( size_t i = 0; i < path.size(); ++i )
    {
        p = p->find( path[i]->word() );
        if ( !p ) return false;
        dpath.push_back ( p );
    }

    return true;
}

bool NTree::rpath ( const vector<string>& path, Path& dpath ) const
{
    dpath.clear();
    const NTree* p = this;
    for ( size_t i = 0; i < path.size(); ++i )
    {
        p = p->find( path[i] );
        if ( !p ) return false;
        dpath.push_back ( p );
    }

    return true;
}

const NTree* NTree::lfind ( const Path& path ) const
{
    if ( path.empty() ) return NULL;

    const NTree* p = this;
    for ( int i = (int)(path.size()-1); i >= 0; --i )
    {
        p = p->find( path[i]->word() );
        if ( !p ) return NULL;
    }
    
    return p;
}

const NTree* NTree::rfind ( const vector<string>& path ) const
{
    if ( path.empty() ) return NULL;

    const NTree* p = this;
    for ( size_t i = 0; i < path.size(); ++i )
    {
        p = p->find( path[i] );
        if ( !p ) return NULL;
    }
    
    return p;
}

const NTree* NTree::rfind ( const Path& path ) const
{
    if ( path.empty() ) return NULL;

    const NTree* p = this;
    for ( size_t i = 0; i < path.size(); ++i )
    {
        p = p->find( path[i]->word() );
        if ( !p ) return NULL;
    }
    
    return p;
}

void NTree::prune( double rate, unsigned minFreq, unsigned parentFreq )
{
    if ( !m_childs ) return;
    
    for ( unsigned i = 1; i <= childcount(); )
    {
        if ( 1 == m_childs[i].freq()
            || minFreq > m_childs[i].freq()
            || rate*parentFreq > m_childs[i].freq() )
        {
            //要剪枝这棵子树。
            //m_childs[i].destroy();
            
            //如果只有一个子节点，则要把子节点置空。
            if ( 1 == childcount() )
            {
                delete [] m_childs;
                m_childs = NULL;
                return;
            }
            
            del ( m_childs, i );
            continue;
        }

        //该树不被剪枝，则剪枝其子树。
        m_childs[i].prune( rate, 0, m_childs[i].freq() );

        //循环变量。
        ++i;
    }
}

void NTree::destroy ( )
{
    if ( !m_childs ) return;
    for ( unsigned i = 1; i <= childcount(); ++i )
    {
        m_childs[i].destroy ();
    }
    delete [] m_childs;
    m_childs = NULL;
}

void NTree::statinfo() const
{
    uint64_t nodeNum = 0;
    uint64_t allocNum = 0;
    nodeNum = count ( allocNum );

    cerr << "-------------- TREE STAT INFO --------------\n";
    cerr << "\tNODE NUM: " << nodeNum << "\n";
    cerr << "\tALLOC NUM: " << allocNum << "\n";
    cerr << "\tWORD NUM: " << m_wordMap.size() << "\n";
    cout << "--------------------------------------------\n";
}

uint64_t NTree::count( uint64_t& alloced ) const
{
    uint64_t total = 0;
    if ( !m_childs ) return 0;
    
    total += childcount();
    alloced += m_childs[0].m_word;
    for ( unsigned i = 1; i <= childcount(); ++i )
    {
        total += m_childs[i].count( alloced );
    }

    return total;
}

uint64_t NTree::countWeight() const
{
	uint64_t totalWeight = 0;
	if ( !m_childs ) return totalWeight;
	/*for ( unsigned i = 1; i <= childcount(); ++i )
    {
        totalWeight += m_childs[i].countWeight();
    }*/
	//以当前结点为开头的字符串个数
	for ( unsigned i = 1; i <= childcount(); ++i )
    {
		/*tempFreq1 = m_childs[i].freq();//pos+freq
        tempFreq2 = tempFreq1 >> 24;//pos*/
		totalWeight += (uint64_t)(m_childs[i].freq());
    }
	
	return totalWeight;
}

void NTree::merge ( const NTree* tree )
{
    //根节点不处理。
    if ( !tree || tree->empty() ) return;
    
    for ( unsigned i = 1; i <= tree->childcount(); ++i )
    {
        NTree* pnode = appendchild( tree->m_childs[i].word(),tree->m_childs[i].pos(),tree->m_childs[i].freq() );
        pnode->merge ( &(tree->m_childs[i]) );
    }
}

//fin()时用到。
NTree* NTree::insert ( NTree* parray, const NTree& node )
{
    unsigned insertPos = 0;
    if ( bisection ( parray, node.m_word, insertPos ) )
    {
        //有找到。
        InfoLog ( "重复的节点[" << node.m_word << "]" );
        return parray + insertPos;
    }
    if ( parray[0].m_freq+1 >= parray[0].m_word )
    {
        throw myexception ( "不够的队列空间" );
    }

    if ( insertPos >= parray[0].m_freq+1 )
    {
        //是插入在最后。
        parray[insertPos] = node;
        ++(parray[0].m_freq);    
        return parray + insertPos;
    }
    
    //没有找到，则要插入。
    unsigned copyLen = parray[0].m_freq+1-insertPos;
    memcpy ( m_swap, parray+insertPos, sizeof(NTree)*copyLen );
    memcpy ( parray+insertPos+1, m_swap, sizeof(NTree)*copyLen );

    //列表长度增加。
    ++(parray[0].m_freq);    
    
    parray[insertPos] = node;
    return parray + insertPos;
}

void NTree::del ( NTree* parray, unsigned pos )
{
    if ( !parray ) return;
    unsigned dataLen = parray[0].m_freq;
    unsigned copyLen = dataLen - pos;
    if ( 0 < copyLen )
    {
        memcpy ( m_swap, parray+pos+1, sizeof(NTree)*copyLen );
        memcpy ( parray+pos, m_swap, sizeof(NTree)*copyLen );
    }
    --(parray[0].m_freq);
}

NTree* NTree::appendchild ( const string& word, int POS, int freq )
{
    unsigned seq = getseq ( word );

    unsigned insertPos = 1;
    if ( m_childs && bisection( m_childs, seq, insertPos ) )
    {
        //限制最大频率，防止越界到词性区域。
        if ( MAX_FREQ > m_childs[insertPos].freq() )
            m_childs[insertPos].m_freq += freq&MAGIC_NUM;

#ifdef _BIG_TREE_NODE
        m_childs[insertPos].m_count += POS;
#endif
        return m_childs + insertPos;
    }

    //分配空间
    if ( !idle() ) alloc();

    //没有找到。
    if ( insertPos >= childcount()+1 )
    {
        //是插入在最后。
    }
    else
    {
        //没有找到，则要插入。
        unsigned copyLen = childcount()+1-insertPos;
        memcpy ( m_swap, m_childs+insertPos, sizeof(NTree)*copyLen );
        memcpy ( m_childs+insertPos+1, m_swap, sizeof(NTree)*copyLen );
    }

    //赋数据值。
    m_childs[insertPos].m_freq = POS << 24;
    m_childs[insertPos].m_freq += freq;
    m_childs[insertPos].m_word = seq;
    m_childs[insertPos].m_childs = NULL;

#ifdef _BIG_TREE_NODE
        m_childs[insertPos].m_count = POS;
#endif    
    
    //列表长度增加。
    ++(m_childs[0].m_freq);    
    
    return m_childs + insertPos;
}

NTree* NTree::alloc ( unsigned length )
{   
    unsigned total = 0;
    
    if ( !m_childs )
    {
        total = 2;
        if ( length >= total ) total = length+1;
        m_childs = new NTree[total];
        m_childs[0].m_word = total;
        return m_childs;
    }

    unsigned len = m_childs[0].m_word;

    /*
    if ( length >= len )
    {
        total = length+1;
    }
    else
    {
        total = len+1;
    }
    */

    if ( length >= len )
    {
        total = length+1;
    }
    else if ( 10 > len )
    {
        total = len+1;
    }
    else if ( 256 > len )
    {
        total = (unsigned)(len*1.2);
    }
    else if ( 1024 > len )
    {
        total = (unsigned)(len*1.1);
    }
    else if ( 10000 > len )
    {
        total = len + 200;
    }
    else 
    {
        total = len + 500;
    }
    
    /*
    else if ( 8 > len )
    {
        total = len*2;
    }
    else if ( 64 > len )
    {
        total = (unsigned)(len*1.5);
    }
    else if ( 256 > len )
    {
        total = (unsigned)(len*1.2);
    }
    else if ( 1024 > len )
    {
        total = (unsigned)(len*1.1);
    }
    else if ( 10000 > len )
    {
        total = len + 200;
    }
    else 
    {
        total = len + 500;
    }
    */

    NTree* tmp = new NTree[total];    
    memcpy ( tmp, m_childs, sizeof(NTree)*(childcount()+1) );
    tmp[0].m_word = total;

    delete [] m_childs;
    m_childs = tmp;
    
    return m_childs;
}

const NTree* NTree::bisection ( const NTree* p, const unsigned& seq, unsigned& insertPos )
{   
    if ( !p )
    {
        insertPos = 1;  //插入位置是1，标志着要重alloc空间
        return NULL;
    }
        
    unsigned len = p[0].m_freq;    
    if ( 0 == len ) 
    {
        insertPos = 1;  //首元素用于保存长度。
        return NULL;
    }
    
    unsigned low = 1, high = len, mid=0;
    while ( low <= high )
    {
        mid = (low+high)/2;
        if ( seq == p[mid].m_word ) 
        {
            insertPos = mid;
            return p + insertPos;
        }
        
        if ( p[mid].m_word < seq )  low = mid+1;
        else high = mid-1;
        
    }
    
    if ( p[mid].m_word < seq ) insertPos = mid+1;
    else insertPos = mid;

    //std::cout << "----[" << mid << "][" << m_pointer[mid] << "][" << insertPos << "]\n";
    
    return NULL;
}

void NTree::dbgTree ( ostream& os, int outLevel, int depth ) const
{
    for ( int i = 0; i < depth; ++i ) os << "\t";
    
    //os << "[" << GBK(word()) << "/" << m_word << "] [" << freq() << "][" << pos() << "][" << m_childs << "]";
    //if ( m_childs ) os << "[" << childcount() << "][" << m_childs[0].m_word << "]";
    //os << "\n";
    
    os << "[" << word() << "][" << freq() << "][" << pos() << "]\n";

    if ( empty() || (0 != outLevel && outLevel <= depth) ) return;
    multimap<unsigned,unsigned> sorted;
    for ( unsigned i = 1; i <= childcount(); ++i )
    {
        sorted.insert( make_pair<unsigned,unsigned>(m_childs[i].freq(), i) );
        //m_childs[i].dbgTree( os, depth+1 );
    }
    for ( multimap<unsigned,unsigned>::const_reverse_iterator it = sorted.rbegin(); it != sorted.rend(); ++it )
    {
        m_childs[it->second].dbgTree( os, outLevel, depth+1 );
    }
}

bool fout ( const string& fileName, const NTree& left, const NTree& right )
{
    InfoLog ( "Now write tree to file [" << fileName << "]......" );
    FILE* fp = fopen ( fileName.c_str(), "w" );
    if ( !fp ) 
    {
        ErrorLog ( "Open file [" << fileName << "] failed!" );
        return false;
    }

    Timer t1;
    fout ( fp, left );
    fout ( fp, right );
    
    fflush ( fp );
    fclose ( fp );

    InfoLog ( "Write to file sucess [" << t1.elapsed() << "]" );
    return true;
}

bool fout ( const string& fileName, const NTree& n )
{
    InfoLog ( "Now write tree to file [" << fileName << "]......" );
    FILE* fp = fopen ( fileName.c_str(), "w" );
    if ( !fp ) 
    {
        ErrorLog ( "Open file [" << fileName << "] failed!" );
        return false;
    }

    Timer t1;
    fout ( fp, n );
    fflush ( fp );
    fclose ( fp );

    InfoLog ( "Write to file sucess [" << t1.elapsed() << "]" );
    return true;
}

//输出到文件中。
void fout ( FILE* fp, const NTree& n )
{
    static unsigned ubuf[3];
    
    unsigned len = 0;
    const NTree* tree = n.child( len );
    
    ubuf[0] = n.m_freq;
    ubuf[1] = len;
    ubuf[2] = n.word().length();

    fwrite ( ubuf, sizeof(unsigned), 3, fp );
    fwrite ( n.word().c_str(), 1, ubuf[2], fp );
    

    //遍历子树。
    for ( unsigned i = 0; i < len; ++i )
    {
        fout ( fp, tree[i] );
    }
}

bool fin ( const string& fileName, NTree& left, NTree& right )
{
    InfoLog ( "Now read tree from file [" << fileName << "]......" );
    FILE* fp = fopen ( fileName.c_str(), "r" );
    if ( !fp ) 
    {
        ErrorLog ( "Open file [" << fileName << "] failed!" );
        return false;
    }

    Timer t1;
    fin ( fp, left );
    fin ( fp, right );
    
    fclose ( fp );

    InfoLog ( "Read tree from file success [" << t1.elapsed() << "]" );
    return true;
}

bool fin ( const string& fileName, NTree& n )
{
    InfoLog ( "Now read tree from file [" << fileName << "]......" );
    FILE* fp = fopen ( fileName.c_str(), "r" );
    if ( !fp ) 
    {
        ErrorLog ( "Open file [" << fileName << "] failed!" );
        return false;
    }

    Timer t1;
    fin ( fp, n );
    fclose ( fp );

    InfoLog ( "Read tree from file success [" << t1.elapsed() << "]" );
    return true;
}

void fin ( FILE* fp, NTree& n )
{
    fin ( fp, &n );
}

void fin ( FILE* fp, NTree* root, bool isRroot )
{
    static char buf[128];
    static unsigned ubuf[3];
    
    if ( !root ) return;
    
    //读取节点值。
    fread ( ubuf, sizeof(unsigned), 3, fp );
    fread ( buf, 1, ubuf[2], fp );
    buf[ubuf[2]] = 0;
    
    NTree node;
    node.m_freq = ubuf[0];
    node.m_word = NTree::getseq ( buf );
    unsigned childLen = ubuf[1];
    
    //插入到root节点或队列。
    if ( isRroot ) *root = node;
    else root = NTree::insert ( root->m_childs, node );
    
    if ( 0 == childLen ) return;
    
    //分配整个子树的空间
    root->alloc( childLen );              
    for ( unsigned i = 1; i <= childLen; ++i )
    {
        fin ( fp, root, false );
    }    
}

ostream& operator<< (ostream& os, const NTree& node )
{
    os << "--------------------\n";
    
    node.dbgTree ( os );

    os << "--------------------\n";
    
    return os;
}
































/*
void NTree::calculateAverageWeight(unsigned& averageWight, uint64_t& totalWeight)
{
	averageWight = 0;
	unsigned totalNodeNum = 0;
	totalNodeNum = childcount();
	totalWeight = 0;
	totalWeight = countWeight();
	if(totalNodeNum <= 0) return;
	//平均每个子符串出现次数
	averageWight = unsigned (totalWeight/totalNodeNum);
}

NTree* NTree::insert ( NTree** parray, const unsigned& seq, int POS, int freq )
{
    NTree* p = *parray;
    
    unsigned insertPos = 0;
    if ( -1 != bisection ( p, seq, insertPos ) )
    {
        //有找到。
        p[insertPos].m_freq += freq&0x00FFFFFF;
        return p + insertPos;
    }

    //cout << "NO FOUND [" << seq << "]\n";
    //重分配buffer
    if ( p[0].m_freq+1 >= p[0].m_word )
    {
        //空间不够，扩展空间
        alloc ( parray );
        //buffer可能被重分配， parray的值也可能被修改。
        p = *parray;
    }

    //cout << "------------[" << insertPos << "]\n";
    
    //没有找到。
    if ( insertPos >= p[0].m_freq+1 )
    {
        //是插入在最后。
    }
    else
    {
        //没有找到，则要插入。
        unsigned copyLen = p[0].m_freq+1-insertPos;
        memcpy ( m_swap, p+insertPos, sizeof(NTree)*copyLen );
        memcpy ( p+insertPos+1, m_swap, sizeof(NTree)*copyLen );
    }

    //赋数据值。
    p[insertPos].m_freq = POS << 24;
    p[insertPos].m_freq += freq&0x00FFFFFF;
    p[insertPos].m_word = seq;
    p[insertPos].m_leftChilds = 0;
    p[insertPos].m_rightChilds = 0;
    
    //列表长度增加。
    ++(p[0].m_freq);    
    
    return p + insertPos;
}


fstream& operator<< ( fstream& fs, const NTree& n )
{
    fs << n.m_freq << n.word();

    //遍历左子树。
    unsigned len = 0;
    const NTree* tree = n.leftchild ( len );
    fs << len;   //左子树长度。
    for ( unsigned i = 0; i < len; ++i )
    {
        fs << tree[i];
    }

    //遍历右子树。
    tree = n.rightchild( len );
    fs << len;   //右子树长度。
    for ( unsigned i = 0; i < len; ++i )
    {
        fs << tree[i];
    }

    return fs;
}

fstream& operator>> ( fstream& fs, NTree& n )
{
    string word;
    fs >> n.m_freq >> word;
    n.m_word = NTree::getseq ( word );

    //左子树。
    unsigned childLen = 0;
    fs << childLen;            //左子树长度。
    NTree* tree = NULL;
    if ( 0 != childLen )
    {
        tree = n.childbuf ( true, childLen );   //预留了子树长度。
        for ( unsigned i = 0; i < childLen; ++i )
        {
            fs >> tree[i+1];
        }
    }

    //右子树。
    fs >> childLen;     //右子树长度。
    if ( 0 != childLen )
    {
        tree = n.childbuf ( false, childLen );   //预留了子树长度。
        for ( unsigned i = 0; i < childLen; ++i )
        {
            fs >> tree[i+1];
        }
    }

    return fs;
}



    unsigned insertPos = 0;
    if ( -1 != bisection ( p, seq, insertPos ) )
    {
        //有找到。
        p[insertPos].m_freq += freq&0x00FFFFFF;

        return p + insertPos;
    }

    //cout << "NO FOUND [" << seq << "]\n";
    //重分配buffer
    if ( p[0].m_freq+1 >= p[0].m_word )
    {
        //空间不够，扩展空间
        alloc ( &p );
        
        //修改参照表。
        if ( left ) m_childMap[m_leftChilds] = p;
        else m_childMap[m_rightChilds] = p;
    }

    //cout << "------------[" << insertPos << "]\n";
    
    //没有找到。
    if ( insertPos >= p[0].m_freq+1 )
    {
        //是插入在最后。
    }
    else
    {
        //没有找到，则要插入。
        unsigned copyLen = p[0].m_freq+1-insertPos;
        NTree* tmp = new NTree[copyLen];
        memcpy ( tmp, p+insertPos, sizeof(NTree)*copyLen );
        memcpy ( p+insertPos+1, tmp, sizeof(NTree)*copyLen );
        delete [] tmp;
    }

    p[insertPos].m_freq = POS << 24;
    p[insertPos].m_freq += freq&0x00FFFFFF;
    p[insertPos].m_word = seq;
    p[insertPos].m_leftChilds = 0;
    p[insertPos].m_rightChilds = 0;
    ++(p[0].m_freq);

    //cout << "------------*******[" << m_leftChilds << "][" << p[0].m_freq << "][" << childcount(true) << "]\n";
    
    return p + insertPos;
*/    

