#include "stdafx.h"
#include "Overlay/Overlay.h"
#include "Overlay/D3D9Overlay.h"
#include "Overlay/Color.h"
#include <iostream>   
#include <string>  
#include <sstream> 
#include <iomanip>
#include <algorithm> 

/* Globals */
int ScreenX = 0;
int ScreenY = 0;
int barColor = 0x0;
int barPosX = 0;
int barPosY = 0;
int barWidth = 0;
int barHeight = 0;
int hpBarX = 0;
int hpBarY = 0;

int selectedOption = -1;
std::string debugText = "";
std::string infoText = "";
int maxOption = 1;
bool showMenu = true;
BYTE* ScreenData = 0;

void screenCap(int startX, int startY, int widthX, int widthY)
{
	HDC hScreen = GetDC(GetDesktopWindow());
	ScreenX = GetDeviceCaps(hScreen, HORZRES);
	ScreenY = GetDeviceCaps(hScreen, VERTRES);

	ScreenX = min(startX + widthX, ScreenX);
	ScreenY = min(startY + widthY, ScreenY);

	startX = min(startX, ScreenX);
	startY = min(startY, ScreenY);

	HDC hdcMem = CreateCompatibleDC(hScreen);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, ScreenX, ScreenY);
	HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);
	BitBlt(hdcMem, -startX, -startY, ScreenX + 1, ScreenY + 1, hScreen, 0, 0, SRCCOPY);
	SelectObject(hdcMem, hOld);

	BITMAPINFOHEADER bmi = { 0 };
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biPlanes = 1;
	bmi.biBitCount = 32;
	bmi.biWidth = ScreenX;
	bmi.biHeight = -ScreenY;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = 0;// 3 * ScreenX * ScreenY;

	if (ScreenData)
		free(ScreenData);
	ScreenData = (BYTE*)malloc(4 * ScreenX * ScreenY);

	GetDIBits(hdcMem, hBitmap, 0, ScreenY, ScreenData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

	ReleaseDC(GetDesktopWindow(), hScreen);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);
}

int posB(int x, int y)
{
	return ScreenData[4 * ((y*ScreenX) + x)];
}

int posG(int x, int y)
{
	return ScreenData[4 * ((y*ScreenX) + x) + 1];
}

int posR(int x, int y)
{
	return ScreenData[4 * ((y*ScreenX) + x) + 2];
}

unsigned long createRGBA(int r, int g, int b, int a)
{
	return ((a & 0xff) << 24) + ((r & 0xff) << 16) + ((g & 0xff) << 8)
		+ (b & 0xff);
}

bool colorRangeCheck(int r1, int g1, int b1, int r2, int g2, int b2, int maxDif) {
	bool r = (r1 <= r2 + maxDif) && (r2 <= r1 + maxDif);
	bool g = (g1 <= g2 + maxDif) && (g2 <= g1 + maxDif);
	bool b = (b1 <= b2 + maxDif) && (b2 <= b1 + maxDif);
	return r && g && b;
}

char* convertString(std::string s) {
	static char arr[200];
	strncpy_s(arr, s.c_str(), sizeof(arr));
	return arr;
}

unsigned long convertColor(unsigned long oldCol)
{
	//return createRGBA(((oldCol >> 16) & 0xFF), ((oldCol >> 8) & 0xFF), ((oldCol)& 0xFF), 255);
	return createRGBA(GetRValue(oldCol), GetGValue(oldCol), GetBValue(oldCol), 255);
}

