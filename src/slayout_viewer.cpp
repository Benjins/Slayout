#include "slayout.cpp"

BitmapData solvedData = {};

const char* slayoutFileName = "resume.slt";

void ResolveLayout(){
	if (solvedData.data != nullptr){
		free(solvedData.data);
		solvedData.data = nullptr;
		solvedData.width  = 0;
		solvedData.height  = 0;
	}
	
	String fileContents = ReadStringFromFile(slayoutFileName);

	Vector<BNSexpr> sexprs;
	BNSexprParseResult res = ParseSexprs(&sexprs, fileContents);
	
	if (res == BNSexpr_Success){
		LayoutContext ctx;
		InitLayoutContext(&ctx, frameWidth, frameHeight);
		LayoutContextSkeletonPass(sexprs, &ctx);
		LayoutContextVerbsPass(sexprs, &ctx);

		LayoutSolverResult res = LSR_SomeProgress;
		const int timeout = 1000;
		int timeoutCounter = 0;
		while (res == LSR_SomeProgress && timeoutCounter < timeout) {
			printf("Stepping solver...\n");
			res = StepLayoutContextSolver(&ctx);
			timeoutCounter++;
		}

		if (timeoutCounter >= timeout){
			printf("Solver timed out.\n");
		}
		else if (res == LSR_Success) {
			const char* fileName = "output/slayout.bmp";
			printf("Writeing out result to '%s'\n", fileName);
			RenderLayoutToBMP(&ctx, &solvedData);
		}
		else if (res == LSR_NoProgress){
			printf("Solver could not make progress!\n");
		}
		else if (res == LSR_Error) {
			printf("Error encountered during solving!\n");
		}
	}
}

uint64 lastFileWriteTime = 0;

void Init(){
	ResolveLayout();
}

int prevFrameWidth = 0, prevFrameHeight = 0;

void RunFrame(){
	BitmapData frameBuffer = { frameWidth, frameHeight, (int*)bitmapData };
	
	if (prevFrameWidth != frameWidth || prevFrameHeight != frameHeight){
		ResolveLayout();
		prevFrameWidth = frameWidth;
		prevFrameHeight = frameHeight;
	}

	{
		uint64 newLastFileWriteTime = GetLastFileAccess(slayoutFileName);
		if (newLastFileWriteTime > lastFileWriteTime) {
			lastFileWriteTime = newLastFileWriteTime;
			ResolveLayout();
		}
	}
	
	if (solvedData.data != nullptr){
		BlitBitmap(frameBuffer, 0, 0, frameBuffer.width, frameBuffer.height, solvedData);
	}
}

//struct WindowObj{
//	// TODO
//};

WindowObj* windowObj = nullptr;

int frameWidth = 0;
int frameHeight = 0;
void* bitmapData;

KeyState keyStates[256];

int currMouseX;
int currMouseY;
int mouseState;

const char* arg1Str;
int arg1Length;
