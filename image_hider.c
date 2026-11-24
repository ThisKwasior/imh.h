#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STBI_NO_JPEG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define IMH_IMPLEMENTATION
#include "imh.h"

#define PNG_COLORTYPE_ALPHA (0x06)

/*
    Structures
*/
typedef struct PNG_INFO PNG_INFO;
struct PNG_INFO
{
    uint32_t width;
    uint32_t height;
    uint8_t color_type; /* We only care for 0x06 (RGBA) */
};

/*
    Function definitions
*/
void encode(const char* input_path, const char* output_path, const char* data_path, const uint8_t force_alpha);
void decode(const char* input_path, const char* output_path);

void print_usage(const char* program_path);
int64_t get_file_size(const char* file_path);
uint8_t does_file_exist(const char* file_path);

PNG_INFO png_check(const char* file_path);

/*
    Implementation
*/
void encode(const char* input_path, const char* output_path, const char* data_path, const uint8_t force_alpha)
{
    PNG_INFO png_info = png_check(input_path);
    
    if((png_info.width && png_info.height && png_info.color_type) == 0)
    {
        printf("Malformed PNG (%ux%u %u)", png_info.width, png_info.height,
                                           png_info.color_type);
        return;
    }
    
    printf("Encoding!\n");
    printf("Input    : %s\n", input_path);
    printf("Output   : %s\n", output_path);
    printf("Data     : %s\n", data_path);
    
    if((png_info.color_type == PNG_COLORTYPE_ALPHA) || force_alpha)
        printf("Alpha    : %s\n", force_alpha ? "Overwriting" : "Keeping");
    
    uint8_t channels = (png_info.color_type == PNG_COLORTYPE_ALPHA) ? 4 : 3;
    uint8_t imh_fmt = (png_info.color_type == PNG_COLORTYPE_ALPHA)
                      ? IMH_FMT_RGBA_NO_A
                      : IMH_FMT_LINEAR;
                      
    if(force_alpha)
    {
        channels = 4;
        imh_fmt = IMH_FMT_LINEAR;
    }
    
    int x,y,n;
    unsigned char* img_data = stbi_load(input_path, &x, &y, &n, channels);
    const uint32_t capacity = imh__get_image_capacity(x*y*channels, imh_fmt);
    const int64_t data_size = get_file_size(data_path);
    
    printf("PNG size : %u x %u\n", x, y);
    printf("PNG comp : %u\n", channels);
    printf("Capacity : %u Bytes\n", capacity);
    printf("Data     : %llu Bytes\n", data_size);
    
    if((data_size+IMH_HEADER_SIZE) > capacity)
    {
        printf("Image is too small to encode the file.\n");
        return;
    }

    /* Read the data file to memory */
    uint8_t* data_to_encode = (uint8_t*)malloc(data_size);
    
    if(data_to_encode == NULL)
    {
        printf("Couldn't allocate the data buffer\n");
        return;
    }

    FILE* fdata = fopen(data_path, "rb");

    if(fdata == NULL)
    {
        printf("Couldn't open the data file for reading.\n");
        return;
    }
    
    if(fread(data_to_encode, data_size, 1, fdata) != 1)
    {
        printf("Couldn't read data file contents.\n");
        return;
    }

    fclose(fdata);
    
    /* Encoding */
    imh_encode_unsafe(img_data, x*y*channels, data_to_encode, data_size, imh_fmt);
    
    /* Saving */
    stbi_write_png(output_path, x, y, channels, img_data, x*channels);
    
    /* Cleanup */
    free(img_data);
    free(data_to_encode);

}

