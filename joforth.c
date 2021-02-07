
#include "joforth.h"
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <errno.h>

//TESTING
#include <stdio.h>

// based on https://en.wikipedia.org/wiki/Pearson_hashing#C,_64-bit
// initialised at start up
static unsigned char T[256];
joforth_word_key_t pearson_hash(const char *_x) {
    size_t i;
    size_t j;
    unsigned char h;
    joforth_word_key_t retval;
    const unsigned char* x = (const unsigned char*)_x;

    //TODO: if x is shorter than the key length we just replicate the MSC 
    //      and use the bytes as the key

    for (j = 0; j < sizeof(retval); ++j) {
        h = T[(x[0] + j) % 256];
        i = 1;
        while(x[i]) {
            h = T[h ^ x[i++]];
        }
        retval = ((retval << 8) | h);
    }
    return retval;
}

// ================================================================

static void _dup(joforth_t* joforth) {
    joforth_push_value(joforth, joforth_top_value(joforth));
}

static void _mul(joforth_t* joforth) {
    joforth_push_value(joforth, joforth_pop_value(joforth) * joforth_pop_value(joforth));
}

static void _plus(joforth_t* joforth) {
    joforth_push_value(joforth, joforth_pop_value(joforth) + joforth_pop_value(joforth));
}

static void _minus(joforth_t* joforth) {
    joforth_value_t val1 = joforth_pop_value(joforth);
    joforth_value_t val2 = joforth_pop_value(joforth);
    //NOTE: explicit reverse polish
    joforth_push_value(joforth, val2 - val1);
}

static void _swap(joforth_t* joforth) {
    if ( joforth->_sp < joforth->_stack_size-1) {
        joforth_value_t tos = joforth->_stack[joforth->_sp+1];
        joforth_value_t nos = joforth->_stack[joforth->_sp+2];
        joforth->_stack[joforth->_sp+1] = nos;
        joforth->_stack[joforth->_sp+2] = tos;
    }
    //TODO: else error 
}

static void _dot(joforth_t* joforth) {
    printf("%lld", joforth_pop_value(joforth));
}

// ================================================================

#define JOFORTH_DICT_BUCKETS    257

static _joforth_dict_entry_t* find_word(joforth_t* joforth, joforth_word_key_t key) {
    size_t index = key % JOFORTH_DICT_BUCKETS;
    _joforth_dict_entry_t* i = joforth->_dict + index;
    while(i!=0) {
        if(i->_key==key) {
            return i;
        }
        i = i->_next;
    }
    return 0;
}

void    joforth_initialise(joforth_t* joforth) {

    static bool _t_initialised = false;
    if (!_t_initialised) {        
        unsigned char source[256];
        for(size_t i = 0; i < 256; ++i) {
            source[i] = i;
        }
        // fill T with a random permutation of 0..255
        for(size_t i = 0; i < 256; ++i) {
            size_t index = rand() % (256-i);
            T[i] = source[index];
            source[index] = source[255-i];
        }

        _t_initialised = true;
    }

    joforth->_stack_size = joforth->_stack_size ? joforth->_stack_size : JOFORTH_DEFAULT_STACK_SIZE;
    joforth->_stack = (joforth_value_t*)malloc(joforth->_stack_size*sizeof(joforth_value_t));

    joforth->_rstack_size = joforth->_rstack_size ? joforth->_rstack_size : JOFORTH_DEFAULT_RSTACK_SIZE;
    joforth->_rstack = (_joforth_dict_entry_t**)malloc(joforth->_rstack_size*sizeof(_joforth_dict_entry_t*));

    joforth->_memory_size = joforth->_memory_size ? joforth->_memory_size : JOFORTH_DEFAULT_MEMORY_SIZE;
    joforth->_memory = (joforth_value_t*)malloc(joforth->_memory_size*sizeof(joforth_value_t));

    joforth->_dict = (_joforth_dict_entry_t*)malloc(JOFORTH_DICT_BUCKETS*sizeof(_joforth_dict_entry_t));

    // empty stacks
    joforth->_sp = joforth->_memory_size-1;
    joforth->_rp = joforth->_rstack_size-1;

    // start with decimal
    joforth->_base = 10;

    // add built-in handlers
    joforth_add_word(joforth, "dup", _dup, 1);
    joforth_add_word(joforth, "*", _mul, 2);
    joforth_add_word(joforth, "+", _plus, 2);
    joforth_add_word(joforth, "-", _minus, 2);
    joforth_add_word(joforth, ".", _dot, 1);
    joforth_add_word(joforth, "swap", _swap, 2);
}

