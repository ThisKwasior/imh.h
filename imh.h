#pragma once

/*
    imh.h - Image Hider API
    
    To enable the implementation, define
    
       #define IMH_IMPLEMENTATION
       
    before #including.
*/

/*
    ABOUT:

    "Hides" arbitrary data by replacing the least-significant bit 
    of an image channel with next bit of user data.
    Basically I'm exploiting that human eye doesn't see
    much variance in black tones.
    
    Header is encoded as u64be, so some shenanigans are required.
    Last (first?) bit is the format (changes how payload is read), 
    format is 8 bits, so the MsB is set - 7 bits are reserved.
    Next 32 bits is the size of the payload (in u32be).
    Next 23 bits are reserved.
    LsB is discarded.
    
    Format      : 10000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    Format p2   : 01111111 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    Payload size: 00000000 11111111 11111111 11111111 11111111 00000000 00000000 00000000
    Reserved    : 00000000 00000000 00000000 00000000 00000000 11111111 11111111 11111110
    Discarded   : 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000001
    
    After that we get the payload.
*/

/*
    Some ideas for future development:
    - Make the bit selection configurable
    - Support word sizes other than 8 bits
    - Checksum?
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
    Includes
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
    Defines and data
*/

/* imh_fmt in the functions */
#define IMH_FMT_LINEAR          (0x00)    /* Reading bits as we go */
#define IMH_FMT_RGBA_NO_A       (0x80)    /* Skips every 4th byte of input data */

#define IMH_FMT_LINEAR_START    (64)
#define IMH_FMT_RGBA_NO_A_START (84)

#define IMH_HEADER_SIZE         (sizeof(uint64_t))
#define IMH_FMT_MASK            ((uint64_t)0xFF00000000000000)
#define IMH_FMT_SHIFT           (56)
#define IMH_SIZE_MASK           ((uint64_t)0x00FFFFFFFF000000)
#define IMH_SIZE_SHIFT          (24)

#define IMH_BYTE_MASK           ((uint8_t)0x01)
#define IMH_BYTE_MASK_INV       ((uint8_t)0xFE)
#define IMH_8BYTES_MASK         ((uint64_t)0x0101010101010101)
#define IMH_8BYTES_MASK_INV     ((uint64_t)0xFEFEFEFEFEFEFEFE)

/*
    Helper functions
*/

/* Should work on GCC, Clang and MSVC */
static inline uint32_t imh__read_u32be(const void* data)
{
    const uint32_t buf = *(uint32_t*)data;
    
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) ||  \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
    return (uint32_t)((buf&0xFF000000)>>24 | (buf&0x00FF0000)>>8 |
                      (buf&0x0000FF00)<<8 | (buf&0x000000FF)<<24);
#else
    return buf;
#endif
}

static inline uint64_t imh__read_u64be(const void* data)
{
    const uint64_t buf = *(uint64_t*)data;
    
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) ||  \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
	return (uint64_t)((buf&0xFF00000000000000)>>56|(buf&0x00FF000000000000)>>40|
		              (buf&0x0000FF0000000000)>>24|(buf&0x000000FF00000000)>>8 |
		              (buf&0x00000000FF000000)<<8 |(buf&0x0000000000FF0000)<<24|
		              (buf&0x000000000000FF00)<<40|(buf&0x00000000000000FF)<<56);
#else
    return buf;
#endif
}

/*
    Encodes a byte in the u64be by discarding least-significant bit in each byte.
*/
static inline uint64_t imh__byte_to_u64(const uint64_t byte, uint64_t data)
{
    data &= IMH_8BYTES_MASK_INV;
    
    data |= ((byte>>7)&1);
    data |= ((byte>>6)&1)<<8;
    data |= ((byte>>5)&1)<<16;
    data |= ((byte>>4)&1)<<24;
    data |= ((byte>>3)&1)<<32;
    data |= ((byte>>2)&1)<<40;
    data |= ((byte>>1)&1)<<48;
    data |= ((byte   )&1)<<56;
    
    return data;
}

/*
    Masks input u64be value and decodes a byte.
*/
static inline uint8_t imh__u64_to_byte(uint64_t value)
{
    value &= IMH_8BYTES_MASK;

    return (value>>56) | (value>>47) | (value>>38) |
           (value>>29) | (value>>20) | (value>>11) |
           (value>>2)  | (value<<7);
}

