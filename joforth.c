
#include "joforth.h"
#include "joforth_ir.h"
#include <errno.h>
#include <stdlib.h>

#include <stdio.h>

// based on https://en.wikipedia.org/wiki/Pearson_hashing#C,_64-bit
// initialised at start up
static unsigned char T[256];
joforth_word_key_t pearson_hash(const char* _x) {
    size_t i;
    size_t j;
    unsigned char h;
    joforth_word_key_t retval = 0;
    const unsigned char* x = (const unsigned char*)_x;

    for (j = 0; j < sizeof(retval); ++j) {
        h = T[(x[0] + j) % 256];
        i = 1;
        while (x[i]) {
            h = T[h ^ x[i++]];
        }
        retval = ((retval << 8) | h);
    }
    return retval;
}

// ================================================================

#define JOFORTH_DICT_BUCKETS    257
static _joforth_dict_entry_t* _add_entry(joforth_t* joforth, const char* word) {
    joforth_word_key_t key = pearson_hash(word);
    //TODO: check it's not already there
    size_t index = key % JOFORTH_DICT_BUCKETS;
    _joforth_dict_entry_t* i = joforth->_dict + index;
    while (i->_next != 0) {
        i = i->_next;
    }

    i->_next = (_joforth_dict_entry_t*)joforth->_allocator._alloc(sizeof(_joforth_dict_entry_t));
    memset(i->_next, 0, sizeof(_joforth_dict_entry_t));
    i->_key = key;
    i->_word = word; //<ZZZ: this *should* be copied, not stored    

    return i;
}

static _joforth_dict_entry_t* _add_word(joforth_t* joforth, const char* word) {

    _joforth_dict_entry_t* i = _add_entry(joforth, word);
    i->_details = (_joforth_word_details_t*)joforth->_allocator._alloc(sizeof(_joforth_word_details_t));
    return i;
}

static uint8_t* _alloc(joforth_t* joforth, size_t bytes) {
    assert(joforth->_mp < (joforth->_memory_size - bytes));
    size_t mp = joforth->_mp;
    joforth->_mp += bytes;
    return joforth->_memory + mp;
}

static void _lt(joforth_t* joforth) {
    //NOTE: NOS < TOS
    joforth_push_value(joforth, joforth_pop_value(joforth) > joforth_pop_value(joforth) ? JOFORTH_TRUE : JOFORTH_FALSE);
}

static void _gt(joforth_t* joforth) {
    //NOTE: NOS > TOS
    joforth_push_value(joforth, joforth_pop_value(joforth) < joforth_pop_value(joforth) ? JOFORTH_TRUE : JOFORTH_FALSE);
}

static void _eq(joforth_t* joforth) {
    //NOTE: NOS == TOS
    joforth_push_value(joforth, joforth_pop_value(joforth) == joforth_pop_value(joforth) ? JOFORTH_TRUE : JOFORTH_FALSE);
}

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
    if (joforth->_sp < joforth->_stack_size - 1) {
        joforth_value_t tos = joforth->_stack[joforth->_sp + 1];
        joforth_value_t nos = joforth->_stack[joforth->_sp + 2];
        joforth->_stack[joforth->_sp + 1] = nos;
        joforth->_stack[joforth->_sp + 2] = tos;
    }
    //TODO: else error 
}

static void _drop(joforth_t* joforth) {
    /* _ = */ joforth_pop_value(joforth);
}

static void _dot(joforth_t* joforth) {
    printf("%lld", joforth_pop_value(joforth));
}

static void _bang(joforth_t* joforth) {
    // store value at (relative) address
    joforth_value_t address = joforth_pop_value(joforth);
    joforth_value_t* ptr = (joforth_value_t*)(joforth->_memory + address);
    ptr[0] = joforth_pop_value(joforth);
}

static void _at(joforth_t* joforth) {
    // retrieve value at (relative) address
    joforth_value_t address = joforth_pop_value(joforth);
    joforth_value_t* ptr = (joforth_value_t*)(joforth->_memory + address);
    joforth_push_value(joforth, ptr[0]);
}

// simply drop the entire stack
static void _popa(joforth_t* joforth) {
    joforth->_sp = joforth->_stack_size - 1;
}

static void _dec(joforth_t* joforth) {
    joforth->_base = 10;
}

static void _hex(joforth_t* joforth) {
    joforth->_base = 16;
}

