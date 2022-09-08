#ifndef _STRING_BUIDER_H_
#define _STRING_BUIDER_H_
#include <string>

namespace conet
{
    template <size_t init_size>
    class StringBuilder
    {
        public:
        char m_buff[init_size];
        std::string m_str;
        size_t m_len;
        StringBuilder()
        {
            m_len = 0;
        }

        void append(unsigned int n) {
            char s[20]={0};
            char *p = s+sizeof(s)-1;
            int i =0 ;
            do {
                *p-- = "0123456789"[n%10];
                n = n/10;
                i++;
            } while(n>0);
            ++p;
            append(p, i);
        }  

        void append(std::string const &str)
        {
            size_t len = str.size();
            char const *data = str.data();
            append(data, len);
        }

        void append(char const *data, size_t len)
        {
            size_t new_len = m_len + len;
            if (m_len >= 0)
            {
                if (new_len <= init_size) {
                    memcpy(&m_buff[m_len], data, len);
                    m_len = new_len;
                    return;
                }
                m_str.reserve(4 * init_size);
                m_len = -1;
                m_str.append(&m_buff[0], init_size);
            }

            m_str.append(data, len);
        }

        char const *data()
        {
            if (m_len >= 0)
            {
                return &m_buff[0];
            }
            return m_str.data();
        }

        size_t size()
        {
            if (m_len >= 0)
            {
                return m_len;
            }

            return m_str.size();
        }
    };
}

#endif