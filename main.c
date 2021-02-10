#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "joforth.h"

joforth_t joforth;

void test_incorrect_number(void) {
    assert(joforth_eval(&joforth, "0"));
    //assert(joforth_eval(&joforth, "0foobaar") == false);
    joforth_pop_value(&joforth);    
}

void test_dec_hex(void) {
    assert(joforth_eval(&joforth, "hex"));
    assert(joforth_eval(&joforth, "0x2f"));
    assert(joforth_top_value(&joforth)==0x2f);
    assert(joforth_eval(&joforth, "dec"));
    assert(joforth_eval(&joforth, "42"));
    assert(joforth_top_value(&joforth)==42);    
    assert(joforth_eval(&joforth, "popa"));
}

void test_define_word(void) {
    assert(joforth_eval(&joforth, ": squared ( a -- a*a ) dup *  ;"));
    assert(joforth_eval(&joforth, "80"));
    assert(joforth_eval(&joforth, "squared"));
    assert(joforth_top_value(&joforth)==6400);
}

void test_create_allot(void) {
    assert(joforth_eval(&joforth, "create X 8 cells allot"));
    assert(joforth_eval(&joforth, "X"));
    assert(joforth_eval(&joforth, "here"));
    joforth_value_t top1 = joforth_pop_value(&joforth);
    joforth_value_t top2 = joforth_pop_value(&joforth);
    assert(top1 == top2+64);
    // store 137 in the 5th cell, starting at X
    assert(joforth_eval(&joforth, "137  X 5 cells +  !"));
    // retrieve it
    assert(joforth_eval(&joforth, "X 5 cells + @"));
    assert(joforth_pop_value(&joforth) == 137);
}

void test_comparison(void) {
    assert(joforth_eval(&joforth, "2 3 >"));
    assert(joforth_pop_value(&joforth)==JOFORTH_FALSE);
    assert(joforth_eval(&joforth, "2 3 <"));
    assert(joforth_pop_value(&joforth)==JOFORTH_TRUE);
    assert(joforth_eval(&joforth, "3 3 ="));
    assert(joforth_pop_value(&joforth)==JOFORTH_TRUE);
}

void test_ifthenelse(void) {
    assert(joforth_eval(&joforth, ".if-then-else 0 0 =  IF  TRUE  ELSE  FALSE  THEN cr"));
    joforth_value_t tos = joforth_pop_value(&joforth);
    assert(tos == JOFORTH_TRUE);
}

int main(int argc, char* argv[]) {

    joforth_allocator_t allocator = {
        ._alloc = malloc,
        ._free = free,
    };

    joforth._stack_size = 0;    
    joforth._rstack_size = 0;
    joforth._memory_size = 0;
    joforth._allocator = *(&(joforth_allocator_t){
        ._alloc = malloc,
        ._free = free,
    });
    joforth_initialise(&joforth);

    joforth_dump_dict(&joforth);
    printf("\n");
    
    assert(joforth_eval(&joforth, ".\"running tests..\" cr"));
    test_ifthenelse();
    test_dec_hex();
    test_create_allot();
    test_incorrect_number();
    test_comparison();
    test_define_word();

    printf(" bye\n");
    joforth_dump_stack(&joforth);

    joforth_destroy(&joforth);
}