void decode(const char* input_path, const char* output_path)
{
    PNG_INFO png_info = png_check(input_path);
    
    if((png_info.width && png_info.height && png_info.color_type) == 0)
    {
        printf("Malformed PNG (%ux%u %u)", png_info.width, png_info.height,
                                           png_info.color_type);
        return;
    }
    
    printf("Decoding!\n");
    printf("Input    : %s\n", input_path);
    printf("Output   : %s\n", output_path);
    
    const uint8_t channels = (png_info.color_type == PNG_COLORTYPE_ALPHA) ? 4 : 3;

    int x,y,n;
    unsigned char* img_data = stbi_load(input_path, &x, &y, &n, channels);
    
    uint32_t data_size = 0;
    uint8_t* decoded_data = imh_decode(img_data, x*y*channels, &data_size);
    
    printf("Data     : %u Bytes\n", data_size);
    
    if((decoded_data == NULL) && (data_size == 0))
    {
        printf("Couldn't decode data.\n");
        printf("Image might not have encoded data.\n");
        return;
    }
    
    /* Write to file */
    FILE* fdata = fopen(output_path, "wb");
    
    if(fdata == NULL)
    {
        printf("Couldn't open the data file for writing.\n");
        return;
    }
    
    if(fwrite(decoded_data, data_size, 1, fdata) != 1)
    {
        printf("Couldn't write the decoded data to file.\n");
        return;
    }
    
    /* Cleanup */
    fclose(fdata);
    free(decoded_data);
    free(img_data);
}

void print_usage(const char* program_path)
{
    printf("Image Hider - @ThisKwasior\n\n");
    
    printf("Usage:\n");
    printf("  To encode data in a PNG (doesn't touch alpha):\n");
    printf("    %s <image.png> <output.png> <file to encode>\n", program_path);
    printf("\n");
    printf("  To force encoding in alpha channel:\n");
    printf("    %s <image.png> <output.png> <file to encode> fa\n", program_path);
    printf("\n");
    printf("  To decode data from a PNG:\n");
    printf("    %s <embedded_data.png> <output.bin>\n", program_path);
}

int64_t get_file_size(const char* file_path)
{
    int64_t size = 0;
    FILE* f = fopen(file_path, "rb");
    
    if(f)
    {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
    }
    
    return size;
}

uint8_t does_file_exist(const char* file_path)
{
    FILE* f = fopen(file_path, "rb");
    
    if(f)
    {
        fclose(f);
        return 1;
    }
    
    return 0;
}

PNG_INFO png_check(const char* file_path)
{
    /* header + IHDR */
    uint8_t png_data_buf[29];
    
    PNG_INFO png = {0};
    FILE* fpng = fopen(file_path, "rb");
    
    if(fpng)
    {
        /* Only do stuff if we could read 29 bytes (header + IHDR) */
        if(fread(&png_data_buf[0], 29, 1, fpng) == 1)
        {
            fclose(fpng);
            
            /* Check if PNG and IHDR are in place */
            if((strncmp((char*)&png_data_buf[1], "PNG", 3) == 0)
               && (strncmp((char*)&png_data_buf[12], "IHDR", 4) == 0))
            {
               /* Finally read some data */
               png.width = imh__read_u32be((void*)&png_data_buf[16]);
               png.height = imh__read_u32be((void*)&png_data_buf[20]);
               png.color_type = png_data_buf[25];
            }
        }
    }
    
    return png;
}

/*
    Entry point
*/
int main(int argc, char** argv)
{
    switch(argc)
    {
        /* Decode */
        case 3: 
            if(does_file_exist(argv[1]))
                decode(argv[1], argv[2]);
            else
                goto print_usage;
            break;
        
        /* Encode - don't touch alpha */
        case 4: 
            if(does_file_exist(argv[1]) && does_file_exist(argv[3]))
                encode(argv[1], argv[2], argv[3], 0);
            else
                goto print_usage;
            break;
        
        /* Encode - write over alpha */
        case 5: 
            if((argv[4][0] == 'f') && (argv[4][1] == 'a'))
            {
                if(does_file_exist(argv[1]) && does_file_exist(argv[3]))
                    encode(argv[1], argv[2], argv[3], 1);
                else
                    goto print_usage;
            }
            else
            {
                goto print_usage;
            }
            break;
        default:
print_usage:
            print_usage(argv[0]);
    }

    return 0;
}