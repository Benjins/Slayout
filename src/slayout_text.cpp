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

	LayoutTextEventChangeJustify(LayoutTextJustification _justification) {
		justification = _justification;
	}
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

struct LayoutTextEventLineBreak {

};

#define DISC_MAC(mac) \
	mac(LayoutTextEventDrawText) \
	mac(LayoutTextEventChangeJustify) \
	mac(LayoutTextEventChangeFont) \
	mac(LayoutTextEventChangeScale) \
	mac(LayoutTextEventChangeColor) \
	mac(LayoutTextEventLineBreak) \
	mac(LayoutTextEventPopStyle)

DEFINE_DISCRIMINATED_UNION(LayoutTextEvent, DISC_MAC)

#undef DISC_MAC

struct LayoutRenderTextLineInfo {
	float totalWidth;
	int spaceCount;
	float height;

	LayoutRenderTextLineInfo() {
		totalWidth = 0.0f;
		spaceCount = 0;
		height = 0.0f;
	}
};

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

void LayoutTextForEvent(SubString text, Vector<LayoutTextEvent>* newEvents, float* currX,
	float startX, float w, const LayoutTextStyle& currStyle, LayoutRenderingContext* rc,
	Vector<LayoutRenderTextLineInfo>* outLineInfo) {

	stbtt_fontinfo font = rc->GetFont(StringStackBuffer<256>("%.*s", BNS_LEN_START(currStyle.fontName)).buffer);
	float pixelScale = currStyle.scale;
	float scale = stbtt_ScaleForPixelHeight(&font, pixelScale);

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
	float baseline = ascent * scale;

	outLineInfo->Back().height = BNS_MAX(outLineInfo->Back().height, baseline);

	int latestWhitespaceCharIdx = 0;
	float additionalWidth = 0.0f;
	for (int i = 0; i < text.length; i++) {
		char c = text.start[i];
		// TODO: Hard line breaks?
		if (c == ' ') {
			latestWhitespaceCharIdx = i;
			outLineInfo->Back().spaceCount++;
			outLineInfo->Back().totalWidth += additionalWidth;
			additionalWidth = 0.0f;
		}

		int x0, y0, x1, y1;
		stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0, &y0, &x1, &y1);

		int advanceWidth = 0, leftSideBearing = 0;
		stbtt_GetCodepointHMetrics(&font, c, &advanceWidth, &leftSideBearing);
		float xAdv = scale * advanceWidth;

		if (*currX + x1 > startX + w) {
			// Need to break line
			SubString start = text, rest = text;
			start.length = latestWhitespaceCharIdx;
			rest.start += (latestWhitespaceCharIdx + 1);
			rest.length -= (latestWhitespaceCharIdx + 1);

			*currX = startX;

			newEvents->PushBack(LayoutTextEventDrawText(start));
			newEvents->PushBack(LayoutTextEventLineBreak());
			outLineInfo->EmplaceBack();
			outLineInfo->Back().height = BNS_MAX(outLineInfo->Back().height, pixelScale);

			i = 0;
			text = rest;
			latestWhitespaceCharIdx = 0;
			additionalWidth = 0.0f;
			i--;
		}
		else {
			*currX += xAdv;
			additionalWidth += xAdv;
		}
	}

	// TODO: Will include width of any whitespace at the end as well
	outLineInfo->Back().totalWidth += additionalWidth;

	newEvents->PushBack(LayoutTextEventDrawText(text));
}