/*
    Capacity of the image in bytes.
*/
static inline uint32_t imh__get_image_capacity(const uint32_t img_size, const uint8_t imh_fmt)
{
    if(imh_fmt == IMH_FMT_RGBA_NO_A)
    {
        return (((img_size/4)*3) + (img_size%4))/8;
    }
    
    return img_size/8;
}

/*
    Reads bits from u64 values to the buffer as bytes
*/
static inline void imh__read_bytes_linear(uint8_t* buffer, const uint32_t length, uint64_t* data)
{
    for(uint32_t i = 0; i != length; ++i)
    {
        buffer[i] = imh__u64_to_byte(imh__read_u64be(&data[i]));
    }
}

/*
    Writes data as bits to the buffer as whole u64 values
*/
static inline void imh__write_bytes_linear(uint64_t* buffer, const uint32_t length, const uint8_t* data)
{
    for(uint32_t i = 0; i != length; ++i)
    {
        uint64_t encoded = imh__byte_to_u64(data[i], imh__read_u64be(&buffer[i]));
        buffer[i] = imh__read_u64be(&encoded);
    }
}

/*
    Reads data as bits to the buffer while skipping every 4th byte (alpha channel).
    It expects you wanna read data from 1st byte (red channel) in the array.
*/
void imh__read_bytes_rgba_no_a(uint8_t* buffer, const uint32_t bits_to_read, const uint8_t* data)
{
    uint32_t iter = 0;
    uint8_t* dst = &buffer[0];
    const uint8_t* src = &data[0];
    
    while(iter != bits_to_read)
    {
        const uint32_t pos = (src - buffer);
        const uint8_t is_alpha = ((pos%4)==3) ? 1 : 0;
        
        if(is_alpha)
        {
            src += 1;
        }
        else
        {
            const uint8_t cur_bit_shift = 7-(iter%8);
            const uint8_t bit = ((*src)&IMH_BYTE_MASK)<<cur_bit_shift;
            
            (*dst) |= bit;
            
            iter += 1;
            src += 1;
            
            if(cur_bit_shift == 0)
            {
                dst += 1;
            }
        }
    }
}

/*
    Writes data as bits to the buffer while skipping every 4th byte (alpha channel).
    It expects you wanna write to the buffer from 1st byte (red channel) in the array.
*/
void imh__write_bytes_rgba_no_a(uint8_t* buffer, const uint32_t length_in_bits, const uint8_t* data)
{
    uint32_t iter = 0;
    uint8_t* dst = &buffer[0];
    const uint8_t* src = &data[0];
    
    while(iter != length_in_bits)
    {
        const uint32_t pos = (dst - buffer);
        const uint8_t is_alpha = ((pos%4)==3) ? 1 : 0;

        if(is_alpha)
        {
            dst += 1;
        }
        else
        {
            const uint8_t cur_bit_shift = 7-(iter%8);
            const uint8_t bit = ((*src)>>cur_bit_shift)&IMH_BYTE_MASK;
            
            (*dst) = ((*dst)&IMH_BYTE_MASK_INV) | bit;
            
            iter += 1;
            dst += 1;
            
            if(cur_bit_shift == 0)
            {
                src += 1;
            }
        }
    }
}

/*
    Public API Definitions
*/

/*
    Encodes data in the image.
    
    Returns a pointer to copied image with encoded data if successful.
    Otherwise NULL.
*/
uint8_t* imh_encode(const uint8_t* img_data, const uint32_t img_size,
                    const uint8_t* data_ptr, const uint32_t data_size,
                    const uint8_t imh_fmt);

/*
    Encodes data in the image in-place.
    Modifies the `img_data` so it's destructive,
    but faster and uses less memory.
*/
void imh_encode_unsafe(uint8_t* img_data, const uint32_t img_size,
                       const uint8_t* data_ptr, const uint32_t data_size,
                       const uint8_t imh_fmt);

/*
    Decodes data from the image.
    
    Returns a pointer to data and sets `data_size`.
    Otherwise NULL and 0.
*/
uint8_t* imh_decode(const uint8_t* img_data, const uint32_t img_size, uint32_t* data_size);

/*
    Implementation
*/

