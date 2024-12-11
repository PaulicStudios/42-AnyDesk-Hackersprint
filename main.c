#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <immintrin.h>

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)

#pragma pack(1)
struct bmp_header
{
	// Note: header
	i8  signature[2]; // should equal to "BM"
	u32 file_size;
	u32 unused_0;
	u32 data_offset;

	// Note: info header
	u32 info_header_size;
	u32 width; // in px
	u32 height; // in px
	u16 number_of_planes; // should be 1
	u16 bit_per_pixel; // 1, 4, 8, 16, 24 or 32
	u32 compression_type; // should be 0
	u32 compressed_image_size; // should be 0
	// Note: there are more stuff there but it is not important here
};

struct file_content
{
	i8*   data;
	u32   size;
};

struct file_content   read_entire_file(char* filename)
{
	char* file_data = 0;
	unsigned long	file_size = 0;
	int input_file_fd = open(filename, O_RDONLY);
	if (input_file_fd >= 0)
	{
		struct stat input_file_stat = {0};
		stat(filename, &input_file_stat);
		file_size = input_file_stat.st_size;
		file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
		close(input_file_fd);
	}
	return (struct file_content){file_data, file_size};
}

struct bgr_pixel
{
	u8 b;
	u8 g;
	u8 r;
};

u32 get_pixel_index(struct bmp_header *header, u32 row, u32 col)
{
	return row * header->width * 4 + col * 4 + header->data_offset;
}

struct bgr_pixel get_pixel(struct file_content *file_content, struct bmp_header *header, u32 row, u32 col)
{
	u32 pixel_index = get_pixel_index(header, row, col);
	return (struct bgr_pixel){file_content->data[pixel_index], file_content->data[pixel_index + 1], file_content->data[pixel_index + 2]};
}

u8 valid_header(struct file_content *file_content, struct bmp_header *header, u32 row, u32 col)
{
	__m128i target = _mm_setr_epi8(
        127, (char)188, (char)217, 0,  // First pixel (-1 for alpha to ignore)
        127, (char)188, (char)217, 0,  // Second pixel
        127, (char)188, (char)217, 0,  // Third pixel
        127, (char)188, (char)217, 0   // Fourth pixel
    );

    for (u8 i = 0; i < 8; i += 1) {
        u32 idx = get_pixel_index(header, row + i, col);
        __m128i pixels = _mm_loadu_si128((__m128i*)&file_content->data[idx]);

        // Compare all bytes at once
        __m128i cmp = _mm_cmpeq_epi8(pixels, target);
        int mask = _mm_movemask_epi8(cmp);

        if ((mask & 0x7) != 0x7)
            return 0;
    }

	u32 idx = get_pixel_index(header, row + 7, col);
	__m128i hpixels1 = _mm_loadu_si128((__m128i*)&file_content->data[idx]);
	__m128i hpixels2 = _mm_loadu_si128((__m128i*)&file_content->data[idx + 8]);
	__m128i cmp1 = _mm_cmpeq_epi8(hpixels1, target);
	__m128i cmp2 = _mm_cmpeq_epi8(hpixels2, target);
	int mask1 = _mm_movemask_epi8(cmp1);
	int mask2 = _mm_movemask_epi8(cmp2);

	if ((mask1 & 0x7777) != 0x7777 || (mask2 & 0x7777) != 0x7777)
		return 0;

	return 1;
}

void decode_file(struct file_content *file_content, struct bmp_header *header)
{
	for (u32 row = 0; row < header->height - 8; row += 1)
	{
		for (u32 col = 0; col < header->width - 8; col += 1)
		{
			if (valid_header(file_content, header, row, col))
			{
				// printf("Found at %i %i\n", row, col);
				// file_content->data[pixel_index] = (u8) 255;
				// file_content->data[pixel_index + 1] = (u8) 0;
				// file_content->data[pixel_index + 2] = (u8) 0;

				struct bgr_pixel lenght_pixel = get_pixel(file_content, header, row + 7, col + 7);
				const u16 strLength = lenght_pixel.b + lenght_pixel.r;
				// printf("Lenght: %i\n", strLength);

				char output[strLength];
				for (u16 i = 0; i < strLength / 3 + 1; i += 1) {
					const u32 char_pixel_ind = get_pixel_index(header, row + 5 - (i / 6), col + 2 + (i % 6));

					__m128i pixel = _mm_loadu_si32(&file_content->data[char_pixel_ind]);
					_mm_storeu_si32(&output[i * 3], pixel);
				}
				write(STDOUT_FILENO, output, strLength);
				return;
			}
		}
	}
	PRINT_ERROR("No header found\n");
}

void export_file(struct file_content *file_content)
{
	int output_file_fd = open("output.bmp", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (output_file_fd < 0)
	{
		PRINT_ERROR("Failed to create output file\n");
		return;
	}
	write(output_file_fd, file_content->data, file_content->size);
	close(output_file_fd);
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		PRINT_ERROR("Usage: decode <input_filename>\n");
		return 1;
	}
	struct file_content file_content = read_entire_file(argv[1]);
	if (file_content.data == NULL)
	{
		PRINT_ERROR("Failed to read file\n");
		return 1;
	}
	struct bmp_header* header = (struct bmp_header*) file_content.data;
	// printf("signature: %.2s\nfile_size: %u\ndata_offset: %u\ninfo_header_size: %u\nwidth: %u\nheight: %u\nplanes: %i\nbit_per_px: %i\ncompression_type: %u\ncompression_size: %u\n", header->signature, header->file_size, header->data_offset, header->info_header_size, header->width, header->height, header->number_of_planes, header->bit_per_pixel, header->compression_type, header->compressed_image_size);

	decode_file(&file_content, header);
	// export_file(&file_content);
	return 0;
}
