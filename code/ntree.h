#ifndef __N_TREE_H__
#define __N_TREE_H__

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "packstream.h"
#include "streamlog.h"

#define MAX_WORD_NUM 500000
#define MAGIC_NUM 0x00FFFFFF
#define MAX_FREQ 0x00FFFFF0

using namespace std;
typedef long long LLong;

struct Path;

class NTree
{
protected:
    
    //词分配表。
    //word, index
    static map<string,unsigned> m_wordMap;

    //词表。是一个大的词buffer
    static string* m_wordArray;

    //词表总长度。
    static unsigned m_arrayLen;
    
    //词buffer的增长seq
    static unsigned m_arraySeq;

    //数据交换空间。空间必须足够大。
    static NTree* m_swap;
    
    ///////////////////////////////////////////////////////
    //在子树列表中，第一个元素，m_freq 是数据的长度。m_word 是buffer的长度。
    unsigned m_freq;            //高8位: POS，低24位: freq
    unsigned m_word;            //词的序号。
    NTree* m_childs;          //子树列表

#ifdef _BIG_TREE_NODE
    long int m_count;
#endif

    public:
        NTree() { m_freq=0; m_word=0; m_childs=NULL; };
        
        inline unsigned freq() const 
            { 
            return m_freq & MAGIC_NUM;
            };

        inline int pos() const
            {
            return m_freq >> 24;
            };
        
        inline const string& word() const 
            {
            return m_wordArray[m_word];
            };
        
        inline const NTree* child( unsigned& len ) const
            {
            len = 0;
            if ( !m_childs ) return NULL;

            len = m_childs[0].m_freq;
            return m_childs + 1;    //第一个节点是列表长度。
            };
        
#ifdef _BIG_TREE_NODE
        inline unsigned value () const
            {
            return m_count;
            }
#endif

        inline unsigned childcount () const
            {
            if ( !m_childs ) return 0;
            return m_childs[0].m_freq;
            }
        
        inline bool empty( ) const
            {
            return !m_childs;
            };
        
        inline static NTree* setroot ()
            {
            NTree* root = new NTree;
            root->m_word = getseq ( "ROOT" );
            return root;
            };
        
        inline const NTree* find ( const string& word ) const
            {
            if ( !m_childs ) return NULL;
            
            map<string,unsigned>::const_iterator it = m_wordMap.find(word);
            if ( m_wordMap.end() == it ) return NULL;

            return find ( it->second );
            };

        inline const NTree* find ( unsigned wordId ) const
            {
            unsigned insertPos = 0;
            if ( !bisection( m_childs, wordId, insertPos ) )
            {
                return NULL;
            }
            
            return m_childs+insertPos;
            };

        inline const NTree* find ( const NTree& node ) const
            {
            return find ( node.m_word );
            };
        
		inline const LLong getWordNum ( const std::string& strWord ) const
			{
				std::map<std::string,unsigned>::const_iterator it = m_wordMap.find(strWord);
            	if ( m_wordMap.end() == it )
				{
					return -1;
            	}
				else
				{
					return it->second;
				}
			};
		inline const unsigned getIndex () const
			{
				return m_word;
			};

        const NTree* lfind ( const vector<string>& path ) const;

        const NTree* rfind ( const vector<string>& path ) const;
        
        const NTree* lfind ( const Path& path ) const;

        const NTree* rfind ( const Path& path ) const;

        bool lpath ( const Path& rpath, Path& lpath ) const;

        bool rpath ( const Path& path, Path& dpath ) const;

        bool rpath ( const vector<string>& path, Path& dpath ) const;
        
        //树的一些统计信息。
        void statinfo() const;

        //对树进行剪枝。
        //minFreq: 树的第一层要剪去的最小频率。
        //rate: 第一层往下的层，要剪去的比例。
        //parentFreq: 内部控制变量，不需要赋值。
        void prune( double rate, unsigned minFreq, unsigned parentFreq=0 );
        
        //合并两棵树。
        void merge ( const NTree* tree );

        //销毁一棵树。
        void destroy ();
        
        //插入子节点。
        NTree* appendchild ( const string& word, int POS, int freq=1 );
        
        //打包和解包
        friend ostream& operator<< (ostream& os, const NTree& node );