#ifdef IMH_IMPLEMENTATION

uint8_t* imh_encode(const uint8_t* img_data, const uint32_t img_size,
                    const uint8_t* data_ptr, const uint32_t data_size,
                    const uint8_t imh_fmt)
{
    uint8_t* out_buffer = NULL;
    const uint32_t capacity = imh__get_image_capacity(img_size, imh_fmt);
    
    if((data_size+IMH_HEADER_SIZE) > capacity)
    {
        /* Not enough capacity to encode data */
        return NULL;
    }
    
    out_buffer = (uint8_t*)calloc(1, img_size);
    
    /* Alloc error */
    if(out_buffer == NULL)
    {
        return NULL;
    }
    
    memcpy(out_buffer, img_data, img_size);
    
    imh_encode_unsafe(out_buffer, img_size, data_ptr, data_size, imh_fmt);
    
    return out_buffer;
}

void imh_encode_unsafe(uint8_t* img_data, const uint32_t img_size,
                       const uint8_t* data_ptr, const uint32_t data_size,
                       const uint8_t imh_fmt)
{
    const uint32_t capacity = imh__get_image_capacity(img_size, imh_fmt);
    
    /* format+size */
    if((data_size+IMH_HEADER_SIZE) > capacity)
    {
        /* Not enough capacity to encode data */
        return;
    }
    
    /* Creating and writing a header */
    uint64_t header = (((uint64_t)imh_fmt)<<IMH_FMT_SHIFT) |
                      (((uint64_t)data_size)<<IMH_SIZE_SHIFT);
              
    header = imh__read_u64be(&header);

    switch(imh_fmt)
    {
        case IMH_FMT_LINEAR: 
            imh__write_bytes_linear((uint64_t*)img_data, 8, (uint8_t*)&header);
            break;
        case IMH_FMT_RGBA_NO_A:
            imh__write_bytes_rgba_no_a(img_data, 63, (uint8_t*)&header);
            break;
    }
    
    /* Writing data */
    switch(imh_fmt)
    {
        case IMH_FMT_LINEAR: 
            imh__write_bytes_linear((uint64_t*)&img_data[IMH_FMT_LINEAR_START],
                                    data_size, data_ptr);
            break;
        case IMH_FMT_RGBA_NO_A:
            imh__write_bytes_rgba_no_a(&img_data[IMH_FMT_RGBA_NO_A_START],
                                       data_size*8, data_ptr);
            break;
    }
}

uint8_t* imh_decode(const uint8_t* img_data, const uint32_t img_size, uint32_t* data_size)
{
    uint8_t fmt = (img_data[0]&IMH_BYTE_MASK)<<7;
    uint64_t header = 0;
    uint8_t* out_buffer = NULL;

    /* Reading header */
    switch(fmt)
    {
        case IMH_FMT_LINEAR: 
            imh__read_bytes_linear((uint8_t*)&header, 8, (uint64_t*)img_data);
            header = imh__read_u64be(&header);
            break;
        case IMH_FMT_RGBA_NO_A:
            imh__read_bytes_rgba_no_a((uint8_t*)&header, 63, img_data);
            header = imh__read_u64be(&header);
            break;
    }

    (*data_size) = (header&IMH_SIZE_MASK) >> IMH_SIZE_SHIFT;
    
    /* Check if capacity can actually hold the embedded data */
    if(((*data_size)+IMH_HEADER_SIZE) > img_size)
    {
        (*data_size) = 0;
        return NULL;
    }
    
    /* Allocing the buffer */
    out_buffer = (uint8_t*)calloc(1, (*data_size));
    
    /* Alloc error */
    if(out_buffer == NULL)
    {
        (*data_size) = 0;
        return NULL;
    }
    
    /* Reading the data */
    switch(fmt)
    {
        case IMH_FMT_LINEAR: 
            imh__read_bytes_linear(out_buffer, (*data_size), (uint64_t*)&img_data[IMH_FMT_LINEAR_START]);
            break;
        case IMH_FMT_RGBA_NO_A:
            imh__read_bytes_rgba_no_a(out_buffer, (*data_size)*8, &img_data[IMH_FMT_RGBA_NO_A_START]);
            break;
    }
    
    return out_buffer;
}

#endif /* IMH_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif