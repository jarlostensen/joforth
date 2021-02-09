
#include "joforth.h"
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <stdio.h>

// based on https://en.wikipedia.org/wiki/Pearson_hashing#C,_64-bit
// initialised at start up
static unsigned char T[256];
joforth_word_key_t pearson_hash(const char* _x) {
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
    if (i->_next != 0) {
        while (i->_next != 0) {
            i = i->_next;
        }
        i->_next = (_joforth_dict_entry_t*)joforth->_allocator->_alloc(sizeof(_joforth_dict_entry_t));
        i = i->_next;
        i->_next = 0;
    }
    i->_key = key;
    i->_word = word; //<ZZZ: this *should* be copied, not stored
    return i;
}

static _joforth_dict_entry_t* _add_word(joforth_t* joforth, const char* word) {

    _joforth_dict_entry_t* i = _add_entry(joforth, word);
    i->_details = (_joforth_word_details_t*)joforth->_allocator->_alloc(sizeof(_joforth_word_details_t));    
    return i;
}

static uint8_t* _alloc(joforth_t* joforth, size_t bytes) {
    assert(joforth->_mp < (joforth->_memory_size - bytes));
    size_t mp = joforth->_mp;
    joforth->_mp += bytes;
    return joforth->_memory + mp;
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
    joforth_push_value(joforth, count*sizeof(joforth_value_t));
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

    joforth->_stack_size = joforth->_stack_size ? joforth->_stack_size : JOFORTH_DEFAULT_STACK_SIZE;
    joforth->_stack = (joforth_value_t*)joforth->_allocator->_alloc(joforth->_stack_size * sizeof(joforth_value_t));

    joforth->_rstack_size = joforth->_rstack_size ? joforth->_rstack_size : JOFORTH_DEFAULT_RSTACK_SIZE;
    joforth->_rstack = (_joforth_rstack_entry_t*)joforth->_allocator->_alloc(joforth->_rstack_size * sizeof(_joforth_rstack_entry_t));

    joforth->_memory_size = joforth->_memory_size ? joforth->_memory_size : JOFORTH_DEFAULT_MEMORY_SIZE;
    joforth->_memory = (uint8_t*)joforth->_allocator->_alloc(joforth->_memory_size);

    joforth->_dict = (_joforth_dict_entry_t*)joforth->_allocator->_alloc(JOFORTH_DICT_BUCKETS * sizeof(_joforth_dict_entry_t));
    memset(joforth->_dict, 0, JOFORTH_DICT_BUCKETS * sizeof(_joforth_dict_entry_t));

    // empty stacks
    joforth->_sp = joforth->_stack_size - 1;
    joforth->_rp = joforth->_rstack_size - 1;
    // full heap
    joforth->_mp = 0;

    // start with decimal
    joforth->_base = 10;

    // add built-in handlers
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

    // add special words
    _joforth_dict_entry_t* entry = _add_word(joforth, "create");
    entry->_details->_type = kWordType_End | kWordType_Prefix;    
    entry->_details->_rep._handler = _create;
    entry->_details->_depth = 1;
}

