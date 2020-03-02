//
// Created by faywong on 2020/2/29.
//

#ifndef SYNCFOLDER_SCIDOWN_MD_H
#define SYNCFOLDER_SCIDOWN_MD_H

#include <stdint.h>
#include <stddef.h>
extern "C" int md2html(const uint8_t* input_data, size_t input_size, uint8_t** output_data, size_t* output_size);
#endif //SYNCFOLDER_SCIDOWN_MD_H
