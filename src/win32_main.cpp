
#include <Windows.h>
#include <Windowsx.h>

#include <stdio.h>
#include <stdint.h>

typedef uint64_t uint64;

typedef struct{
	HWND hwnd;
} WindowObj;

enum KeyState {
	OFF = 0,
	RELEASE = 1,
	PRESS = 2,
	HOLD = 3
};

inline KeyState StateFromBools(bool wasDown, bool isDown) {
	return (KeyState)((wasDown ? 1 : 0) | (isDown ? 2 : 0));
}

extern KeyState keyStates[256];

extern int currMouseX;
extern int currMouseY;
extern int mouseState;

void RunFrame();

void InitText(const char* fileName, int size);

void Init();

extern const char* arg1Str;
extern int arg1Length;

HBITMAP bitmap = 0;
BITMAPINFO bmpInfo = {};

extern void* bitmapData;
extern int frameWidth;
extern int frameHeight;

LRESULT CALLBACK MyGuiWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void WindowsPaintWindow(HWND hwnd) {
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);

	HDC deviceContext = GetDC(hwnd);

	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	int bmpWidth = bmpInfo.bmiHeader.biWidth, bmpHeight = bmpInfo.bmiHeader.biHeight;
	
	StretchDIBits(deviceContext, 
		0, 0, clientWidth, clientHeight, 
		0, 0, bmpWidth, bmpHeight, 
		bitmapData, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

	ReleaseDC(hwnd, deviceContext);
}


void ResizeWindow(int w, int h) {
	if (bitmap) {
		DeleteObject(bitmap);
	}

	bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
	bmpInfo.bmiHeader.biWidth  = w;
	bmpInfo.bmiHeader.biHeight = h;
	bmpInfo.bmiHeader.biPlanes = 1;
	bmpInfo.bmiHeader.biBitCount = 32;
	bmpInfo.bmiHeader.biCompression = BI_RGB;

	HDC deviceContext = CreateCompatibleDC(0);
	bitmap = CreateDIBSection(deviceContext, &bmpInfo, DIB_RGB_COLORS, &bitmapData, 0, 0);
	ReleaseDC(0, deviceContext);
	
	frameWidth = bmpInfo.bmiHeader.biWidth;
	frameHeight = bmpInfo.bmiHeader.biHeight;
}

bool WindowsOpenFile(char* fileName, WindowObj* owner, const char* fileFilter, const char* filterTitle, const char* dirName, int dirLength){
	OPENFILENAME spriteFile = {};
	
	char initialDir[256] = {};
	GetCurrentDirectory(sizeof(initialDir), initialDir);
	char usedDir[256] = {};
	snprintf(usedDir, 256, "%s\\%.*s", initialDir, dirLength, dirName);
	
	spriteFile.lpstrInitialDir = usedDir;

	char szFilters[256] = {};
	sprintf(szFilters, "%s (*.%s)\0*.%s\0\0", filterTitle, fileFilter, fileFilter);
	char szFilePathName[512] = "";

	spriteFile.lStructSize = sizeof(OPENFILENAME);
	spriteFile.hwndOwner = owner->hwnd;
	spriteFile.lpstrFilter = szFilters;
	spriteFile.lpstrFile = szFilePathName;  // This will hold the file name
	spriteFile.lpstrDefExt = "bmp";
	spriteFile.nMaxFile = 512;
	spriteFile.lpstrTitle = "Open BMP File";
	spriteFile.Flags = OFN_OVERWRITEPROMPT;
	
	/*
	if(GetOpenFileName(&spriteFile)){	
		//The open file dialog sets the current directory to the project directory,
		//so we reset it to the outer directory
		SetCurrentDirectory(initialDir);
		
		char shortFileName[256] = {};
		int fullPathLength = strlen(szFilePathName);
		char* fileNameCursor = szFilePathName + (fullPathLength - 1);
		for(int i = 0; i < fullPathLength - 1; i++){
			fileNameCursor--;
			if(*fileNameCursor == '\\' || *fileNameCursor == '/'){
				fileNameCursor++;
				break;
			}
		}
		
		int fileNameLength = strlen(fileNameCursor);
		memcpy(fileName, fileNameCursor, fileNameLength);
		fileName[fileNameLength] = '\0';
		
		return true;
	}*/
	
	return false;
}

extern WindowObj* windowObj;