void    joforth_destroy(joforth_t* joforth) {
    free(joforth->_stack);
    free(joforth->_rstack);
    free(joforth->_memory);
    if(joforth->_dict) {
        for(size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t * bucket = joforth->_dict + i;
            bucket = bucket->_next;
            while(bucket) {
                _joforth_dict_entry_t * j = bucket;
                bucket = bucket->_next;
                free(j);
            }
        }        
        free(joforth->_dict);
    }
    memset(joforth,0,sizeof(joforth_t));
}

void    joforth_add_word(joforth_t* joforth, const char* word, joforth_word_handler_t handler, size_t depth) {

    joforth_word_key_t key = pearson_hash(word);
     //TODO: check it's not already there
    size_t index = key % JOFORTH_DICT_BUCKETS;
    _joforth_dict_entry_t* i = joforth->_dict + index;
    if(i->_next!=0) {        
        while(i->_next!=0) {
            i = i->_next;
        }
        i->_next = (_joforth_dict_entry_t*)malloc(sizeof(_joforth_dict_entry_t));        
        i = i->_next;
        i->_next = 0;
    }    
    i->_key = key;
    i->_word = word; //<ZZZ:
    i->_depth = depth;
    i->_handler = handler;
    //printf("\nadded word \"%s\", key = 0x%x\n", i->_word, i->_key);
}


static void    joforth_push_word(joforth_t* joforth, const char* word) {    
    //TODO: assert on stack overflow
    joforth_word_key_t key = pearson_hash(word);
    _joforth_dict_entry_t* entry = find_word(joforth, key);
    if(entry) {
        joforth->_rstack[joforth->_rp--] = entry;
    }
    //TODO: report error
}

static _joforth_dict_entry_t*    joforth_pop_word(joforth_t* joforth) {
    if(joforth->_rp<joforth->_rstack_size) {
        return joforth->_rstack[++joforth->_rp];
    }
    // empty stack
    return 0;
}

static _joforth_dict_entry_t* joforth_top_word(joforth_t* joforth) {
    if(joforth->_rp<joforth->_rstack_size) {
        return joforth->_rstack[joforth->_rp+1];
    }
    // empty stack
    return 0;
}

void    joforth_eval(joforth_t* joforth) {
    _joforth_dict_entry_t* word;
    while((word = joforth_pop_word(joforth))) {
        word->_handler(joforth);
    }
}

void    joforth_eval_word(joforth_t* joforth, const char* word) {
    joforth_word_key_t key = pearson_hash(word);
    _joforth_dict_entry_t* entry = find_word(joforth, key);
    if(entry) {
        // execute
        entry->_handler(joforth);
    }
    else {
        joforth_value_t value = (joforth_value_t)strtoll(word, 0, joforth->_base);
        if(!errno) {
            // push value
            joforth->_stack[joforth->_sp--] = value;
        }
        else {
            //TODO: invalid format
            fprintf(stderr, "INVALID WORD \"%s\"\n", word);
        }
    }
}

void    joforth_dump_dict(joforth_t* joforth) {
    printf("joforth dictionary info:\n");
    if(joforth->_dict) {
        for(size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t * entry = joforth->_dict + i;
            while(entry) {
                if(entry->_key) {
                    printf("\tentry: key 0x%x, word \"%s\", takes %zu parameters\n", entry->_key, entry->_word, entry->_depth);
                }
                entry = entry->_next;
            }
        }
    }
    else {
        printf("\tempty\n");
    }
}

void    joforth_dump_stack(joforth_t* joforth) {
    if ( joforth->_sp == joforth->_stack_size ) {
        printf("joforth: stack is empty\n");
    }
    else {
        printf("joforth stack contents:\n");
        for(size_t sp = joforth->_sp+1; sp < joforth->_stack_size; ++sp) {
            printf("\t%lld\n", joforth->_stack[sp]);
        }
    }
}