void calibate(int x, int y, int cr, int cg, int cb) {
	HDC hScreen = GetDC(GetDesktopWindow());
	screenCap(0, 0, GetDeviceCaps(hScreen, HORZRES), GetDeviceCaps(hScreen, VERTRES)); //just the whole screen
	if (x <= 0 || y <= 0) {
		return;
	}
	int tempY = y;
	const int cap = 5000;
	const int tollerance = 20;
	int c = 0;
	while (tempY >= 0 && colorRangeCheck(posR(x, tempY), posG(x, tempY), posB(x, tempY), cr, cg, cb, tollerance) && c < cap) {
		tempY--;
		c++;
	}
	barPosY = tempY;
	tempY = y;
	while (tempY < GetDeviceCaps(hScreen, VERTRES) && colorRangeCheck(posR(x, tempY), posG(x, tempY), posB(x, tempY), cr, cg, cb, tollerance) && c < cap) {
		tempY++;
		c++;
	}
	barHeight = tempY - barPosY;
	int tempX = x;
	while (tempX >= 0 && colorRangeCheck(posR(tempX, y), posG(tempX, y), posB(tempX, y), cr, cg, cb, tollerance) && c < cap) {
		tempX--;
		c++;
	}
	barPosX = tempX - 1;
	tempX = x;
	while (tempX < GetDeviceCaps(hScreen, HORZRES) && colorRangeCheck(posR(tempX, y), posG(tempX, y), posB(tempX, y), cr, cg, cb, tollerance) && c < cap) {
		tempX++;
		c++;
	}
	barWidth = tempX - barPosX;
}


float getPercentageHP() {
	screenCap(barPosX, barPosY, barWidth, barHeight);
	if (barWidth == 0 || barHeight == 0) {
		return -1;
	}
	std::vector<int> avgR(barWidth);
	std::vector<int> avgG(barWidth);
	std::vector<int> avgB(barWidth);
	for (int i = 1; i <= barWidth; i++) {
		int sumR = 0;
		int sumG = 0;
		int sumB = 0;
		for (int j = 1; j <= barHeight; j++) {
			int x = i;
			int y = j;
			int red = posR(x, y);
			int green = posG(x, y);
			int blue = posB(x, y);
			
			sumR += red;
			sumG += green;
			sumB += blue;
		}
		sumR /= barHeight;
		sumG /= barHeight;
		sumB /= barHeight;
		avgR[i - 1] = sumR;
		avgG[i - 1] = sumG;
		avgB[i - 1] = sumB;
	}
	for (int i = 0; i < avgR.size(); i++) {
		if (avgR[i] < 80) {
			return (float)i / barWidth;
		}
	}
	return 1.f;
}

void displayPercentualHP(std::shared_ptr< Overlay::ISurface > pSurface) {
	float hp = getPercentageHP() * 100;
	std::stringstream stream;
	stream << std::fixed << std::setprecision(1) << hp;
	std::string hps = stream.str() + "P";
	static char hpca[10];
	strncpy_s(hpca, hps.c_str(), sizeof(hpca));
	int col = col = createRGBA(0, 255, 0, 255);
	if (hp < 75){
		col = createRGBA(255, 255, 0, 255);
	}
	if(hp < 50){
		col = createRGBA(255, 160, 0, 255);
	}
	if (hp < 25) {
		col = createRGBA(255, 0, 0, 255);
	}
	pSurface->String(hpBarX, hpBarY, "Default", col, hpca);
}

bool isInRect(int x, int y, int rx, int ry, int rw, int rh) {
	return (x > rx && x < rx + rw && y > ry && y < ry + rh);
}

bool isInOption(int x, int y, int option) {
	return isInRect(x, y, 170, 95 + 25 * option, 150, 25);
}

bool processInput(std::shared_ptr< Overlay::ISurface > pSurface) {
	if (GetAsyncKeyState(VK_F10)) {
		showMenu = !showMenu;
	}else if (showMenu && GetAsyncKeyState(VK_LBUTTON)) {
		POINT p;
		GetPhysicalCursorPos(&p);
		HWND window = GetForegroundWindow();
		ScreenToClient(window, &p);
		if (selectedOption == -1) {
			for (int i = 0; i <= maxOption; i++) {
				if (isInOption(p.x, p.y, i)) {
					debugText = std::to_string(i);
					selectedOption = i;
				}
			}
			if (selectedOption == 0) {
				infoText = "Click on the HP Bar's Center!";
			}else if (selectedOption == 1) {
				infoText = "Click on desired position";
			}
		}else if (selectedOption == 0) {
			GetPhysicalCursorPos(&p);
			ShowCursor(FALSE);
			screenCap(p.x, p.y, 0, 0);
			barColor = createRGBA(posR(0, 0), posG(0, 0), posB(0, 0), 255);
			calibate(p.x, p.y, posR(0, 0), posG(0, 0), posB(0, 0));
			ShowCursor(TRUE);
			selectedOption = -1;
			infoText = "";
		}else if (selectedOption == 1) {
			hpBarX = p.x;
			hpBarY = p.y;
			selectedOption = -1;
			infoText = "";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(160));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(160));
	return true;
}

