#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t get_bit(const uint8_t *data, unsigned int w, unsigned int x, unsigned int y, unsigned int plane)
{
	return (data[plane * w / 8 + 4 * w * y / 8 + x / 8] >> (7 - x % 8)) & 1;
}

static uint8_t get_byte(const uint8_t *data, unsigned int w, unsigned int x, unsigned int y)
{
	return    (get_bit(data, w, x, y, 0) << 0)
		| (get_bit(data, w, x, y, 1) << 1)
		| (get_bit(data, w, x, y, 2) << 2)
		| (get_bit(data, w, x, y, 3) << 3);
}

static void supaplex2gba(const uint8_t *in, unsigned int w, unsigned int x, unsigned int y, uint32_t *out)
{
	for (unsigned int v = 0; v < 8; ++v) {
		out[v] =
			  (get_byte(in, w, x + 0, y + v) << 0)
			| (get_byte(in, w, x + 1, y + v) << 4)
			| (get_byte(in, w, x + 2, y + v) << 8)
			| (get_byte(in, w, x + 3, y + v) << 12)
			| (get_byte(in, w, x + 4, y + v) << 16)
			| (get_byte(in, w, x + 5, y + v) << 20)
			| (get_byte(in, w, x + 6, y + v) << 24)
			| (get_byte(in, w, x + 7, y + v) << 28);
	}
}

static void dump_8x8_tile(const uint8_t *in, unsigned int w, unsigned int x, unsigned int y)
{
	uint32_t tile[8];
	supaplex2gba(in, w, x, y, tile);

	printf("\t");
	for (unsigned int i = 0; i < 8; ++i)
		printf("0x%08x, ", tile[i]);

	printf("\n");
}

static void dump_16x16_tile(const uint8_t *in, unsigned int w, unsigned int x, unsigned int y)
{
	/* This corresponds to the order in which 16x16 sprites use the
	 * tiles. */
	dump_8x8_tile(in, w, x + 0, y + 0);
	dump_8x8_tile(in, w, x + 8, y + 0);
	dump_8x8_tile(in, w, x + 0, y + 8);
	dump_8x8_tile(in, w, x + 8, y + 8);
}

static void dump_palette(const uint8_t *in)
{
	printf("\t");
	for (unsigned int i = 0; i < 16; ++i) {
		uint8_t r = in[4 * i + 0];
		uint8_t g = in[4 * i + 1];
		uint8_t b = in[4 * i + 2];

		uint16_t colour =
			  ((r * 31 / 15) << 0)
			| ((g * 31 / 15) << 5)
			| ((b * 31 / 15) << 10);

		printf("0x%04x, ", colour);
	}

	printf("\n");
}

static void convert_palette()
{
	int fd = open("data/PALETTES.DAT", O_RDONLY);
	if (fd == -1)
		throw std::runtime_error(strerror(errno));

	uint8_t data[256];
	int len = read(fd, data, sizeof(data));
	if (len == -1)
		throw std::runtime_error(strerror(errno));
	if (len == 0)
		throw std::runtime_error("eof");
	if (len != sizeof(data))
		throw std::runtime_error("wrong number of bytes read");

	printf("static const uint16_t palettes[] = {\n");
	dump_palette(data + 64);
	dump_palette(data + 0);
	dump_palette(data + 128);
	dump_palette(data + 192);
	printf("};\n");

	close(fd);
}

static void convert_fixed()
{
	int fd = open("data/FIXED.DAT", O_RDONLY);
	if (fd == -1)
		throw std::runtime_error(strerror(errno));

	uint8_t data[5120];
	int len = read(fd, data, sizeof(data));
	if (len == -1)
		throw std::runtime_error(strerror(errno));
	if (len == 0)
		throw std::runtime_error("eof");
	if (len != sizeof(data))
		throw std::runtime_error("wrong number of bytes read");

#if 0 /* Dump to console (not pretty) */
	unsigned int colors[] = { 31, 32, 33, 34, 35, 36, 37 };
	for (unsigned int tile = 0; tile < (640 / 16); ++tile) {
		for (unsigned int y = 0; y < 16; ++y) {
			for (unsigned int x = 0; x < 16; ++x) {
				uint8_t byte = get_byte(data, 640, 16 * tile + x, y);

				printf("\e[%dm%02x", colors[byte % 7], byte);
			}

			printf("\n");
		}
	}
#endif

	printf("static const uint32_t fixed[] = {\n");

	/* The order/numbering of these tiles correpond to "enum tile" */
	for (unsigned int i = 0; i < 11; ++i)
		dump_16x16_tile(data, 640, 16 * i, 0);

	/* Skip ports which we can create by mirroring */

	for (unsigned int i = 17; i < 25; ++i)
		dump_16x16_tile(data, 640, 16 * i, 0);

	/* Skip bug (we can reuse the base) */

	for (unsigned int i = 26; i < 40; ++i)
		dump_16x16_tile(data, 640, 16 * i, 0);

	printf("};\n");

	close(fd);
}

