/*
 * =====================================================================================
 *
 *       Filename:  scope_ptr.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月22日 17时12分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __SCOPED_PTR_H__
#define __SCOPED_PTR_H__
namespace conet
{
    template <typename T>
    class ScopedPtr
    {
        public:
            typedef T value_type;
            typedef T* ptr_type;
            typedef T* const_ptr_type;


            ptr_type m_ptr;

            explicit
            ScopedPtr(ptr_type p)
            {
                m_ptr = p;
            }

            value_type &operator *() const
            {
                return *m_ptr;
            }

            value_type *operator ->() const
            {
                return m_ptr;
            }

            value_type * get() const
            {
                return m_ptr;
            }

            value_type * release() 
            {
                T * p = m_ptr;
                m_ptr = NULL;
                return p;
            }

            void reset(T *p= 0)
            {
                m_ptr = p; 
            }

            ~ScopedPtr()
            {
                delete m_ptr;
            }

        private:
            //disable copy and assignment
            ScopedPtr(ScopedPtr<T> const &right);
            ScopedPtr<T> &operator=(ScopedPtr<T> const &right);
    };

    template <typename T>
    void swap(ScopedPtr<T> &a, ScopedPtr<T> &b)
    {
         T * p1 = a.release();
         T * p2 = b.release();
         a.reset(p2);
         b.reset(p1);
    }
}


#endif /* end of include guard */
