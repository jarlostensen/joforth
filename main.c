#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "joforth.h"

joforth_t joforth;

void test_incorrect_number(void) {
    assert(joforth_eval_word(&joforth, "0"));
    //assert(joforth_eval_word(&joforth, "0foobaar") == false);
    joforth_pop_value(&joforth);    
}

void test_dec_hex(void) {
    assert(joforth_eval_word(&joforth, "hex"));
    assert(joforth_eval_word(&joforth, "0x2f"));
    assert(joforth_top_value(&joforth)==0x2f);
    assert(joforth_eval_word(&joforth, "dec"));
    assert(joforth_eval_word(&joforth, "42"));
    assert(joforth_top_value(&joforth)==42);    
    assert(joforth_eval_word(&joforth, "popa"));
}

void test_define_word(void) {
    assert(joforth_eval_word(&joforth, ": squared ( a -- a*a ) dup *  ;"));
    assert(joforth_eval_word(&joforth, "80"));
    assert(joforth_eval_word(&joforth, "squared"));
    assert(joforth_top_value(&joforth)==6400);
}

void test_create_allot(void) {
    assert(joforth_eval_word(&joforth, "create X 8 cells allot"));
    assert(joforth_eval_word(&joforth, "X"));
    assert(joforth_eval_word(&joforth, "here"));
    joforth_value_t top1 = joforth_pop_value(&joforth);
    joforth_value_t top2 = joforth_pop_value(&joforth);
    assert(top1 == top2+64);
}

int main(int argc, char* argv[]) {

    joforth_allocator_t allocator = {
        ._alloc = malloc,
        ._free = free,
    };

    joforth._stack_size = 0;    
    joforth._rstack_size = 0;
    joforth._memory_size = 0;
    joforth._allocator = &allocator;
    joforth_initialise(&joforth);

    joforth_dump_dict(&joforth);
    printf("\n");
    
    test_dec_hex();
    test_create_allot();
    test_incorrect_number();
    test_define_word();

    joforth_eval_word(&joforth, "6000");
    joforth_eval_word(&joforth, "-");
    joforth_eval_word(&joforth, "400");
    joforth_eval_word(&joforth, "+");
    joforth_eval_word(&joforth, "0");
    joforth_eval_word(&joforth, "-");
    joforth_eval_word(&joforth, ".");
    
    printf(" bye\n");
    joforth_dump_stack(&joforth);

    joforth_destroy(&joforth);
}