static void convert_moving()
{
	int moving_fd = open("data/MOVING.DAT", O_RDONLY);
	if (moving_fd == -1)
		throw std::runtime_error(strerror(errno));

	uint8_t moving_data[73920];
	int moving_len = read(moving_fd, moving_data, sizeof(moving_data));
	if (moving_len == -1)
		throw std::runtime_error(strerror(errno));
	if (moving_len == 0)
		throw std::runtime_error("eof");
	if (moving_len != sizeof(moving_data))
		throw std::runtime_error("wrong number of bytes read");

	int fixed_fd = open("data/FIXED.DAT", O_RDONLY);
	if (fixed_fd == -1)
		throw std::runtime_error(strerror(errno));

	uint8_t fixed_data[5120];
	int fixed_len = read(fixed_fd, fixed_data, sizeof(fixed_data));
	if (fixed_len == -1)
		throw std::runtime_error(strerror(errno));
	if (fixed_len == 0)
		throw std::runtime_error("eof");
	if (fixed_len != sizeof(fixed_data))
		throw std::runtime_error("wrong number of bytes read");

#if 0 /* Dump to console (not pretty) */
	unsigned int colors[] = { 31, 32, 33, 34, 35, 36, 37 };
	for (unsigned int tile = 0; tile < (640 / 16); ++tile) {
		for (unsigned int y = 0; y < 16; ++y) {
			for (unsigned int x = 0; x < 16; ++x) {
				uint8_t byte = get_byte(data, 640, 16 * tile + x, y);

				printf("\e[%dm%02x", colors[byte % 7], byte);
			}

			printf("\n");
		}
	}
#endif

	printf("static const uint32_t moving[] = {\n");

	/* Murphy moving left (sprites) */
	dump_16x16_tile(moving_data, 320,  1 * 16 -  2, 0 * 16);
	dump_16x16_tile(moving_data, 320,  5 * 16 -  6, 0 * 16);
	dump_16x16_tile(moving_data, 320,  9 * 16 - 10, 0 * 16);
	dump_16x16_tile(moving_data, 320, 13 * 16 - 14, 0 * 16);

	/* Murphy looking left (sprite) */
	dump_16x16_tile(moving_data, 320, 13 * 16, 1 * 16);

	/* Murphy pressing left (sprite) */
	dump_16x16_tile(moving_data, 320, 15 * 16, 1 * 16);

	/* Murphy leaving the exit (sprite) */
	for (unsigned int i = 0; i < 9; ++i)
		dump_16x16_tile(moving_data, 320, (10 + i) * 16, 4 * 16);

	/* Zonk */
	dump_16x16_tile(fixed_data, 640, 1 * 16, 0);

	/* Zonk rolling to the left */
	for (unsigned int i = 0; i < 5; ++i)
		dump_16x16_tile(moving_data, 320, 32 * i + 16 - 2 * i, 5 * 16 + 4);
	for (unsigned int i = 5; i < 8; ++i)
		dump_16x16_tile(moving_data, 320, 32 * i + 14 - 2 * i, 5 * 16 + 4);

	/* Circuit being zapped (sprites) */
	dump_16x16_tile(moving_data, 320, 16 * 16, 5 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 17 * 16, 5 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 18 * 16, 5 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 19 * 16, 5 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 16 * 16, 6 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 17 * 16, 6 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 18 * 16, 6 * 16 + 4);

	/* Infotron rolling to the left */
	dump_16x16_tile(moving_data, 320, 32 * 0 + 16, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 1 + 13, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 2 + 11, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 3 + 8, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 4 + 6, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 5 + 4, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 6 + 2, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 32 * 7 + 0, 10 * 16 + 4);

	/* Infotron being zapped (sprites) */
	dump_16x16_tile(moving_data, 320, 12 * 16, 9 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 13 * 16, 9 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 14 * 16, 9 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 15 * 16, 9 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 16 * 16, 9 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 17 * 16, 9 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 18 * 16, 9 * 16 + 4);

	/* Explosion */
	for (unsigned int i = 0; i < 7; ++i)
		dump_16x16_tile(moving_data, 320, i * 16, 12 * 16 + 4);

	/* Infotron exploding into existence */
	for (unsigned int i = 11; i < 15; ++i)
		dump_16x16_tile(moving_data, 320, i * 16, 12 * 16 + 4);

	/* Electron */
	dump_16x16_tile(moving_data, 320, 19 * 16, 6 * 16 + 4);
	for (unsigned int i = 16; i < 20; ++i)
		dump_16x16_tile(moving_data, 320, i * 16, 12 * 16 + 4);

	/* Floppy disk being zapped (sprites) */
	dump_16x16_tile(moving_data, 320, 17 * 16, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 18 * 16, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 19 * 16, 10 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 16 * 16, 11 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 17 * 16, 11 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 18 * 16, 11 * 16 + 4);

	/* Snik-snak moving left */
	dump_16x16_tile(moving_data, 320, 13 * 16 - 2, 14 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 15 * 16 - 4, 14 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 17 * 16 - 6, 14 * 16 + 4);
	dump_16x16_tile(moving_data, 320, 19 * 16 - 8, 14 * 16 + 4);

	/* Snik-snak moving up */
	dump_16x16_tile(moving_data, 320, 2 * 16, 26 * 16 + 8);
	dump_16x16_tile(moving_data, 320, 4 * 16, 26 * 16 + 8);
	dump_16x16_tile(moving_data, 320, 6 * 16, 26 * 16 + 8);
	dump_16x16_tile(moving_data, 320, 8 * 16, 26 * 16 + 8);

	/* Snik-snak turning */
	dump_16x16_tile(moving_data, 320, 4 * 16, 16 * 16 + 4);

	/* Unhappy murphy (sprite) */
	dump_16x16_tile(moving_data, 320, 18 * 16, 8 * 16 + 4);

	/* Cursor spark (sprites) */
	dump_8x8_tile(moving_data, 320, 0 * 8, 27 * 16 + 13);
	dump_8x8_tile(moving_data, 320, 1 * 8, 27 * 16 + 13);
	dump_8x8_tile(moving_data, 320, 2 * 8, 27 * 16 + 13);
	dump_8x8_tile(moving_data, 320, 3 * 8, 27 * 16 + 13);
	dump_8x8_tile(moving_data, 320, 0 * 8, 27 * 16 + 22);
	dump_8x8_tile(moving_data, 320, 1 * 8, 27 * 16 + 22);
	dump_8x8_tile(moving_data, 320, 2 * 8, 27 * 16 + 22);
	dump_8x8_tile(moving_data, 320, 3 * 8, 27 * 16 + 22);

#if 0 /* Whole file */
	for (unsigned int y = 64 + 2; y < 462; y += 16) {
		for (unsigned int x = 0; x < 320; x += 16) {
			dump_tile(moving_data, 320, x + 0, y + 0);
			dump_tile(moving_data, 320, x + 0, y + 8);
			dump_tile(moving_data, 320, x + 8, y + 0);
			dump_tile(moving_data, 320, x + 8, y + 8);
		}
	}
#endif

	printf("};\n");

	close(moving_fd);
	close(fixed_fd);
}