void displayMenu(std::shared_ptr< Overlay::ISurface > pSurface) {
	pSurface->Rect(150, 50, 300, 170, createRGBA(101, 71, 39, 255));
	pSurface->BorderBox(150, 50, 300, 170, 3,createRGBA(10, 10, 10, 255));
	pSurface->String(165, 65, "Headline", createRGBA(255, 240, 190, 255), convertString("Percentual HP - F10"));
	pSurface->String(175, 105, "Default", createRGBA(255, 240, 190, 255), convertString(">Calibrate"));
	pSurface->String(175, 130, "Default", createRGBA(255, 240, 190, 255), convertString(">Set Position"));
	//pSurface->BorderBox(170, 100, 150, 25, 3, createRGBA(10, 10, 10, 255));
	pSurface->String(175, 180, "Default", createRGBA(255, 240, 190, 255), convertString(infoText));
	//pSurface->String(175, 250, "Default", createRGBA(255, 240, 190, 255), convertString(debugText));
}


int main( char *pszArg[] )
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);
	SetProcessDPIAware();
	std::string strWindowTitle( "", 0x40 );
	strWindowTitle = "Guild Wars 2";

	Overlay::IOverlay *pOverlay = new Overlay::CD3D9Overlay( );
	pOverlay->GetSurface( )->PrepareFont( "Default", "Arial", 20, FW_BOLD, 0, 0, FALSE );
	pOverlay->GetSurface()->PrepareFont("Headline", "Arial", 25, FW_BOLD, 0, 0, FALSE);
	if( pOverlay->Create( strWindowTitle ) )
	{
		auto WaterMarkFn = []( Overlay::IOverlay *pOverlay, std::shared_ptr< Overlay::ISurface > pSurface )
		{
			//Move overlay if window is moved
			/*for (int i = 1; i <= barWidth; i++) {
				for (int j = 1; j <= barHeight; j++) {
					int x = i;
					int y = j;
					int col = createRGBA(posR(x, y), posG(x, y), posB(x, y), 255);
					pSurface->Rect(500 + i, 800 + j, 1, 1, col);
				}
			}*/
			pOverlay->ScaleOverlayWindow();
			processInput(pSurface);
			displayPercentualHP(pSurface);
			if (showMenu) {
				displayMenu(pSurface);
			}
			/**pSurface->Rect(800, 90, 100, 50, barColor);
			POINT p;
			GetPhysicalCursorPos(&p);
			HWND window = GetForegroundWindow();
			ScreenToClient(window, &p);
			std::stringstream s2;
			s2 << std::fixed << std::setprecision(1) << p.x << " | " << p.y;
			std::string col = s2.str() + "P";
			static char cola[1024];
			strncpy_s(cola, col.c_str(), sizeof(cola));
			pSurface->String(500, 90, "Default", createRGBA(0, 255, 0, 255), cola);
			strncpy_s(cola, std::to_string(barPosX).c_str(), sizeof(cola));
			pSurface->String(500, 120, "Default", createRGBA(0, 255, 0, 255), cola);
			strncpy_s(cola, std::to_string(barPosY).c_str(), sizeof(cola));
			pSurface->String(500, 150, "Default", createRGBA(0, 255, 0, 255), cola);
			strncpy_s(cola, std::to_string(barWidth).c_str(), sizeof(cola));
			pSurface->String(500, 180, "Default", createRGBA(0, 255, 0, 255), cola);
			strncpy_s(cola, std::to_string(barHeight).c_str(), sizeof(cola));
			pSurface->String(500, 210, "Default", createRGBA(0, 255, 0, 255), cola);
			*/
		};
		
		pOverlay->AddToCallback( WaterMarkFn );
		while (pOverlay->Render()) {
		}
	}else {

	}

	pOverlay->Shutdown();
	delete pOverlay; 
	return 0;

}


