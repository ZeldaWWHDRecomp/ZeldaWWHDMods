#include <assert.h>
#include "../src/recovery.h"
int main(void) {
    const enum dragon_phase waiting[]={DRAGON_LOADING,DRAGON_APPROACH};
    for(unsigned i=0;i<2;++i) {
        enum dragon_phase phase=waiting[i];
        assert(!dragon_wait_cancel(phase,300,1,1,0,0));
        assert(dragon_wait_cancel(phase,300.5f,1,1,0,0));
        assert(dragon_wait_cancel(phase,0,0,1,0,0));
        assert(dragon_wait_cancel(phase,0,1,0,0,0));
        assert(dragon_wait_cancel(phase,0,1,1,1,0));
        assert(dragon_wait_cancel(phase,0,1,1,0,1));
    }
    assert(!dragon_wait_cancel(DRAGON_RIDING,500,0,0,1,1));
    unsigned old=DRAGON_PRIVATE_TAG|1,newer=DRAGON_PRIVATE_TAG|2;
    assert(dragon_request_owned(23,newer,23,newer));
    assert(!dragon_request_owned(23,old,23,newer)); /* recycled actor ID */
    assert(!dragon_request_owned(22,newer,23,newer));
    assert(!dragon_request_owned(23,newer,~0u,0)); /* cancelled request */
    assert(!dragon_rope_ready(0,0)); /* constructor failure */
    assert(!dragon_rope_ready(12,0)); /* initialization failure */
    assert(dragon_rope_ready(12,1));
    return 0;
}
