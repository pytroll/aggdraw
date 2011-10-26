//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.2
// Copyright (C) 2002-2004 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software 
// is granted provided this copyright notice appears in all copies. 
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
#ifndef AGG_ARRAY_INCLUDED
#define AGG_ARRAY_INCLUDED

#include <string.h>
#include "agg_basics.h"

namespace agg
{

    //---------------------------------------------------------------pod_array
    // A simple class template to store Plain Old Data, a vector
    // of a fixed size. The data is continous in memory
    //------------------------------------------------------------------------
    template<class T> class pod_array
    {
    public:
        typedef T value_type;

        ~pod_array() { delete [] m_array; }
        pod_array() : m_size(0), m_capacity(0), m_array(0) {}
        pod_array(unsigned cap, unsigned extra_tail=0);

        // Copying
        pod_array(const pod_array<T>&);
        const pod_array<T>& operator = (const pod_array<T>&);

        unsigned capacity() const { return m_capacity; }
        void capacity(unsigned cap, unsigned extra_tail=0);

        void resize(unsigned new_size);

        void add(const T& v)  { m_array[m_size++] = v; }
        void inc_size(unsigned size) { m_size += size; } 
        unsigned size() const { return m_size; }
        const T& operator [] (unsigned idx) const { return m_array[idx]; }
              T& operator [] (unsigned idx)       { return m_array[idx]; }

        void remove_all()         { m_size = 0; }
        void cut_at(unsigned num) { if(num < m_size) m_size = num; }

    private:
        unsigned m_size;
        unsigned m_capacity;
        T*       m_array;
    };

    //------------------------------------------------------------------------
    template<class T> 
    void pod_array<T>::capacity(unsigned cap, unsigned extra_tail)
    {
        m_size = 0;
        if(cap > m_capacity)
        {
            delete [] m_array;
            m_capacity = cap + extra_tail;
            m_array = m_capacity ? new T [m_capacity] : 0;
        }
    }

    //------------------------------------------------------------------------
    template<class T> 
    void pod_array<T>::resize(unsigned new_size)
    {
        if(new_size > m_size)
        {
            if(new_size > m_capacity)
            {
                T* data = new T[new_size];
                memcpy(data, m_array, m_size * sizeof(T));
                delete [] m_array;
                m_array = data;
            }
        }
        else
        {
            m_size = new_size;
        }
    }

    //------------------------------------------------------------------------
    template<class T> pod_array<T>::pod_array(unsigned cap, unsigned extra_tail) :
        m_size(0), m_capacity(0), m_array(0)
    {
        capacity(cap, extra_tail);
    }

    //------------------------------------------------------------------------
    template<class T> pod_array<T>::pod_array(const pod_array<T>& v) :
        m_size(v.m_size),
        m_capacity(v.m_capacity),
        m_array(v.m_capacity ? new T [v.m_capacity] : 0)
    {
        memcpy(m_array, v.m_array, sizeof(T) * v.m_size);
    }

    //------------------------------------------------------------------------
    template<class T> const pod_array<T>& 
    pod_array<T>::operator = (const pod_array<T>&v)
    {
        capacity(v.m_capacity);
        if(v.m_size) memcpy(m_array, v.m_array, sizeof(T) * v.m_size);
        return *this;
    }





    //------------------------------------------------------------------------
    template<class T> class pod_array_adaptor
    {
    public:
        typedef T value_type;
        pod_array_adaptor(T* array, unsigned size) : 
            m_array(array), m_size(size) {}

        unsigned size() const { return m_size; }
        const T& operator [] (unsigned idx) const { return m_array[idx]; }
              T& operator [] (unsigned idx)       { return m_array[idx]; }
    private:
        T*       m_array;
        unsigned m_size;
    };






