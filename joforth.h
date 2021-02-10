#pragma once 

// =======================================================================
// joForth
// A very simple, and yet incomplete, Forth interpreter and compiler. 
// 
// http://galileo.phys.virginia.edu/classes/551.jvn.fall01/primer.htm
//

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <jo.h>

#define JOFORTH_MAX_WORD_LENGTH 128

typedef int64_t     joforth_value_t;
typedef uint32_t    joforth_word_address_t;
typedef uint32_t    joforth_word_key_t;
typedef struct _joforth joforth_t;
typedef struct _joforth_dict_entry _joforth_dict_entry_t;

typedef void (*joforth_word_handler_t)(joforth_t* joforth);

// used internally
typedef enum _joforth_word_type {

    kWordType_Handler = 1,
    kWordType_Value,
    kWordType_Word,
    kWordType_Prefix,
    kWordType_End = 8,
    kWordType_TypeMask = 7

} _joforth_word_type_t;

// used internally
typedef struct _joforth_word_details {
    // this is either a single entry or an array of entries terminated with the "kWordType_End" type
    _joforth_word_type_t    _type;
    union {
        // a native callable function 
        joforth_word_handler_t              _handler;
        // a value to push on the stack during execution
        joforth_value_t                     _value;
        // a dictonary entry; i.e. a call to another word (which contains one or more of these)
        _joforth_dict_entry_t  *            _word;
    } _rep;
    // value stack depth required (i.e. number of arguments to word)
    size_t                                  _depth;

} _joforth_word_details_t;
// used internally
typedef struct _joforth_dict_entry {
    joforth_word_key_t              _key;
    const char*                     _word;
    // one or more words
    _joforth_word_details_t   *     _details;

    // for hash table linking only
    struct _joforth_dict_entry*     _next;
} _joforth_dict_entry_t;

#define JOFORTH_DEFAULT_STACK_SIZE      0x8000
#define JOFORTH_DEFAULT_MEMORY_SIZE     0x8000
#define JOFORTH_DEFAULT_RSTACK_SIZE     0x100
#define JOFORTH_TRUE                    ((joforth_value_t)1)
#define JOFORTH_FALSE                   ((joforth_value_t)0)

// used internally
typedef struct _joforth_rstack_entry {
    _joforth_dict_entry_t*      _entry;
    _joforth_word_details_t*    _details;
} _joforth_rstack_entry_t;

// used to provide allocator/free functionality, joForth doesn't use any other allocator
typedef struct _joforth_allocator {
    void* (*_alloc)(size_t);
    void (*_free)(void*);
} joforth_allocator_t;

typedef struct _joforth_ir_buffer _joforth_ir_buffer_t;

// the joForth VM state
typedef struct _joforth {
    _joforth_dict_entry_t       *   _dict;
    joforth_value_t             *   _stack;
    _joforth_rstack_entry_t     *   _rstack;
    uint8_t                     *   _memory;
    _joforth_ir_buffer_t        *   _ir_buffer;
    joforth_allocator_t             _allocator;
    
    // current input base
    int                             _base;
    // if 0 then default, in units of joforth_value_t 
    size_t                          _stack_size;
    // if 0 then default, in units of _joforth_dict_entry_t 
    size_t                          _rstack_size;    
    // if 0 then default, in units of bytes
    size_t                          _memory_size;
    // stack pointers
    size_t                          _sp;
    size_t                          _rp;
    // memory allocation pointer (we don't do "free")
    size_t                          _mp;
    // status code of last operation
    jo_status_t                     _status;
    
} joforth_t;

// initialise the joForth VM
void    joforth_initialise(joforth_t* joforth);
// shutdown
void    joforth_destroy(joforth_t* joforth);
// add a word to the interpreter with an immediate evaluator (handler) and the required stack depth
void    joforth_add_word(joforth_t* joforth, const char* word, joforth_word_handler_t handler, size_t depth);
// evaluate a sequence of words (sentence)
// for example:
//  joforth_eval(&joforth, ": squared ( a -- a*a ) dup *  ;");
//  joforth_eval(&joforth, "80");
//  joforth_eval(&joforth, "squared");
//
bool    joforth_eval(joforth_t* joforth, const char* word);

// push a value on the stack (use this in your handlers)
// sets the zero flag if the value is 0
static _JO_ALWAYS_INLINE void    joforth_push_value(joforth_t* joforth, joforth_value_t value) {
    assert(joforth->_sp);
    joforth->_stack[joforth->_sp--] = value; 
}

// pop a value off the stack (use this in your handlers)
static _JO_ALWAYS_INLINE joforth_value_t joforth_pop_value(joforth_t* joforth) {
    assert(joforth->_sp < joforth->_stack_size-1);
    return joforth->_stack[++joforth->_sp];
}

// read top value from the stack (use this in your handlers)
static _JO_ALWAYS_INLINE joforth_value_t    joforth_top_value(joforth_t* joforth) {
    return joforth->_stack[joforth->_sp+1];
}

// printf dictionary contents
void    joforth_dump_dict(joforth_t* joforth);
// dump current stack
void    joforth_dump_stack(joforth_t* joforth);