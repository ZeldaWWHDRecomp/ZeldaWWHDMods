#include <assert.h>
#include "../src/dialogue_input.h"
int main(void) {
    dragon_dialogue_input input={0};
    assert(!dragon_dialogue_quiet(&input,0,0));
    assert(dragon_dialogue_quiet(&input,1,0x0800)); /* Opening the letter. */
    assert(dragon_dialogue_quiet(&input,1,0));
    assert(input.blocked==0); /* Opening press is not retained forever. */
    for(unsigned button=0x4000;button<=0x8000;button*=2) {
        assert(dragon_dialogue_quiet(&input,1,button)); /* Close/accept frame. */
        for(unsigned i=0;i<30;++i)assert(dragon_dialogue_quiet(&input,0,button));
        assert(dragon_dialogue_quiet(&input,0,0)); /* Release is also consumed. */
        assert(!dragon_dialogue_quiet(&input,0,0));
        assert(!dragon_dialogue_quiet(&input,0,button)); /* New deliberate press works. */
    }
    assert(dragon_dialogue_quiet(&input,1,0x4000|0x8000));
    assert(dragon_dialogue_quiet(&input,0,0x4000));
    assert(dragon_dialogue_quiet(&input,0,0));
    assert(!dragon_dialogue_quiet(&input,0,0));
    /* Cancel a recognized song with no quest dialogue open. The native game
       must see neither a held B nor its release, but a fresh later B works. */
    assert(dragon_modal_quiet(&input,0,1,0x4000));
    for(unsigned i=0;i<30;++i)assert(dragon_modal_quiet(&input,0,0,0x4000));
    assert(dragon_modal_quiet(&input,0,0,0));
    assert(!dragon_modal_quiet(&input,0,0,0x4000));
    assert(dragon_modal_quiet(&input,1,0,0x8000));
    assert(dragon_modal_quiet(&input,0,0,0));
    assert(!dragon_modal_quiet(&input,0,0,0));
    return 0;
}