        friend bool fout ( const string& fileName, const NTree& left, const NTree& right );

        friend void fout ( FILE* fp, const NTree& n );

        friend bool fout ( const string& fileName, const NTree& n );

        friend bool fin ( const string& fileName, NTree& left, NTree& right );

        friend void fin ( FILE* fp, NTree& n );

        friend void fin ( FILE* fp, NTree* root, bool isRroot=true );

        friend bool fin ( const string& fileName, NTree& n );

        void dbgTree ( ostream& os, int outLevel=0, int depth=0 ) const;

        
    protected:
        static void del ( NTree* parray, unsigned pos );
        
        static NTree* insert ( NTree* parray, const NTree& node );
        
        static NTree* insert ( NTree** parray, const unsigned& seq, int POS, int freq );

        static const NTree* bisection ( const NTree* p, const unsigned& seq, unsigned& insertPos );
        
        uint64_t count( uint64_t& alloced ) const;
		
		uint64_t countWeight() const;
        
        inline int idle() const
            {
                if ( !m_childs ) return 0;
                return m_childs[0].m_word-m_childs[0].m_freq-1;
            };
        
        //分配或扩张子树空间。
        NTree* alloc ( unsigned length=0 );
                    
        inline static unsigned getseq ( const string& word ) 
            {
            
            map<string,unsigned>::const_iterator it = m_wordMap.find(word);
            if ( m_wordMap.end() != it ) return it->second;
            
            m_wordMap[word] = m_arraySeq;
            m_wordArray[m_arraySeq] = word;
            
            unsigned seq = m_arraySeq;
            m_arraySeq++;
            return seq;
            };
        
};

struct Path
{
    std::vector<const NTree*> m_path;
	Path()
	{
	};
	Path( const Path& other, int first, int last )
	{
		for(int i = first; i < last; ++i)
		{
			push_back(other[i]);
		}
	};
    inline int freq() const
        {
            if ( m_path.empty() ) return 0;
            return m_path.back()->freq();
        };

    inline const std::vector<const NTree*>& path() const
        {
            return m_path;
        };

    inline void clear() { m_path.clear(); };
    
    inline bool empty() const
        {
            return m_path.empty();
        };

    inline size_t size() const
        {
            return m_path.size();
        };
    
    inline const NTree* operator[] (size_t i) const
        {
            return m_path.at(i);
        };

    inline const NTree* last() const
        {
            return m_path.back();
        };
    
    inline void push_back ( const NTree* p )
        {
            m_path.push_back ( p );
        };

    inline void pop_back ()
        {
            m_path.pop_back();
        };
    
    string word() const
        {
            std::string w;
            for ( size_t i = 0; i < m_path.size(); ++i ) w += m_path[i]->word();
            return w;
        };
	string word( size_t dwBegin, size_t dwEnd ) const
        {
            std::string w;
            for ( size_t i = dwBegin; i >= 0 && i < m_path.size()&& i < dwEnd; ++i ) w += m_path[i]->word();
            return w;
        };
	void Index( std::vector<unsigned>& vecPathIndex )
        {
            for ( size_t i = 0; i < m_path.size(); ++i ) 
            {
				vecPathIndex.push_back( m_path[i]->getIndex() );
            }
			return;
        };

    inline Path subpath ( size_t wh ) const
        {
            Path sub;
            sub.m_path.assign ( m_path.begin()+wh, m_path.end() );
            return sub;
        };
    
    string head ( size_t wh ) const
        {
            std::string w;
            for ( size_t i = 0; i < wh && i < m_path.size(); ++i ) w += m_path[i]->word();
            return w;
        };

    string tail ( size_t wh ) const
        {
            std::string w;
            for ( size_t i = wh; i < m_path.size(); ++i ) w += m_path[i]->word();
            return w;
        };

    friend ostream& operator<< (ostream& os, const Path& path )
        {
            for ( size_t i = 0; i < path.size(); ++i )
            {
                os << "[" << path[i]->word() << "] ";
            }
            os << "\n";

            return os;
        };
};

class traverse_iterator
{
    protected:
        Path m_path;
        const NTree* m_tree;
        unsigned m_pos;

    public:
        
};

#endif //__N_TREE_H__

