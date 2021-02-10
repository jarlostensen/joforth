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

} _joforth_ir_t;

typedef struct _joforth_keyword_lut_entry {
    const char*     _id;
    _joforth_ir_t   _ir;
} _joforth_keyword_lut_entry_t;

static const _joforth_keyword_lut_entry_t _joforth_keyword_lut[] = {
    { ._id = ";", ._ir = kIr_EndDefineWord },
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
