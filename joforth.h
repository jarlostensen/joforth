#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef int64_t     joforth_value_t;
typedef uint32_t    joforth_word_address_t;
typedef uint32_t    joforth_word_key_t;

typedef struct _joforth joforth_t;

typedef void (*joforth_word_handler_t)(joforth_t* joforth);

typedef struct _joforth_dict_entry {
    joforth_word_key_t      _key;
    const char*             _word;
    joforth_word_handler_t  _handler;
    size_t                  _depth;
    struct _joforth_dict_entry*    _next;
} _joforth_dict_entry_t;

#define JOFORTH_DEFAULT_STACK_SIZE      0x8000
#define JOFORTH_DEFAULT_MEMORY_SIZE     0x8000
#define JOFORTH_DEFAULT_RSTACK_SIZE     0x100

typedef struct _joforth {
    _joforth_dict_entry_t       *   _dict;
    joforth_value_t             *   _stack;
    // the RSTACK is both used to store words as they are parsed and to generate programs
    _joforth_dict_entry_t      **   _rstack;
    joforth_value_t             *   _memory;

    // current input base
    int                             _base;

    // if 0 then default, in units of joforth_value_t 
    size_t                          _stack_size;
    // if 0 then default, in units of joforth_value_t 
    size_t                          _memory_size;
    // if 0 then default, in units of calls 
    size_t                          _rstack_size;

    // stack pointers
    size_t                          _sp;
    size_t                          _rp;
    
} joforth_t;

void    joforth_initialise(joforth_t* joforth);
void    joforth_destroy(joforth_t* joforth);

void    joforth_add_word(joforth_t* joforth, const char* word, joforth_word_handler_t handler, size_t depth);

void    joforth_eval_word(joforth_t* joforth, const char* word);

static __attribute__((always_inline)) void    joforth_push_value(joforth_t* joforth, joforth_value_t value) {
    //TODO: assert on stack overflow
    joforth->_stack[joforth->_sp--] = value;
}
static __attribute__((always_inline)) joforth_value_t joforth_pop_value(joforth_t* joforth) {
    //TODO: assert on stack underflow
    return joforth->_stack[++joforth->_sp];
}
static __attribute__((always_inline)) joforth_value_t    joforth_top_value(joforth_t* joforth) {
    return joforth->_stack[joforth->_sp+1];
}

void    joforth_eval(joforth_t* joforth);

void    joforth_dump_dict(joforth_t* joforth);
void    joforth_dump_stack(joforth_t* joforth);