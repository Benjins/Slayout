#include "../CppUtils/strings.h"
#include "../CppUtils/filesys.h"
#include "../CppUtils/stringmap.h"
#include "../CppUtils/disc_union.h"
#include "../CppUtils/vector.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../ext/stb_truetype.h"

#include "slayout_gfx.h"

enum LayoutTextJustification {
	LTJ_Left,
	LTJ_Right,
	LTJ_Centre,
	LTJ_FullJustify
};

struct LayoutTextStyle {
	SubString fontName;
	float scale;
	int color;
	LayoutTextJustification justification;
};

struct LayoutTextEventDrawText {
	SubString text;

	explicit LayoutTextEventDrawText(const SubString& _text) {
		text = _text;
	}
};

struct LayoutTextEventChangeFont {
	SubString fontName;

	explicit LayoutTextEventChangeFont(const SubString& _fontName) {
		fontName = _fontName;
	}
};

struct LayoutTextEventChangeJustify {
	LayoutTextJustification justification;
};

struct LayoutTextEventChangeScale{
	float scale;

	explicit LayoutTextEventChangeScale(float _scale) {
		scale = _scale;
	}
};

struct LayoutTextEventChangeColor {
	int color;

	explicit LayoutTextEventChangeColor(int _color) {
		color = _color;
	}
};

struct LayoutTextEventPopStyle {

};

#define DISC_MAC(mac) \
	mac(LayoutTextEventDrawText) \
	mac(LayoutTextEventChangeJustify) \
	mac(LayoutTextEventChangeFont) \
	mac(LayoutTextEventChangeScale) \
	mac(LayoutTextEventChangeColor) \
	mac(LayoutTextEventPopStyle)

DEFINE_DISCRIMINATED_UNION(LayoutTextEvent, DISC_MAC)

#undef DISC_MAC

struct LayoutRenderingContext {
	StringMap<stbtt_fontinfo> loadedFonts;
	Vector<void*> fontPointers;
	StringMap<BitmapData> loadedImages;

	Vector<LayoutTextStyle> styleStack;

	void PushStyle(const LayoutTextStyle& style) {
		styleStack.PushBack(style);
	}

	void PopStyle() {
		styleStack.PopBack();
	}

	LayoutTextStyle GetCurrentStyle() {
		return styleStack.Back();
	}

	stbtt_fontinfo GetFont(const char* name) {
		stbtt_fontinfo retfont;
		if (loadedFonts.LookUp(name, &retfont)) {
			return retfont;
		}
		else {
			int len;
			unsigned char* contents = ReadBinaryFile(name, &len);
			ASSERT(contents != nullptr);
			fontPointers.PushBack(contents);
			stbtt_fontinfo font;
			stbtt_InitFont(&font, contents, stbtt_GetFontOffsetForIndex(contents, 0));
			loadedFonts.Insert(name, font);
			return font;
		}
	}

	BitmapData GetImage(const char* name) {
		BitmapData data;
		if (loadedImages.LookUp(name, &data)) {
			return data;
		}
		else {
			data = LoadBMPFromFile(name);
			loadedImages.Insert(name, data);
			return data;
		}
	}

	~LayoutRenderingContext() {
		loadedFonts.Clear();

		BNS_VEC_FOREACH(fontPointers)	{
			free(*ptr);
		}

		for (int i = 0; i < loadedImages.count; i++) {
			free(loadedImages.values[i].data);
		}
	}
};

int MixColor(int tint, unsigned char scale) {
	int a = (tint & 0xFF000000) >> 24;
	int r = (tint & 0xFF0000) >> 16;
	int g = (tint & 0xFF00) >> 8;
	int b = (tint & 0xFF) >> 0;

	r = (r * scale) / 255;
	g = (g * scale) / 255;
	b = (b * scale) / 255;

	return b | (g << 8) | (r << 16) | (a << 24);
}