void LayoutTextEvents(const Vector<LayoutTextEvent>& events, Vector<LayoutTextEvent>* newEvents, float x, float w, LayoutRenderingContext* rc, 
							  Vector<LayoutRenderTextLineInfo>* outLineInfo) {

	// Get first line info
	outLineInfo->EmplaceBack();
	float currX = x;
	BNS_VEC_FOREACH(events) {
		bool addEvent = true;
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
		else if (ptr->IsLayoutTextEventLineBreak()) {
			currX = x;
			// TODO: Advance new line
		}
		else if (ptr->IsLayoutTextEventPopStyle()) {
			rc->PopStyle();
			ASSERT(rc->styleStack.count > 0);
		}
		else if (ptr->IsLayoutTextEventDrawText()) {
			addEvent = false;
			LayoutTextForEvent(ptr->AsLayoutTextEventDrawText().text, newEvents, &currX, x, w, currStyle, rc, outLineInfo);
		}
		else {
			ASSERT(false);
		}

		if (addEvent) {
			newEvents->PushBack(*ptr);
		}
	}
}

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
						float startX, float startY, float w, float h, float lineHeight,
						LayoutTextStyle currStyle, LayoutRenderingContext* rc, BitmapData pageBmp) {
	
	stbtt_fontinfo font = rc->GetFont(StringStackBuffer<256>("%.*s", BNS_LEN_START(currStyle.fontName)).buffer);

	float pixelScale = currStyle.scale;
	float scale = stbtt_ScaleForPixelHeight(&font, pixelScale);

	for (int i = 0; i < text.length; i++) {
		char c = text.start[i];

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
			BlitBitmap(pageBmp, (int)*currX + x0, (pageBmp.height - (int)*currY + y0 + lineHeight) + h, x1 - x0, y1 - y0, charBmp);
		}

		*currX += xAdv;

		if (charBmp.data != nullptr) {
			free(charBmp.data);
		}
	}
}

struct JustifyLineInfo {
	float startX;
	// TODO: calculate these
	float extraWordSpace;
	float extraLetterSpace;
};

JustifyLineInfo StartingXForJustification(float startX, float maxWidth, float textWidth, LayoutTextJustification just) {
	JustifyLineInfo info = {};
	if (just == LTJ_Left) {
		info.startX = startX;
	}
	else if (just == LTJ_Right) {
		info.startX = startX + maxWidth - textWidth;
	}
	else if (just == LTJ_FullJustify) {
		// TODO: Actually justify it as well
		info.startX = startX;
	}
	else if (just == LTJ_Centre) {
		info.startX = startX + (maxWidth - textWidth) / 2;
	}
	else {
		ASSERT(false);
	}

	return info;
}

void RenderTextEventsToBitmap(const Vector<LayoutTextEvent>& events, float x, float y, float w, float h, LayoutRenderingContext* rc, BitmapData pageBmp) {
	float currX = x;
	float currY = y + h;

	Vector<LayoutTextEvent> newEvents;
	Vector<LayoutRenderTextLineInfo> lineInfo;
	LayoutTextEvents(events, &newEvents, x, w, rc, &lineInfo);

	int lineIndex = 0;

	BNS_VEC_FOREACH(newEvents) {
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
			JustifyLineInfo info = StartingXForJustification(x, w, lineInfo.Get(lineIndex).totalWidth, currStyle.justification);
			currX = info.startX;
		}
		else if (ptr->IsLayoutTextEventLineBreak()) {
			// TODO: Advance new line
			currY -= lineInfo.data[lineIndex].height;
			lineIndex++;
			JustifyLineInfo info = StartingXForJustification(x, w, lineInfo.Get(lineIndex).totalWidth, currStyle.justification);
			currX = info.startX;
		}
		else if (ptr->IsLayoutTextEventPopStyle()) {
			rc->PopStyle();
			ASSERT(rc->styleStack.count > 0);
		}
		else if (ptr->IsLayoutTextEventDrawText()) {
			RenderTextToBitmap(ptr->AsLayoutTextEventDrawText().text, &currX, &currY, x, y, w, h, lineInfo.data[lineIndex].height, currStyle, rc, pageBmp);
		}
		else {
			ASSERT(false);
		}
	}

	ASSERT(lineIndex == lineInfo.count || lineIndex == lineInfo.count - 1);
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
