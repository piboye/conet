#ifndef __CONET_FCONTEXT_H__
#define __CONET_FCONTEXT_H__

#include <stdint.h>

namespace conet
{

struct stack_t
{
    void    *sp;
    uint64_t size;

    stack_t() :
        sp(NULL), size(0)
    {

    }
};

struct fp_t
{
    uint32_t     fc_freg[2];

    fp_t() :
        fc_freg()
    {}
};

struct fcontext_t
{
    uint64_t     fc_greg[8];
    stack_t      fc_stack;
    fp_t         fc_fp;

    fcontext_t() :
        fc_greg(),
        fc_stack(),
        fc_fp()
    {}
};

extern "C" 
void *jump_fcontext(fcontext_t **ofc, fcontext_t *nfc,
        void *vp, bool preserve_fpu = true);

extern "C"
fcontext_t *make_fcontext(void * sp, uint64_t size, void (* fn)(void *) );

}


#endif 