void RenderTextToBitmap(const SubString& text, float* currX, float* currY,
						float startX, float startY, float w, float h,
						LayoutTextStyle currStyle, LayoutRenderingContext* rc, BitmapData pageBmp) {
	
	stbtt_fontinfo font = rc->GetFont(StringStackBuffer<256>("%.*s", BNS_LEN_START(currStyle.fontName)).buffer);
	for (int i = 0; i < text.length; i++) {
		char c = text.start[i];
		float pixelScale = currStyle.scale;
		float scale = stbtt_ScaleForPixelHeight(&font, pixelScale);

		int cW = 0, cH = 0;
		unsigned char* cBmp = stbtt_GetCodepointBitmap(&font, 0, scale, c, &cW, &cH, 0, 0);

		BitmapData charBmp = { 0 };
		charBmp.width = cW;
		charBmp.height = cH;
		if (cBmp != nullptr	) {
			// TODO: Don't do this malloc
			charBmp.data = (int*)malloc(sizeof(int)*cW*cH);

			for (int i = 0; i < cW*cH; i++) {
				int x = i % cW;
				int y = i / cW;
				int otherIndex = x + cW * (cH - y - 1);
				charBmp.data[otherIndex] = MixColor(currStyle.color, cBmp[i]);
			}

			// TODO: Or this free?
			free(cBmp);
		}

		int x0, y0, x1, y1;
		stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0, &y0, &x1, &y1);

		int advanceWidth = 0, leftSideBearing = 0;
		stbtt_GetCodepointHMetrics(&font, c, &advanceWidth, &leftSideBearing);
		float xAdv = scale * advanceWidth;

		if (charBmp.data != nullptr) {
			BlitBitmap(pageBmp, (int)*currX + x0, (pageBmp.height - (int)*currY + y0) + h, x1 - x0, y1 - y0, charBmp);
		}

		*currX += xAdv;

		if (*currX > startY + w) {
			*currX = startX;
			*currY += pixelScale;
		}

		if (charBmp.data != nullptr) {
			free(charBmp.data);
		}
	}
}

void RenderTextEventsToBitmap(const Vector<LayoutTextEvent>& events, float x, float y, float w, float h, LayoutRenderingContext* rc, BitmapData pageBmp) {
	float currX = x;
	float currY = y;
	BNS_VEC_FOREACH(events) {
		LayoutTextStyle currStyle = rc->GetCurrentStyle();
		if (ptr->IsLayoutTextEventChangeFont()) {
			currStyle.fontName = ptr->AsLayoutTextEventChangeFont().fontName;
			rc->PushStyle(currStyle);
		}
		else if (ptr->IsLayoutTextEventChangeScale()) {
			currStyle.scale = ptr->AsLayoutTextEventChangeScale().scale;
			rc->PushStyle(currStyle);
		}
		else if (ptr->IsLayoutTextEventChangeColor()) {
			currStyle.color = ptr->AsLayoutTextEventChangeColor().color;
			rc->PushStyle(currStyle);
		}
		else if (ptr->IsLayoutTextEventChangeJustify()) {
			currStyle.justification = ptr->AsLayoutTextEventChangeJustify().justification;
			rc->PushStyle(currStyle);
			currX = x;
			// TODO: Force a new line here?
		}
		else if (ptr->IsLayoutTextEventPopStyle()) {
			rc->PopStyle();
			ASSERT(rc->styleStack.count > 0);
		}
		else if (ptr->IsLayoutTextEventDrawText()) {
			RenderTextToBitmap(ptr->AsLayoutTextEventDrawText().text, &currX, &currY, x, y, w, h, currStyle, rc, pageBmp);
		}
		else {
			ASSERT(false);
		}
	}
}

LayoutTextStyle GetDefaultStyle() {
	LayoutTextStyle style;
	style.fontName = STATIC_TO_SUBSTRING("fonts/Vera.ttf");
	style.color = 0xFFFFFFFF;
	style.justification = LTJ_Left;
	style.scale = 14.0f;

	return style;
}

void InitLayoutRenderingContext(LayoutRenderingContext* rc) {
	LayoutTextStyle style = GetDefaultStyle();
	rc->PushStyle(style);
}
