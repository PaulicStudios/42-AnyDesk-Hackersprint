#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

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
	for (u8 i = 0; i < 8; i++) {
		struct bgr_pixel pixel = get_pixel(file_content, header, row + i, col);
		if (pixel.b != 127 || pixel.g != 188 || pixel.r != 217)
			return 0;
	}
	for (u8 i = 0; i < 7; i++) {
		struct bgr_pixel pixel = get_pixel(file_content, header, row + 7, col + i);
		if (pixel.b != 127 || pixel.g != 188 || pixel.r != 217)
			return 0;
	}
	return 1;
}

void decode_file(struct file_content *file_content, struct bmp_header *header)
{
	char header_found = 0;
	u16 strLength = 0;

	for (u32 row = 0; row < header->height; row += 1)
	{
		for (u32 col = 0; col < header->width; col += 1)
		{
			if (valid_header(file_content, header, row, col))
			{
				if (!header_found)
				{
					// printf("Found at %i %i\n", row, col);
					// file_content->data[pixel_index] = (u8) 255;
					// file_content->data[pixel_index + 1] = (u8) 0;
					// file_content->data[pixel_index + 2] = (u8) 0;

					struct bgr_pixel lenght_pixel = get_pixel(file_content, header, row + 7, col + 7);
					strLength = lenght_pixel.b + lenght_pixel.r;
					// printf("Lenght: %i\n", strLength);
					header_found = 1;

					for (u16 i = 0; i < strLength / 3 + 1; i += 1) {
						struct bgr_pixel char_pixel = get_pixel(file_content, header, row + 5 - (i / 6), col + 2 + (i % 6));

						u8 remaining = (strLength - (i * 3));

						if (remaining == 1)
							write(STDOUT_FILENO, (char *) &(char_pixel.b), 1);
						else if (remaining == 2)
						{
							write(STDOUT_FILENO, (char *) &(char_pixel.b), 1);
							write(STDOUT_FILENO, (char *) &(char_pixel.g), 1);
						}
						else if (remaining >= 3)
						{
							write(STDOUT_FILENO, (char *) &(char_pixel.b), 1);
							write(STDOUT_FILENO, (char *) &(char_pixel.g), 1);
							write(STDOUT_FILENO, (char *) &(char_pixel.r), 1);
						}
					}
					return;
				}
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