    //---------------------------------------------------------------pod_deque
    // A simple class template to store Plain Old Data, similar to std::deque
    // It doesn't reallocate memory but instead, uses blocks of data of size 
    // of (1 << S), that is, power of two. The data is NOT continuous in memory, 
    // so the only valid access method is operator [] or curr(), prev(), next()
    // 
    // There reallocs occure only when the pool of pointers to blocks needs 
    // to be extended (it happens very rear). You can control the value 
    // of increment to reallocate the pointer buffer. See the second constructor.
    // By default, the incremeent value equals (1 << S), i.e., the block size.
    //------------------------------------------------------------------------
    template<class T, unsigned S=6> class pod_deque
    {
        enum 
        {   
            block_shift = S,
            block_size  = 1 << block_shift,
            block_mask  = block_size - 1
        };

    public:
        typedef T value_type;

        ~pod_deque();
        pod_deque();
        pod_deque(unsigned block_ptr_inc);

        // Copying
        pod_deque(const pod_deque<T, S>& v);
        const pod_deque<T, S>& operator = (const pod_deque<T, S>& v);

        void remove_all() { m_size = 0; }
        void free_all() { free_tail(0); }
        void free_tail(unsigned size);
        void add(const T& val);
        void modify_last(const T& val);
        void remove_last();

        int allocate_continuous_block(unsigned num_elements);

        void cut_at(unsigned size)
        {
            if(size < m_size) m_size = size;
        }

        unsigned size() const { return m_size; }

        const T& operator [] (unsigned idx) const
        {
            return m_blocks[idx >> block_shift][idx & block_mask];
        }

        T& operator [] (unsigned idx)
        {
            return m_blocks[idx >> block_shift][idx & block_mask];
        }

        const T& curr(unsigned idx) const
        {
            return (*this)[idx];
        }

        T& curr(unsigned idx)
        {
            return (*this)[idx];
        }

        const T& prev(unsigned idx) const
        {
            return (*this)[(idx + m_size - 1) % m_size];
        }

        T& prev(unsigned idx)
        {
            return (*this)[(idx + m_size - 1) % m_size];
        }

        const T& next(unsigned idx) const
        {
            return (*this)[(idx + 1) % m_size];
        }

        T& next(unsigned idx)
        {
            return (*this)[(idx + 1) % m_size];
        }

        const T& last() const
        {
            return (*this)[m_size - 1];
        }

        T& last()
        {
            return (*this)[m_size - 1];
        }

        unsigned byte_size() const;
        void serialize(int8u* ptr) const;

    private:
        void allocate_block(unsigned nb);
        T*   data_ptr();

        unsigned        m_size;
        unsigned        m_num_blocks;
        unsigned        m_max_blocks;
        T**             m_blocks;
        unsigned        m_block_ptr_inc;
    };




