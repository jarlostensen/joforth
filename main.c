#include <stdio.h>
#include "joforth.h"

joforth_t joforth;

int main(int argc, char* argv[]) {

    joforth._memory_size = 0;
    joforth._stack_size = 0;
    joforth_initialise(&joforth);
    
    joforth_eval_word(&joforth, "80");
    joforth_eval_word(&joforth, "dup");
    joforth_eval_word(&joforth, "*");
    joforth_eval_word(&joforth, "dup");
    joforth_eval_word(&joforth, "6000");
    joforth_eval_word(&joforth, "-");
    joforth_eval_word(&joforth, "400");
    joforth_eval_word(&joforth, "+");
    joforth_eval_word(&joforth, ".");
    
    printf(" bye\n");

    joforth_destroy(&joforth);
}