static void convert_level(const uint8_t *level)
{
	printf("\t{\n");

	printf("\t\t{\n");
	for (unsigned int y = 0; y < 24; ++y) {
		printf("\t\t\t{ ");

		for (unsigned int x = 0; x < 60; ++x)
			printf("0x%02x, ", level[60 * y + x]);

		printf(" },\n");
	}

	printf("\t\t},\n");

	/* Name */
	printf("\t\t{ ");
	for (unsigned int i = 0; i < 23; ++i)
		printf("0x%02x, ", level[1440 + 4 + 1 + 1 + i]);
	printf(" 0x00 },\n");

	/* XXX: Other properties */

	printf("\t},\n");
}

static void convert_levels()
{
	int fd = open("data/LEVELS.DAT", O_RDONLY);
	if (fd == -1)
		throw std::runtime_error(strerror(errno));

	uint8_t data[170496];
	int len = read(fd, data, sizeof(data));
	if (len == -1)
		throw std::runtime_error(strerror(errno));
	if (len == 0)
		throw std::runtime_error("eof");
	if (len != sizeof(data))
		throw std::runtime_error("wrong number of bytes read");

	printf("static struct {\n");
	printf("\tuint8_t field[24][60];\n");
	printf("\tuint8_t name[24];\n");
	printf("\tuint8_t nr_infotrons;\n");
	printf("} levels[] = {\n");

	for (unsigned int i = 0; i < 111; ++i)
		convert_level(data + 1536 * i);

	printf("};\n");

	close(fd);
}

int main(int argc, char *argv[])
{
	convert_palette();
	convert_fixed();
	convert_moving();
	convert_levels();

	return 0;
}