    //------------------------------------------------------------------------
    template<class T, unsigned S> pod_deque<T, S>::~pod_deque()
    {
        if(m_num_blocks)
        {
            T** blk = m_blocks + m_num_blocks - 1;
            while(m_num_blocks--)
            {
                delete [] *blk;
                --blk;
            }
            delete [] m_blocks;
        }
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    void pod_deque<T, S>::free_tail(unsigned size)
    {
        if(size < m_size)
        {
            unsigned nb = (size + block_mask) >> block_shift;
            while(m_num_blocks > nb)
            {
                delete [] m_blocks[--m_num_blocks];
            }
            m_size = size;
        }
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> pod_deque<T, S>::pod_deque() :
        m_size(0),
        m_num_blocks(0),
        m_max_blocks(0),
        m_blocks(0),
        m_block_ptr_inc(block_size)
    {
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    pod_deque<T, S>::pod_deque(unsigned block_ptr_inc) :
        m_size(0),
        m_num_blocks(0),
        m_max_blocks(0),
        m_blocks(0),
        m_block_ptr_inc(block_ptr_inc)
    {
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    pod_deque<T, S>::pod_deque(const pod_deque<T, S>& v) :
        m_size(v.m_size),
        m_num_blocks(v.m_num_blocks),
        m_max_blocks(v.m_max_blocks),
        m_blocks(v.m_max_blocks ? new T* [v.m_max_blocks] : 0),
        m_block_ptr_inc(v.m_block_ptr_inc)
    {
        unsigned i;
        for(i = 0; i < v.m_num_blocks; ++i)
        {
            m_blocks[i] = new T [block_size];
            memcpy(m_blocks[i], v.m_blocks[i], block_size * sizeof(T));
        }
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    const pod_deque<T, S>& pod_deque<T, S>::operator = (const pod_deque<T, S>& v)
    {
        unsigned i;
        for(i = m_num_blocks; i < v.m_num_blocks; ++i)
        {
            allocate_block(i);
        }
        for(i = 0; i < v.m_num_blocks; ++i)
        {
            memcpy(m_blocks[i], v.m_blocks[i], block_size * sizeof(T));
        }
        m_size = v.m_size;
        return *this;
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S>
    void pod_deque<T, S>::allocate_block(unsigned nb)
    {
        if(nb >= m_max_blocks) 
        {
            T** new_blocks = new T* [m_max_blocks + m_block_ptr_inc];

            if(m_blocks)
            {
                memcpy(new_blocks, 
                       m_blocks, 
                       m_num_blocks * sizeof(T*));

                delete [] m_blocks;
            }
            m_blocks = new_blocks;
            m_max_blocks += m_block_ptr_inc;
        }
        m_blocks[nb] = new T [block_size];
        m_num_blocks++;
    }



    //------------------------------------------------------------------------
    template<class T, unsigned S>
    inline T* pod_deque<T, S>::data_ptr()
    {
        unsigned nb = m_size >> block_shift;
        if(nb >= m_num_blocks)
        {
            allocate_block(nb);
        }
        return m_blocks[nb] + (m_size & block_mask);
    }



    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    inline void pod_deque<T, S>::add(const T& val)
    {
        *data_ptr() = val;
        ++m_size;
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    inline void pod_deque<T, S>::remove_last()
    {
        if(m_size) --m_size;
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    void pod_deque<T, S>::modify_last(const T& val)
    {
        remove_last();
        add(val);
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    int pod_deque<T, S>::allocate_continuous_block(unsigned num_elements)
    {
        if(num_elements < block_size)
        {
            data_ptr(); // Allocate initial block if necessary
            unsigned rest = block_size - (m_size & block_mask);
            unsigned index;
            if(num_elements <= rest)
            {
                // The rest of the block is good, we can use it
                //-----------------
                index = m_size;
                m_size += num_elements;
                return index;
            }

            // New block
            //---------------
            m_size += rest;
            data_ptr();
            index = m_size;
            m_size += num_elements;
            return index;
        }
        return -1; // Impossible to allocate
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    unsigned pod_deque<T, S>::byte_size() const
    {
        return m_size * sizeof(T);
    }


    //------------------------------------------------------------------------
    template<class T, unsigned S> 
    void pod_deque<T, S>::serialize(int8u* ptr) const
    {
        unsigned i;
        for(i = 0; i < m_size; i++)
        {
            memcpy(ptr, &(*this)[i], sizeof(T));
            ptr += sizeof(T);
        }
    }





    //-----------------------------------------------------------pod_allocator
    // Allocator for arbitrary POD data. Most usable in different cache
    // systems for efficient memory allocations. 
    // Memory is allocated with blocks of fixed size ("block_size" in
    // the constructor). If required size exceeds the block size the allocator
    // creates a new block of the required size. However, the most efficient
    // use is when the average reqired size is much less than the block size. 
    //------------------------------------------------------------------------
    class pod_allocator
    {
    public:
        void remove_all()
        {
            if(m_num_blocks)
            {
                int8u** blk = m_blocks + m_num_blocks - 1;
                while(m_num_blocks--)
                {
                    delete [] *blk;
                    --blk;
                }
                delete [] m_blocks;
            }
            m_num_blocks = 0;
            m_max_blocks = 0;
            m_blocks = 0;
            m_buf_ptr = 0;
            m_rest = 0;
        }

        ~pod_allocator()
        {
            remove_all();
        }

        pod_allocator(unsigned block_size, unsigned block_ptr_inc=256-8) :
            m_block_size(block_size),
            m_block_ptr_inc(block_ptr_inc),
            m_num_blocks(0),
            m_max_blocks(0),
            m_blocks(0),
            m_buf_ptr(0),
            m_rest(0)
        {
        }
       

        int8u* allocate(unsigned size, unsigned alignment=1)
        {
            if(size == 0) return 0;
            if(size <= m_rest)
            {
                int8u* ptr = m_buf_ptr;
                if(alignment > 1)
                {
                    unsigned align = (alignment - (unsigned long)(ptr) % alignment) % alignment;
                    size += align;
                    ptr += align;
                    if(size <= m_rest)
                    {
                        m_rest -= size;
                        m_buf_ptr += size;
                        return ptr;
                    }
                    allocate_block(size);
                    return allocate(size - align, alignment);
                }
                m_rest -= size;
                m_buf_ptr += size;
                return ptr;
            }
            allocate_block(size + alignment - 1);
            return allocate(size, alignment);
        }


    private:
        void allocate_block(unsigned size)
        {
            if(size < m_block_size) size = m_block_size;
            if(m_num_blocks >= m_max_blocks) 
            {
                int8u** new_blocks = new int8u* [m_max_blocks + m_block_ptr_inc];

                if(m_blocks)
                {
                    memcpy(new_blocks, 
                           m_blocks, 
                           m_num_blocks * sizeof(int8u*));

                    delete [] m_blocks;
                }
                m_blocks = new_blocks;
                m_max_blocks += m_block_ptr_inc;
            }
            m_blocks[m_num_blocks] = m_buf_ptr = new int8u [size];
            m_num_blocks++;
            m_rest = size;
        }

        unsigned m_block_size;
        unsigned m_block_ptr_inc;
        unsigned m_num_blocks;
        unsigned m_max_blocks;
        int8u**  m_blocks;
        int8u*   m_buf_ptr;
        unsigned m_rest;
    };








    //------------------------------------------------------------------------
    enum
    {
        quick_sort_threshold = 9
    };

    
    //-----------------------------------------------------------swap_elements
    template<class T> inline void swap_elements(T& a, T& b)
    {
        T temp = a;
        a = b;
        b = temp;
    }


    //--------------------------------------------------------------quick_sort
    template<class Array, class Less>
    void quick_sort(Array& arr, Less less)
    {
        if(arr.size() < 2) return;

        typename Array::value_type* e1;
        typename Array::value_type* e2;

        int  stack[80];
        int* top = stack; 
        int  limit = arr.size();
        int  base = 0;

        for(;;)
        {
            int len = limit - base;

            int i;
            int j;
            int pivot;

            if(len > quick_sort_threshold)
            {
                // we use base + len/2 as the pivot
                pivot = base + len / 2;
                swap_elements(arr[base], arr[pivot]);

                i = base + 1;
                j = limit - 1;

                // now ensure that *i <= *base <= *j 
                e1 = &(arr[j]); 
                e2 = &(arr[i]);
                if(less(*e1, *e2)) swap_elements(*e1, *e2);

                e1 = &(arr[base]); 
                e2 = &(arr[i]);
                if(less(*e1, *e2)) swap_elements(*e1, *e2);

                e1 = &(arr[j]); 
                e2 = &(arr[base]);
                if(less(*e1, *e2)) swap_elements(*e1, *e2);

                for(;;)
                {
                    do i++; while( less(arr[i], arr[base]) );
                    do j--; while( less(arr[base], arr[j]) );

                    if( i > j )
                    {
                        break;
                    }

                    swap_elements(arr[i], arr[j]);
                }

                swap_elements(arr[base], arr[j]);

                // now, push the largest sub-array
                if(j - base > limit - i)
                {
                    top[0] = base;
                    top[1] = j;
                    base   = i;
                }
                else
                {
                    top[0] = i;
                    top[1] = limit;
                    limit  = j;
                }
                top += 2;
            }
            else
            {
                // the sub-array is small, perform insertion sort
                j = base;
                i = j + 1;

                for(; i < limit; j = i, i++)
                {
                    for(; less(*(e1 = &(arr[j + 1])), *(e2 = &(arr[j]))); j--)
                    {
                        swap_elements(*e1, *e2);
                        if(j == base)
                        {
                            break;
                        }
                    }
                }
                if(top > stack)
                {
                    top  -= 2;
                    base  = top[0];
                    limit = top[1];
                }
                else
                {
                    break;
                }
            }
        }
    }




    //------------------------------------------------------remove_duplicates
    // Remove duplicates from a sorted array. It doesn't cut the the 
    // tail of the array, it just returns the number of remaining elements.
    //-----------------------------------------------------------------------
    template<class Array, class Equal>
    unsigned remove_duplicates(Array& arr, Equal equal)
    {
        if(arr.size() < 2) return arr.size();

        unsigned i, j;
        for(i = 1, j = 1; i < arr.size(); i++)
        {
            typename Array::value_type& e = arr[i];
            if(!equal(e, arr[i - 1]))
            {
                arr[j++] = e;
            }
        }
        return j;
    }




}

#endif
