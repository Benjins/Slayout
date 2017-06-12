#include "slayout_gfx.h"

#include <stdio.h>
#include <stdlib.h>

BitmapData LoadBMPFromFile(const char* fileName) {
	FILE* bmpFile = fopen(fileName, "rb");

	if (bmpFile == NULL) {
		printf("\n\nError: could not open file '%s'.\n", fileName);

		BitmapData empty = { 0 };
		return empty;
	}

	fseek(bmpFile, 0, SEEK_END);
	size_t fileSize = ftell(bmpFile);
	fseek(bmpFile, 0, SEEK_SET);

	unsigned char* fileBuffer = (unsigned char*)malloc(fileSize);
	fread(fileBuffer, fileSize, 1, bmpFile);
	fclose(bmpFile);

	BitMapHeader* bmpInfo = (BitMapHeader*)fileBuffer;
	int width = bmpInfo->imageWidth;
	int height = bmpInfo->imageHeight;
	int* data = (int*)malloc(4 * width*height);

	unsigned char* fileCursor = fileBuffer + bmpInfo->imageDataOffset;

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			int pixel = fileCursor[0] | (fileCursor[1] << 8) | (fileCursor[2] << 16);
			fileCursor += 3;
			data[j*width + i] = pixel;
		}
	}

	free(fileBuffer);
	BitmapData bmpData = { width, height, data };
	return bmpData;
}

void WriteBMPToFile(BitmapData bmp, const char* fileName) {
	FILE* bmpFile = fopen(fileName, "wb");

	if (bmpFile == NULL) {
		printf("\n\nError: could not open file '%s' for writing.\n", fileName);
		return;
	}

	int actualWidth = (((3 * bmp.width) + 3) / 4) * 4;
	int imageDataSize = actualWidth*bmp.height;

	BitMapHeader header = { 0 };
	header.bitDepth = 24;
	header.imageWidth = bmp.width;
	header.imageHeight = bmp.height;
	header.imageDataOffset = sizeof(header);
	header.imageDataSize = imageDataSize;
	header.headerSize = 40;
	header.fileTag = 0x4D42;
	header.fileSize = sizeof(header) + imageDataSize;
	header.numColorPlanes = 1;

	fwrite(&header, 1, sizeof(header), bmpFile);

	unsigned char* dataBuffer = (unsigned char*)malloc(imageDataSize);

	for (int j = 0; j < bmp.height; j++) {
		for (int i = 0; i < bmp.width; i++) {
			int bufferIndex = j * actualWidth + i * 3;
			int bmpIndex = j * bmp.width + i;
			dataBuffer[bufferIndex] = (bmp.data[bmpIndex] & 0xFF);
			dataBuffer[bufferIndex + 1] = (bmp.data[bmpIndex] & 0xFF00) >> 8;
			dataBuffer[bufferIndex + 2] = (bmp.data[bmpIndex] & 0xFF0000) >> 16;
		}
	}

	fwrite(dataBuffer, 1, imageDataSize, bmpFile);

	fclose(bmpFile);

	free(dataBuffer);
}

void BlitBitmap(BitmapData bitmap, int x, int y, int w, int h, BitmapData img) {
	int x1 = x;
	int y1 = bitmap.height - y - h;
	int x2 = x + w;
	int y2 = bitmap.height - y;

	if (x1 < 0) { x1 = 0; }
	if (y1 < 0) { y1 = 0; }
	if (x2 >= bitmap.width) { x2 = bitmap.width - 1; }
	if (y2 < 0) { y2 = 0; }
	if (y2 >= bitmap.height) { y2 = bitmap.height - 1; }

	float heightRatio = 0.0f;
	for (int j = y1; j < y2; j++, heightRatio += 1.0f / (h)) {
		float widthRatio = 0.0f;
		for (int i = x1; i < x2; i++, widthRatio += 1.0f / (w)) {
			int idx = j*bitmap.width + i;

			int spriteX = widthRatio * img.width;
			int spriteY = heightRatio * img.height;
			int spriteIdx = spriteY*img.width + spriteX;

			bitmap.data[idx] = img.data[spriteIdx];
		}
	}
}