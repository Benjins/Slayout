#ifndef SLAYOUT_GFX_H
#define SLAYOUT_GFX_H

#pragma once

typedef struct {
	int width;
	int height;
	int* data;
} BitmapData;

#if defined(_MSC_VER)
#pragma pack(1)
typedef struct {
#else
typedef struct __attribute((packed))__{
#endif
	short fileTag;
	int fileSize;
	short reservedA;
	short reservedB;
	int imageDataOffset;

	int headerSize;
	int imageWidth;
	int imageHeight;
	short numColorPlanes;
	short bitDepth;
	int compressionMethod;
	int imageDataSize;
	int horizontalResolution;
	int verticalResolution;
	int numPaletteColors;
	int numImportantColors;
} BitMapHeader;
#if defined(_MSC_VER)
#pragma pack()
#endif

void BlitBitmap(BitmapData bitmap, int x, int y, int w, int h, BitmapData img);

BitmapData LoadBMPFromFile(const char* fileName);

void WriteBMPToFile(BitmapData data, const char* fileName);

void DrawLine(BitmapData bitmap, int x0, int y0, int x1, int y1, int thickness, int col);

#endif