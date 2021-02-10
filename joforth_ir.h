#pragma once


#include <joforth.h>

typedef enum _joforth_ir {

    kIr_DefineWord = 1,          // ":"
    kIr_WordPtr,                 // followed by 64 bit pointer to a joforth_dict_t entry
    kIr_ValuePtr,                // followed by 64 bit pointer to a 0 terminated string
    kIr_Value,                   // followed by a 64 bit immediate value
    kIr_If,
    kIr_Else,
    kIr_Then,
    kIr_Begin,
    kIr_Until,
    kIr_While,
    kIr_Repeat,
    kIr_Do,
    kIr_Loop,
    kIr_EndDefineWord,    
    kIr_Dot,
    kIr_True,
    kIr_False,

} _joforth_ir_t;

typedef struct _joforth_keyword_lut_entry {
    const char*     _id;
    _joforth_ir_t   _ir;
} _joforth_keyword_lut_entry_t;

static const _joforth_keyword_lut_entry_t _joforth_keyword_lut[] = {
    { ._id = ";", ._ir = kIr_EndDefineWord },
    { ._id = "true", ._ir = kIr_True },
    { ._id = "false", ._ir = kIr_False },
    { ._id = "if", ._ir = kIr_If },
    { ._id = "else", ._ir = kIr_Else },
    { ._id = "then", ._ir = kIr_Then },
    { ._id = "begin", ._ir = kIr_Begin },
    { ._id = "until", ._ir = kIr_Until },
    { ._id = "while", ._ir = kIr_While },
    { ._id = "repeat", ._ir = kIr_Repeat },
    { ._id = "do", ._ir = kIr_Do },
    { ._id = "loop", ._ir = kIr_Loop },
};
static const size_t _joforth_keyword_lut_size = sizeof(_joforth_keyword_lut)/sizeof(_joforth_keyword_lut_entry_t);

typedef struct _joforth_ir_buffer {

    uint8_t   *     _buffer;
    size_t          _size;
    size_t          _irw;
    size_t          _irr;

} _joforth_ir_buffer_t;

static _JO_ALWAYS_INLINE void _ir_emit(_joforth_ir_buffer_t* buffer, _joforth_ir_t ir) {
    assert(buffer->_irw < buffer->_size);
    buffer->_buffer[buffer->_irw++] = (uint8_t)(ir & 0xff);
}

static _JO_ALWAYS_INLINE void _ir_emit_ptr(_joforth_ir_buffer_t* buffer, void* ptr) {
    assert(buffer->_irw <= (buffer->_size - sizeof(void*)));
    memcpy(buffer->_buffer + buffer->_irw, &ptr, sizeof(void*));
    buffer->_irw += sizeof(void*);
}

static _JO_ALWAYS_INLINE void _ir_emit_value(_joforth_ir_buffer_t* buffer, joforth_value_t value) {
    assert(buffer->_irw <= (buffer->_size - sizeof(value)));
    memcpy(buffer->_buffer + buffer->_irw, &value, sizeof(value));
    buffer->_irw += sizeof(value);
}

static _JO_ALWAYS_INLINE _joforth_ir_t _ir_peek(_joforth_ir_buffer_t* buffer) {
    assert(buffer->_irr < buffer->_irw);
    return (_joforth_ir_t)buffer->_buffer[buffer->_irr];
}

static _JO_ALWAYS_INLINE _joforth_ir_t _ir_consume(_joforth_ir_buffer_t* buffer) {
    assert(buffer->_irr < buffer->_irw);
    return (_joforth_ir_t)buffer->_buffer[buffer->_irr++];
}

static _JO_ALWAYS_INLINE void* _ir_consume_ptr(_joforth_ir_buffer_t* buffer) {
    assert(buffer->_irr <= (buffer->_irw - sizeof(void*)));
    void* ptr;
    memcpy(&ptr, buffer->_buffer + buffer->_irr, sizeof(void*));
    buffer->_irr += sizeof(void*);
    return ptr;
}

static _JO_ALWAYS_INLINE joforth_value_t _ir_consume_value(_joforth_ir_buffer_t* buffer) {
    assert(buffer->_irr <= (buffer->_irw - sizeof(joforth_value_t)));
    joforth_value_t value;
    memcpy(&value, buffer->_buffer + buffer->_irr, sizeof(value));
    buffer->_irr += sizeof(value);
    return value;
}

static _JO_ALWAYS_INLINE bool _ir_buffer_is_empty(_joforth_ir_buffer_t* buffer) {
    return buffer->_irr == buffer->_irw;
}
