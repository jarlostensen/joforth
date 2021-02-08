#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "joforth.h"

int main(int argc, char* argv[]) {

    joforth_allocator_t allocator = {
        ._alloc = malloc,
        ._free = free,
    };

    joforth_t joforth;
    joforth._stack_size = 0;
    joforth._rstack_size = 0;
    joforth._allocator = &allocator;
    joforth_initialise(&joforth);

    joforth_dump_dict(&joforth);
    printf("\n");
    
    joforth_eval_word(&joforth, ": squared ( a -- a*a ) dup *  ;");
    joforth_eval_word(&joforth, "80");
    joforth_eval_word(&joforth, "squared");    
    joforth_eval_word(&joforth, "6000");
    joforth_eval_word(&joforth, "-");
    joforth_eval_word(&joforth, "400");
    joforth_eval_word(&joforth, "+");
    joforth_eval_word(&joforth, ".");
    
    printf(" bye\n");
    joforth_dump_stack(&joforth);

    joforth_destroy(&joforth);
}