static void _create(joforth_t* joforth) {
    // the stack MUST contain the address of the name of a new word we'll create
    char* ptr = (char*)joforth_pop_value(joforth);
    // here goes nothing...
    _joforth_dict_entry_t* entry = _add_word(joforth, ptr);
    if (entry) {
        entry->_details->_type = kWordType_End | kWordType_Value;
        // next available memory slot, at the time we're creating
        entry->_details->_rep._value = (joforth_value_t)joforth->_mp;
    }
    else {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
    }
}

static void _here(joforth_t* joforth) {
    joforth_push_value(joforth, joforth->_mp);
}

static void _cells(joforth_t* joforth) {
    // convert COUNT CELLS to a byte count and push it
    joforth_value_t count = joforth_pop_value(joforth);
    joforth_push_value(joforth, count * sizeof(joforth_value_t));
}

static void _allot(joforth_t* joforth) {
    // expects byte count on stack
    joforth_value_t bytes = joforth_pop_value(joforth);
    if (bytes) {
        //NOTE: we don't save the result, it's expected that the caller 
        //      uses variables or HERE for that
        _alloc(joforth, bytes);
    }
    else {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
    }
}

static void _cr(joforth_t* joforth) {
    (void)joforth;
    printf("\n");
}

// ================================================================

static _joforth_dict_entry_t* _find_word(joforth_t* joforth, joforth_word_key_t key) {
    size_t index = key % JOFORTH_DICT_BUCKETS;
    _joforth_dict_entry_t* i = joforth->_dict + index;
    while (i != 0) {
        if (i->_key == key) {
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
        for (size_t i = 0; i < 256; ++i) {
            source[i] = i;
        }
        // fill T with a random permutation of 0..255
        for (size_t i = 0; i < 256; ++i) {
            size_t index = rand() % (256 - i);
            T[i] = source[index];
            source[index] = source[255 - i];
        }

        _t_initialised = true;
    }

    joforth->_stack_size = joforth->_stack_size > JOFORTH_DEFAULT_STACK_SIZE ? joforth->_stack_size : JOFORTH_DEFAULT_STACK_SIZE;
    joforth->_stack = (joforth_value_t*)joforth->_allocator._alloc(joforth->_stack_size * sizeof(joforth_value_t));
    joforth->_sp = joforth->_stack_size - 1;

    joforth->_rstack_size = joforth->_rstack_size > JOFORTH_DEFAULT_RSTACK_SIZE ? joforth->_rstack_size : JOFORTH_DEFAULT_RSTACK_SIZE;
    joforth->_rstack = (_joforth_rstack_entry_t*)joforth->_allocator._alloc(joforth->_rstack_size * sizeof(_joforth_rstack_entry_t));
    joforth->_rp = joforth->_rstack_size - 1;

    joforth->_memory_size = joforth->_memory_size > JOFORTH_DEFAULT_MEMORY_SIZE ? joforth->_memory_size : JOFORTH_DEFAULT_MEMORY_SIZE;
    joforth->_memory = (uint8_t*)joforth->_allocator._alloc(joforth->_memory_size);
    joforth->_mp = 0;

    // set aside some memory for the IR buffer    
#define JOFORTH_DEFAULT_IRBUFFER_SIZE    1024
    joforth->_ir_buffer = (_joforth_ir_buffer_t*)_alloc(joforth, JOFORTH_DEFAULT_IRBUFFER_SIZE+sizeof(_joforth_ir_buffer_t));
    joforth->_ir_buffer->_buffer = joforth->_ir_buffer+1;
    joforth->_ir_buffer->_size = JOFORTH_DEFAULT_IRBUFFER_SIZE;
    joforth->_ir_buffer->_irr = joforth->_ir_buffer->_irw = 0;
    
    joforth->_dict = (_joforth_dict_entry_t*)joforth->_allocator._alloc(JOFORTH_DICT_BUCKETS * sizeof(_joforth_dict_entry_t));
    memset(joforth->_dict, 0, JOFORTH_DICT_BUCKETS * sizeof(_joforth_dict_entry_t));

    // start with decimal
    joforth->_base = 10;

    // all clear
    joforth->_status = _JO_STATUS_SUCCESS;

    // add built-in handlers
    joforth_add_word(joforth, "<", _lt, 2);
    joforth_add_word(joforth, ">", _gt, 2);
    joforth_add_word(joforth, "=", _eq, 2);
    joforth_add_word(joforth, "dup", _dup, 1);
    joforth_add_word(joforth, "*", _mul, 2);
    joforth_add_word(joforth, "+", _plus, 2);
    joforth_add_word(joforth, "-", _minus, 2);
    joforth_add_word(joforth, ".", _dot, 1);
    joforth_add_word(joforth, "swap", _swap, 2);
    joforth_add_word(joforth, "drop", _drop, 1);
    joforth_add_word(joforth, "!", _bang, 2);
    joforth_add_word(joforth, "@", _at, 1);
    joforth_add_word(joforth, "dec", _dec, 0);
    joforth_add_word(joforth, "hex", _hex, 0);
    joforth_add_word(joforth, "popa", _popa, 0);
    joforth_add_word(joforth, "here", _here, 0);
    joforth_add_word(joforth, "allot", _allot, 1);
    joforth_add_word(joforth, "cells", _cells, 1);
    joforth_add_word(joforth, "cr", _cr, 0);

    // add special words
    _joforth_dict_entry_t* entry = _add_word(joforth, "create");
    entry->_details->_type = kWordType_End | kWordType_Prefix;
    entry->_details->_rep._handler = _create;
    entry->_details->_depth = 1;
}

void    joforth_destroy(joforth_t* joforth) {
    joforth->_allocator._free(joforth->_stack);
    joforth->_allocator._free(joforth->_rstack);
    joforth->_allocator._free(joforth->_memory);
    if (joforth->_dict) {
        for (size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t* bucket = joforth->_dict + i;
            bucket = bucket->_next;
            while (bucket) {
                _joforth_dict_entry_t* j = bucket;
                bucket = bucket->_next;
                joforth->_allocator._free(j);
            }
        }
        joforth->_allocator._free(joforth->_dict);
    }
    memset(joforth, 0, sizeof(joforth_t));
}

void    joforth_add_word(joforth_t* joforth, const char* word, joforth_word_handler_t handler, size_t depth) {

    _joforth_dict_entry_t* i = _add_word(joforth, word);
    i->_details->_depth = depth;
    i->_details->_type = kWordType_Handler | kWordType_End;
    i->_details->_rep._handler = handler;

    //printf("\nadded word \"%s\", key = 0x%x\n", i->_word, i->_key);
}


static _JO_ALWAYS_INLINE void _push_rstack(joforth_t* joforth, _joforth_dict_entry_t* entry, _joforth_word_details_t* details) {
    assert(joforth->_rp);
    _joforth_rstack_entry_t item = { ._entry = entry, ._details = details };
    joforth->_rstack[joforth->_rp--] = item;
}

static _JO_ALWAYS_INLINE _joforth_rstack_entry_t _pop_rstack(joforth_t* joforth) {
    assert(joforth->_rp < joforth->_rstack_size - 1);
    return joforth->_rstack[++joforth->_rp];
}

static _JO_ALWAYS_INLINE bool _rstack_is_empty(joforth_t* joforth) {
    return joforth->_rp == joforth->_rstack_size - 1;
}

static joforth_value_t  _str_to_value(joforth_t* joforth, const char* str) {
    joforth_value_t value = (joforth_value_t)strtoll(str, 0, joforth->_base);
    if (errno) {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
        return 0;
    }
    if (!value) {
        // is it 0, or is it wrong...?
        // skip sign first

        if (*str == '-' || *str == '+')
            ++str;

        switch (joforth->_base) {
        case 10:
        {
            while (*str && *str >= '0' && *str <= '9') ++str;
            if (*str && *str != ' ') {
                // some non-decimal character found
                joforth->_status = _JO_STATUS_INVALID_INPUT;
                return 0;
            }
            // otherwise it's a valid 0
            joforth->_status = _JO_STATUS_SUCCESS;
        }
        break;
        case 16:
        {
            if (strlen(str) > 2 && str[1] == 'x')
                str += 2;

            while (*str && *str >= '0' && *str <= '9' && *str >= 'a' && *str <= 'f') ++str;
            if (*str && *str != ' ') {
                // some non-hex character found
                joforth->_status = _JO_STATUS_INVALID_INPUT;
                return 0;
            }
            // otherwise it's a valid 0
            joforth->_status = _JO_STATUS_SUCCESS;
        }
        break;
        default: {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
        }
        }
    }
    return value;
}

static size_t _count_words(const char* word) {
    size_t words = 0;
    do {
        while (word[0] && word[0] == ' ') ++word;
        if (word[0] && word[0] != ';') {

            if (word[0] == '(') {
                while (word[0] && word[0] != ')') ++word;
                if (!word[0]) {
                    return 0;
                }
                ++word;
                continue;
            }

            while (word[0] && word[0] != ' ' && word[0] != ';') ++word;
            if (word[0] == ' ')
                ++words;
        }
    } while (word[0] && word[0] != ';');

    return words;
}

static const char* _next_word(joforth_t* joforth, char* buffer, size_t buffer_size, const char* word, size_t* wp_) {

    *wp_ = 0;
    while (word[0] && word[0] == ' ') ++word;
    if (!word[0]) {
        return 0;
    }
    // skip comments
    if (word[0] == '(') {
        while (word[0] && word[0] != ')') ++word;
        if (!word[0]) {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return 0;
        }
        ++word;
        while (word[0] && word[0] == ' ') ++word;
        if (!word[0]) {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return 0;
        }
    }
    size_t wp = 0;
    bool scan_string = false;
    while (word[0]) {
        if (*word == '\"') {
            scan_string = !scan_string;
        }
        if (!scan_string && *word == ' ') {
            break;
        }
        buffer[wp++] = (char)tolower(*word++);
        if (wp == buffer_size) {
            joforth->_status = _JO_STATUS_RESOURCE_EXHAUSTED;
            return 0;
        }
    }
    buffer[wp] = 0;
    *wp_ = wp;
    return word;
}

// evaluator mode
typedef enum _joforth_eval_mode {

    kEvalMode_Compiling,
    kEvalMode_Interpreting,
    kEvalMode_Skipping,

} _joforth_eval_mode_t;

bool    joforth_eval(joforth_t* joforth, const char* word) {

    if (_JO_FAILED(joforth->_status))
        return false;

    while (word[0] && word[0] == ' ') ++word;
    if (word[0] == 0) {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
        return false;
    }

    _joforth_eval_mode_t mode = (word[0] == ':') ? kEvalMode_Compiling : kEvalMode_Interpreting;

    char buffer[JOFORTH_MAX_WORD_LENGTH];
    size_t wp;
    
    _joforth_ir_buffer_t*   irbuffer = joforth->_ir_buffer;

    if (mode == kEvalMode_Compiling) {

        _ir_emit(irbuffer, kIr_DefineWord);
        // skip ":"
        word++;
        // we expect the identifier to be next
        word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp);
        if (!word || _JO_FAILED(joforth->_status)) {
            return false;
        }
        uint8_t* memory = _alloc(joforth, wp + 1);
        memcpy(memory, buffer, wp + 1);
        _ir_emit_ptr(irbuffer, memory);
    }

    word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp);
    if (!word || _JO_FAILED(joforth->_status)) {
        return false;
    }

    // =====================================================================================
    // phase 1: convert the input text to a stream of IR codes    
    // count words
    // =====================================================================================

    size_t word_count = 0;
    do {
        // first check for language keywords
        bool is_language_keyword = false;
        for (size_t n = 0; n < _joforth_keyword_lut_size; ++n) {
            if (strcmp(buffer, _joforth_keyword_lut[n]._id) == 0) {
                _ir_emit(irbuffer, _joforth_keyword_lut[n]._ir);
                is_language_keyword = true;
                break;
            }
        }

        if (!is_language_keyword) {
            // then check for very special ."string" syntax (which gets here as a single word)
            if (wp > 1 && buffer[0] == '.') {
                // print a string
                size_t start = 1;
                size_t end = 2;
                if (buffer[start] == '\"') {
                    start = 2;
                    end = 3;
                }
                while (buffer[end] && buffer[end] != '\"') ++end;
                buffer[end] = 0;
                if (start < end) {
                    // emit "dot" and put the allocated string on the value stack 
                    uint8_t* memory = _alloc(joforth, end - start + 1);
                    memcpy(memory, buffer + start, end - start + 1);
                    _ir_emit(irbuffer, kIr_ValuePtr);
                    _ir_emit_ptr(irbuffer, memory);
                    _ir_emit(irbuffer, kIr_Dot);
                }
            }
            else {
                joforth_word_key_t key = pearson_hash(buffer);
                _joforth_dict_entry_t* entry = _find_word(joforth, key);

                if (entry) {
                    _joforth_word_details_t* details = entry->_details;

                    switch (details->_type & kWordType_TypeMask) {
                    case kWordType_Prefix:
                    case kWordType_Handler:
                    case kWordType_Word:
                        _ir_emit(irbuffer, kIr_WordPtr);
                        _ir_emit_ptr(irbuffer, entry);
                        break;
                    case kWordType_Value:
                        _ir_emit(irbuffer, kIr_Value);
                        _ir_emit_value(irbuffer, details->_rep._value);
                        break;
                    default:;
                    }
                }
                else {
                    joforth_value_t value = _str_to_value(joforth, buffer);
                    if (_JO_FAILED(joforth->_status)) {
                        uint8_t* memory = _alloc(joforth, wp + 1);
                        memcpy(memory, buffer, wp + 1);
                        _ir_emit(irbuffer, kIr_ValuePtr);
                        _ir_emit_ptr(irbuffer, memory);
                        joforth->_status = _JO_STATUS_SUCCESS;
                    }
                    else {
                        _ir_emit(irbuffer, kIr_Value);
                        _ir_emit_value(irbuffer, value);
                    }
                }
            }
        }
        ++word_count;
        word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp);

    } while (wp);

    // =====================================================================================
    // phase 2: interpret or compile
    // =====================================================================================

    _joforth_word_details_t* details = 0;
    size_t irr = 0;

    // are we interpreting or compiling? if the latter we need a bit of setup
    if (mode == kEvalMode_Compiling) {
        assert(_ir_consume(irbuffer) == kIr_DefineWord);
        const char* id = (const char*)_ir_consume_ptr(irbuffer);
        joforth_word_key_t key = pearson_hash(id);
        _joforth_dict_entry_t* entry = _find_word(joforth, key);
        if (entry) {
            // already exists
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return false;
        }
        entry = _add_entry(joforth, id);
        // allocate a array for the sequence of words we need (minus the ; at the end)
        details = entry->_details = (_joforth_word_details_t*)joforth->_allocator._alloc((word_count - 1) * sizeof(_joforth_word_details_t));
    }

    //ZZZ:
    _joforth_eval_mode_t mode_stack[32];
    size_t msp = 31;

    // interpret or compile the contents of the IR buffer
    while (!_ir_buffer_is_empty(irbuffer)) {

        _joforth_ir_t ir = _ir_consume(irbuffer);

        switch (ir) {
        case kIr_True:
        {
            if (mode != kEvalMode_Skipping) {
                joforth_push_value(joforth, JOFORTH_TRUE);
            }
        }
        break;
        case kIr_False:
        {
            if (mode != kEvalMode_Skipping) {
                joforth_push_value(joforth, JOFORTH_FALSE);
            }
        }
        break;
        case kIr_If:
        {
            //TODO: COMPILE

            // decide what to do based on TOS
            joforth_value_t tos = joforth_pop_value(joforth);
            if (tos == JOFORTH_FALSE) {
                // skip until ELSE
                mode = kEvalMode_Skipping;
                // we'll switch back to interpreting later
                mode_stack[msp--] = kEvalMode_Interpreting;
            }
            else {
                // else we continue as if nothing happened until we hit ELSE...
                mode_stack[msp--] = kEvalMode_Skipping;
            }
        }
        break;
        case kIr_Else:
        {
            // switch to the mode selected by the last IF and continue
            mode = mode_stack[++msp];
        }
        break;
        case kIr_Then:
        {
            //TODO: COMPILING
            // switch back to interpreting
            mode = kEvalMode_Interpreting;
        }
        break;
        case kIr_Dot:
        {
            if (mode != kEvalMode_Skipping) {
                const char* str = (const char*)joforth_pop_value(joforth);
                printf(str);
            }
        }
        break;
        case kIr_ValuePtr:
        {
            joforth_value_t value = (joforth_value_t)_ir_consume_ptr(irbuffer);
            if (mode != kEvalMode_Skipping) {
                //TODO: compiling...?
                joforth_push_value(joforth, value);
            }
        }
        break;
        case kIr_Value:
        {
            joforth_value_t value = _ir_consume_value(irbuffer);
            if (mode != kEvalMode_Skipping) {
                if (mode == kEvalMode_Compiling) {
                    // a number; we add this to the word sequence as an immediate value
                    details->_type = kWordType_Value;
                    details->_rep._value = value;
                }
                else {
                    joforth_push_value(joforth, value);
                }
            }
        }
        break;
        case kIr_WordPtr:
        {
            if (mode == kEvalMode_Compiling) {
                details->_type = kWordType_Word;
                details->_rep._word = (_joforth_dict_entry_t*)_ir_consume_ptr(irbuffer);
            }
            else {
                _joforth_dict_entry_t* entry = (_joforth_dict_entry_t*)_ir_consume_ptr(irbuffer);
                details = entry->_details;
                // ========================================================================
                // execute the words making up the entry until we hit the "kWordType_End" flag
                // 
                while (1) {
                    // execute word(s)
                    switch (details->_type & kWordType_TypeMask) {
                    case kWordType_Handler:
                        if (mode != kEvalMode_Skipping) {
                            // check that the stack has enough variables for this routine to execute
                            if ((joforth->_stack_size - joforth->_sp - 1) >= details->_depth) {
                                details->_rep._handler(joforth);
                                if (_JO_FAILED(joforth->_status)) {
                                    return false;
                                }
                            }
                            else {
                                joforth->_status = _JO_STATUS_FAILED_PRECONDITION;
                                return false;
                            }
                        }
                        break;
                    case kWordType_Value:
                        if (mode != kEvalMode_Skipping) {
                            joforth_push_value(joforth, details->_rep._value);
                        }
                        break;
                    case kWordType_Word:
                    {   if (mode != kEvalMode_Skipping) {
                        // return entry on rstack (unless this is the last one)
                        if ((details->_type & kWordType_End) == 0) {
                            _push_rstack(joforth, entry, details + 1);
                        }
                        // switch to a different word
                        entry = details->_rep._word;
                        details = details->_rep._word->_details;
                        continue;
                    }
                    }
                    break;
                    case kWordType_Prefix:
                    {
                        // first check that we've got enough arguments following this instruction
                        //TODO: can CREATE, VARIABLE, SEE etc. accept the result of another word...?
                        // for now; only accept value and word ptrs 
                        assert(details->_depth < 2);

                        //ZZZ: this doesn't check softly if we're at the end of the buffer
                        _joforth_ir_t next_ir = _ir_consume(irbuffer);
                        switch (next_ir) {
                        case kIr_WordPtr:
                        case kIr_ValuePtr:
                        {
                            //NOTE: we're not doing any type checking here, if it requires a WordPtr when a ValuePtr 
                            //      is passed then things WILL go wrong...
                            void* ptr = _ir_consume_ptr(irbuffer);
                            if (mode != kEvalMode_Skipping) {
                                joforth_push_value(joforth, (joforth_value_t)ptr);
                                details->_rep._handler(joforth);
                            }
                        }
                        break;
                        default:
                            joforth->_status = _JO_STATUS_INVALID_INPUT;
                            return false;
                        }
                    }
                    default:;
                    }

                    if (mode != kEvalMode_Skipping) {
                        if ((details->_type & kWordType_End)) {
                            if (_rstack_is_empty(joforth)) {
                                // we're done for now
                                break;
                            }
                            _joforth_rstack_entry_t i = _pop_rstack(joforth);
                            entry = i._entry;
                            details = i._details;
                            continue;
                        }
                        // next word in multi-word sentence...
                        ++details;
                    }
                }
            }
        }
        break;
        default:;
        }

        if (mode == kEvalMode_Compiling) {
            if (_ir_peek(irbuffer) != kIr_EndDefineWord) {
                // advance details pointer as we're building a word
                ++details;
            }
            else {
                // terminate this word
                details->_type |= kWordType_End;
                // and consume the EndDefineWord IR
                _ir_consume(irbuffer);
            }
        }
    }

    return true;
}

void    joforth_dump_dict(joforth_t* joforth) {
    printf("joforth dictionary info:\n");
    if (joforth->_dict) {
        for (size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t* entry = joforth->_dict + i;
            while (entry->_key) {
                if ((entry->_details->_type & kWordType_Prefix) == 0) {
                    printf("\tentry: key 0x%x, word \"%s\", takes %zu parameters\n", entry->_key, entry->_word, entry->_details->_depth);
                }
                else {
                    printf("\tPREFIX entry: word \"%s\", takes %zu parameters\n", entry->_word, entry->_details->_depth);
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
    if (joforth->_sp == (joforth->_stack_size - 1)) {
        printf("joforth: stack is empty\n");
    }
    else {
        printf("joforth stack contents:\n");
        for (size_t sp = joforth->_sp + 1; sp < joforth->_stack_size; ++sp) {
            printf("\t%lld\n", joforth->_stack[sp]);
        }
    }
}