extern const char* arg1Str;
extern int arg1Length;

 int CALLBACK WinMain(HINSTANCE inst, HINSTANCE prevInst,
					 LPSTR cmdLine, int cmdShow) {
	
	
	WNDCLASS windowCls = {};
	windowCls.hInstance = inst;
	windowCls.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	windowCls.lpfnWndProc = MyGuiWindowProc;
	windowCls.lpszClassName = "my-gui-window";

	RegisterClass(&windowCls);

	HWND window = CreateWindow(windowCls.lpszClassName, "Tile Mapper", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 850, 1100, 0, 0, inst, 0);

	WindowObj windowObjVal = {};
	windowObjVal.hwnd = window;

	windowObj = &windowObjVal;

	windowObj->hwnd = window;
	
	char* commandLine = cmdLine;
	arg1Str = commandLine + strspn(commandLine, "\n ");
	arg1Length = strcspn(arg1Str, "\n ");

	//InitText("C:/Program Files/Java/jdk1.8.0_65/jre/lib/fonts/LucidaSansRegular.ttf", 18);
	//InitText("C:/Program Files (x86)/Java/jdk1.7.0_55/jre/lib/fonts/LucidaSansRegular.ttf", 18);
	Init();

	bool isRunning = true;
	while (isRunning) {
		tagMSG msg;

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_QS_INPUT))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			//TODO: Why the f--k is this 161, not WM_QUIT?
			if (msg.message == 161 && msg.wParam == 20) {
				isRunning = false;
			}
		}

		RunFrame();
		
		WindowsPaintWindow(window);
	}

}

LRESULT CALLBACK MyGuiWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	
	switch (message) {
		case WM_SIZE: {
			RECT clientRect;
			GetClientRect(hwnd, &clientRect);
			ResizeWindow(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
		}break;
	
		case WM_DESTROY: {
			result = DefWindowProc(hwnd, message, wParam, lParam);
		}break;

		case WM_CLOSE: {
			result = DefWindowProc(hwnd, message, wParam, lParam);
		}break;

		case WM_MOUSEMOVE:
		{
			int mouseX = GET_X_LPARAM(lParam);
			int mouseY = GET_Y_LPARAM(lParam);
			
			currMouseX = mouseX;
			currMouseY = mouseY;

			if (wParam & MK_LBUTTON) {
				mouseState = HOLD;
			}
		}break;

		case WM_LBUTTONDOWN:
		{
			int mouseX = GET_X_LPARAM(lParam);
			int mouseY = GET_Y_LPARAM(lParam);
			
			if(mouseState != HOLD){
				mouseState = PRESS;
			}
		}break;
		
		case WM_LBUTTONUP:
		{
			//int mouseX = GET_X_LPARAM(lParam);
			//int mouseY = GET_Y_LPARAM(lParam);
			
			mouseState = RELEASE;
			//MouseDown(mouseX, mouseY);
		}break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			int code = wParam;
			bool wasDown = (lParam & (1 << 30)) != 0;
			bool  isDown = (lParam & (1 << 31)) == 0;

			keyStates[code] = StateFromBools(wasDown, isDown);
		}break;
		
		
		case WM_PAINT: {
			WindowsPaintWindow(hwnd);
		}break;
		

		case WM_ACTIVATEAPP: {
		
		}break;

		case WM_MOUSEWHEEL: {
			float zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			printf("Whell delta: %f\n", zDelta);
			
		}break;

		case WM_GESTURE: {
			printf("WM Gesture.\n");
		}break;
		
		case WM_SETFOCUS:{
			printf("WM Set Fcous");
		} break;

		case WM_TOUCH: {
			printf("WM Touch.\n");
		}break;
		
		default: {
			result = DefWindowProc(hwnd, message, wParam, lParam);
		} break;
	
	}

	return result;
}

uint64 GetLastFileAccess(const char* fileName) {
	HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		printf("CreateFile failed with %d\n", GetLastError());
		return 0;
	}
	
	FILETIME lastWriteTime;
	if (!GetFileTime(hFile, NULL, NULL, &lastWriteTime)){
		return 0;
	}

	CloseHandle(hFile);

	return ((uint64)lastWriteTime.dwHighDateTime << 32) | (uint64)lastWriteTime.dwLowDateTime;
}

#include "slayout_viewer.cpp"