void    joforth_destroy(joforth_t* joforth) {
    joforth->_allocator->_free(joforth->_stack);
    joforth->_allocator->_free(joforth->_rstack);
    joforth->_allocator->_free(joforth->_memory);
    if (joforth->_dict) {
        for (size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t* bucket = joforth->_dict + i;
            bucket = bucket->_next;
            while (bucket) {
                _joforth_dict_entry_t* j = bucket;
                bucket = bucket->_next;
                joforth->_allocator->_free(j);
            }
        }
        joforth->_allocator->_free(joforth->_dict);
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
    while (word[0] && word[0] != ' ') {
        buffer[wp++] = (char)tolower(*word++);
        if (wp == buffer_size) {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return 0;
        }
    }
    buffer[wp] = 0;
    *wp_ = wp;
    return word;
}

bool    joforth_eval_word(joforth_t* joforth, const char* word) {

    if(_JO_FAILED(joforth->_status))
        return false;

    while (word[0] && word[0] == ' ') ++word;
    if (word[0] == 0) {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
        return false;
    }

    // or else interpreting
    bool compiling = (word[0] == ':');

    char buffer[JOFORTH_MAX_WORD_LENGTH];
    size_t wp;
    _joforth_word_details_t* details = 0;

    if (compiling) {
        // COMPILE 
        word++;

        // we need to count the words to allocate space for them in the dictionary
        size_t words = _count_words(word);
        // identifier and at least one other word
        if (words < 2) {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return false;
        }

        // minus identifier
        --words;

        // the first word is the identifier
        word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp);
        joforth_word_key_t key = pearson_hash(buffer);
        _joforth_dict_entry_t* entry = _find_word(joforth, key);
        if (entry) {
            // already exists
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return false;
        }
        entry = _add_entry(joforth, buffer);
        // allocate a array for the sequence of words we need
        details = entry->_details = (_joforth_word_details_t*)joforth->_allocator->_alloc(words * sizeof(_joforth_word_details_t));
    }

    word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp);
    if (!word || _JO_FAILED(joforth->_status)) {
        return false;
    }

    size_t word_count = 1;
    size_t req_word_count = 0;

    do {
        if (wp) {
            if (compiling && buffer[0] == ';') {
                // end of word sequence
                details->_type |= kWordType_End;
                break;
            }

            joforth_word_key_t key = pearson_hash(buffer);
            _joforth_dict_entry_t* entry = _find_word(joforth, key);

            if (entry) {
                if (compiling) {
                    details->_type = kWordType_Word;
                    details->_rep._word = entry;
                }
                else {
                    details = entry->_details;
                    // process the words associated with one entry
                    while (1) {

                        // execute word(s)
                        switch (details->_type & kWordType_TypeMask) {
                        case kWordType_Handler:
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
                            break;
                        case kWordType_Value:
                            joforth_push_value(joforth, details->_rep._value);
                            break;
                        case kWordType_Word:
                        {
                            // return entry on rstack (unless this is the last one)
                            if ((details->_type & kWordType_End) == 0) {
                                _push_rstack(joforth, entry, details + 1);
                            }
                            // switch to a different word
                            entry = details->_rep._word;
                            details = details->_rep._word->_details;
                            continue;
                        }
                        case kWordType_Prefix:
                        {
                            // words such as CREATE and VARIABLE which don't take their argument from the 
                            // current stack, but rather from the next N values pushed on it.
                            // this is a bit of an "internal" hack, there must be a more elegant solution to this...
                            _push_rstack(joforth, entry, details);
                            req_word_count = details->_depth;
                            word_count = 0;
                        }       
                        default:;
                        }
                        
                        if (details->_type & kWordType_Prefix) {
                            // break out of the inner word processing loop
                            break;
                        }

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
            else {
                joforth_value_t value = _str_to_value(joforth, buffer);
                if (_JO_FAILED(joforth->_status)) {
                    // we'll just have to store this "as-is" and hope for the best
                    uint8_t* memory = _alloc(joforth, wp + 1);
                    memcpy(memory, buffer, wp + 1);
                    //NOTE: we're storing the address here
                    joforth_push_value(joforth, (joforth_value_t)memory);
                    joforth->_status = _JO_STATUS_SUCCESS;
                }
                else {
                    if (compiling) {
                        // a number; we add this to the word sequence as an immediate value
                        details->_type = kWordType_Value;
                        details->_rep._value = value;
                    }
                    else {
                        joforth_push_value(joforth, value);
                    }
                }
            }

            if (compiling) {
                ++details;
            }

            word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp);
            ++word_count;

            if (req_word_count && word_count > req_word_count) {
                // one of the prefix words can be evaluated and 
                // we expect a prefix word to be on the rstack at this point and we'll execute it directly
                assert( !_rstack_is_empty(joforth) );
                _joforth_rstack_entry_t i = _pop_rstack(joforth);
                entry = i._entry;
                details = i._details;
                assert( details->_type & kWordType_Prefix );
                details->_rep._handler(joforth);
                if (_JO_FAILED(joforth->_status)) {
                    return false;
                }
                req_word_count = 0;
            }
        }
    } while (wp);

    // we shouldn't have any left overs here....
    assert(_rstack_is_empty(joforth));

    return true;
}

void    joforth_dump_dict(joforth_t* joforth) {
    printf("joforth dictionary info:\n");
    if (joforth->_dict) {
        for (size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t* entry = joforth->_dict + i;
            while (entry) {
                if (entry->_key) {
                    printf("\tentry: key 0x%x, word \"%s\", takes %zu parameters\n", entry->_key, entry->_word, entry->_details->_depth);
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
