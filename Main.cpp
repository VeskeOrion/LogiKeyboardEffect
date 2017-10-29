#include <LogitechLEDLib.h>
#include <Windows.h>
#include <cmath>
#include <random>
#include <ctime>

#define VIS		0xFF000000
#define RED		0x00FF0000
#define GRN		0x0000FF00
#define BLU		0x000000FF
#define ONEVIS	0x01000000
#define ONERED	0x00010000
#define ONEGRN	0x00000100
#define ONEBLU	0x00000001

void lightUpKeypresses(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool isNewStar(unsigned int c);
bool isStar(unsigned int c);
unsigned int randomInitialStarColor();
unsigned int randomNextStarColor();
unsigned int fadeToColor(unsigned int c1, unsigned int c2);
LRESULT CALLBACK WinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// A random number generator
std::mt19937 gen((unsigned int)time(0));

// Various color constants based on preference
const unsigned int backgroundColor = 0xFF000F46; // Our desired background color is RBG(0, 15, 70) or PERCENT(0, 6, 27) or HEX(0xFF000F46)
const unsigned int newStarMaxGrnInt = 0xFF;
const unsigned int newStarMinGrnInt = 0xBB;
const unsigned int oldStarMaxGrnInt = 0xAA;
const unsigned int oldStarMinGrnInt = 0x77;

// Various constants relating to the keyboard bitmap
const int size = LOGI_LED_BITMAP_SIZE / LOGI_LED_BITMAP_BYTES_PER_KEY;
const int gridHeight = LOGI_LED_BITMAP_HEIGHT;
const int gridWidth = LOGI_LED_BITMAP_WIDTH;

// The bitmap of colors onto all the keyboard keys
unsigned int bitmap[size]; 

// Raw input device handle, used in lightUpKeypresses for getting keyboard input
RAWINPUTDEVICE device;

// Various constants dealing with the main loop timing and star generation/fading
const int dt = 50;
const int pollsPerSecond = 1000 / dt;
const int desiredStarsPerSecond = 5;
const int starChance = 100 * desiredStarsPerSecond / pollsPerSecond;
const int fadePollsPerSecond = 10;
const int fadePollsCounter = pollsPerSecond / fadePollsPerSecond;

// Entry point for the program
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int cmdSHow) {

	// Set up a hidden window
	WNDCLASS wc = {};
	wc.lpfnWndProc = WinProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = LPCSTR("LogiKeyboardEffect_Stars");
	if (RegisterClass(&wc) == 0)
		return 0;
	HWND hwnd = CreateWindow(wc.lpszClassName, LPCSTR("LogiKeyboardEffect_Stars"), 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
	if (hwnd == NULL)
		return 0;

	// Setup the raw input device
	device.usUsagePage = 0x01;
	device.usUsage = 0x06;
	device.dwFlags = RIDEV_INPUTSINK | RIDEV_NOLEGACY; // RIDEV_INPUTSINK means this raw input device always gets inputs, even when out of focus
	device.hwndTarget = hwnd;
	RegisterRawInputDevices(&device, 1, sizeof(device));

	// Fill the bitmap with the background color
	for (int i = 0; i < size; ++i)
		bitmap[i] = backgroundColor;

	// Initialize the Logitech keyboard lighting system
	LogiLedInit();
	Sleep(1000);
	LogiLedSetLightingFromBitmap((unsigned char *)bitmap); // Fill all keyboard keys with the bitmap
	LogiLedSetLighting(0, 6, 27); // Set miscelaneous Logitech lighting (volume buttons, caps/scroll/numlock lights) to the background color

	// Initialize the counter for fading stars (fading only happens every few iterations of the main loop)
	int fadeWhenZero = fadePollsCounter;

	// Continually fade out old stars, draw new stars, and poll for Windows messages and keyboard inputs, then sleep
	while (true) {

		// Fade out old stars when the counter has reached zero, then reset it
		if (fadeWhenZero == 0) {
			for (int i = 0; i < size; ++i) {
				if (isNewStar(bitmap[i]))
					bitmap[i] = randomNextStarColor(); // Sharply decrease the brightness of brand new stars
				else if (isStar(bitmap[i]))
					bitmap[i] = fadeToColor(bitmap[i], backgroundColor); // Slowly restore the background color on all keys that are not the background color
			}
			fadeWhenZero = fadePollsCounter;
		}
		--fadeWhenZero;

		// Create new stars randomly
		if (gen() % 100 < starChance) {
			unsigned int randKey = gen() % size;
			bitmap[randKey] = randomInitialStarColor(); // Create a new random bright star
		}

		// Windows message loop
		MSG msg;
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Update lighting
		LogiLedSetLightingFromBitmap((unsigned char *)bitmap);

		// Delay until next iteration
		Sleep(dt);
	}

	// Shutdown the Logi system (this never happens because of the infinite while)
	LogiLedShutdown();
}

// Returns if a color is symbolic of being a brand new star, ie a color returned from randomInitialStarColor but not randomNextStarColor
bool isNewStar(unsigned int c) {
	return (c & GRN) > (ONEGRN * newStarMinGrnInt);
}

// Returns if a color is equal to the background color
bool isStar(unsigned int c) {
	return c != backgroundColor;
}

// Returns a yellowish-bright color
unsigned int randomInitialStarColor() {
	unsigned int grnInt = newStarMinGrnInt + gen() % (newStarMaxGrnInt - newStarMinGrnInt);
	unsigned int c = VIS | RED | (ONEGRN * grnInt);
	return c;
}

// Returns an orangeish-dim color
unsigned int randomNextStarColor() {
	unsigned int grnInt = oldStarMinGrnInt + gen() % (oldStarMaxGrnInt - oldStarMinGrnInt);
	unsigned int c = VIS | RED | (ONEGRN * grnInt);
	return c;
}

// Returns a new color that is a step further from color c1 and closer to color c2, but blue moves slightly slower
unsigned int fadeToColor(unsigned int c1, unsigned int c2) {
	int c1red = (c1 & RED) >> 16;
	int c1grn = (c1 & GRN) >> 8;
	int c1blu = (c1 & BLU) >> 0;

	int c2red = (c2 & RED) >> 16;
	int c2grn = (c2 & GRN) >> 8;
	int c2blu = (c2 & BLU) >> 0;

	int dr = c2red - c1red;
	int dg = c2grn - c1grn;
	int db = c2blu - c1blu;
	dr = std::abs(dr);
	dg = std::abs(dg);
	db = std::abs(db);

	int redStep = 0;
	int greenStep = 0;
	int blueStep = 0;

	// Constant step
	//redStep = (dr > 0)? (int) std::log2(dr) + 1 : 0;
	//greenStep = (dg > 0)? (int) std::log2(dg) + 1 : 0;
	//blueStep = (db > 0)? (int) std::log2(db) + 1 : 0;

	// Logarithmic step
	redStep = (dr > 3) ? (int)std::log2(dr) + 1 : 0;
	greenStep = (dg > 3) ? (int)std::log2(dg) + 1 : 0;
	blueStep = (db > 3) ? (int)std::log(std::log2(db)) + 1 : 0; // log* !!!!!!!!

	if (c2red < c1red && redStep != 0)
		redStep = -redStep;
	if (c2grn < c1grn && greenStep != 0)
		greenStep = -greenStep;
	if (c2blu < c1blu && blueStep != 0)
		blueStep = -blueStep;

	unsigned int newRed = c1red + redStep;
	unsigned int newGrn = c1grn + greenStep;
	unsigned int newBlu = c1blu + blueStep;

	unsigned int newColor = 0;
	newColor |= 0xFF000000;
	newColor |= (newRed << 16);
	newColor |= (newGrn << 8);
	newColor |= (newBlu << 0);

	return newColor;
}

// Gets the data from the current message and responds in some way
LRESULT CALLBACK WinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INPUT: // User has pressed a keyboard key
		lightUpKeypresses(hwnd, uMsg, wParam, lParam);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Lights up keys according to raw input
void lightUpKeypresses(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	// Random scary Windows code
	unsigned char rawInputBuffer[sizeof(RAWINPUT)] = {};
	UINT rawInputBufferSize = sizeof(RAWINPUT);

	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawInputBuffer, &rawInputBufferSize, sizeof(RAWINPUTHEADER));
	RAWINPUT * raw = (RAWINPUT *)rawInputBuffer;

	if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		RAWKEYBOARD rawKB = raw->data.keyboard;

		UINT virtualKey = rawKB.VKey;
		UINT scanCode = rawKB.MakeCode;
		UINT flags = rawKB.Flags;

		//if ((flags & RI_KEY_BREAK))
			//return; // figure out which case is "key down" and return if key is not down

		// Return if we get a pointless key
		if (virtualKey == 255)
			return;

		// Fix the mapping between both shifts
		if (virtualKey == VK_SHIFT)
			virtualKey = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);

		// See if key is an extended key, such that that isE0 == true
		bool isE0 = ((flags & RI_KEY_E0) != 0);

		// Now light up the specific key (these are ordered according to Windows VK values)
		switch (virtualKey) {

		// Back and Tab Keys
		case VK_BACK: bitmap[gridWidth * 1 + 13] = randomInitialStarColor(); break;
		case VK_TAB:  bitmap[gridWidth * 2 +  0] = randomInitialStarColor(); break;

		// Enter Key
		case VK_RETURN:
			if (!isE0)
				bitmap[gridWidth * 3 + 13] = randomInitialStarColor(); // regular enter
			else
				bitmap[gridWidth * 4 + 20] = randomInitialStarColor(); // numpad enter
			break;

		// Control, Alt, Pause, Caps Lock Keys
		case VK_CONTROL:
			if (!isE0)
				bitmap[gridWidth * 5 +  0] = randomInitialStarColor(); // left control
			else
				bitmap[gridWidth * 5 + 14] = randomInitialStarColor(); // right control
			break;
		case VK_MENU:
			if (!isE0)
				bitmap[gridWidth * 5 + 2] = randomInitialStarColor(); // left alt
			else
				bitmap[gridWidth * 5 + 11] = randomInitialStarColor(); // right alt
			break;
		case VK_PAUSE:   bitmap[gridWidth * 0 + 15] = randomInitialStarColor(); break;
		case VK_CAPITAL: bitmap[gridWidth * 3 +  0] = randomInitialStarColor(); break;

		// Escape Key
		case VK_ESCAPE: bitmap[gridWidth * 0 + 0] = randomInitialStarColor(); break;

		// Spacebar Key
		case VK_SPACE: bitmap[gridWidth * 5 + 5] = randomInitialStarColor(); break;

		// Insert, Home, Page Up, Delete, End, Page Down, Arrow Keys, Printscrn, Delete Keys
		case VK_PRIOR:
			if (isE0)
				bitmap[gridWidth * 1 + 16] = randomInitialStarColor(); // regular page up
			else
				bitmap[gridWidth * 2 + 19] = randomInitialStarColor(); // numlock numpad 9
			break;
		case VK_NEXT:
			if (isE0)
				bitmap[gridWidth * 2 + 16] = randomInitialStarColor(); // regular page down
			else
				bitmap[gridWidth * 4 + 19] = randomInitialStarColor(); // numlock numpad 3
			break;
		case VK_END: 
			if (isE0)
				bitmap[gridWidth * 2 + 15] = randomInitialStarColor(); // regular end
			else
				bitmap[gridWidth * 4 + 17] = randomInitialStarColor(); // numlock numpad 1
			break;
		case VK_HOME:
			if (isE0)
				bitmap[gridWidth * 1 + 15] = randomInitialStarColor(); // regular home
			else
				bitmap[gridWidth * 2 + 17] = randomInitialStarColor(); // numlock numpad 7
			break;
		case VK_LEFT:
			if (isE0)
				bitmap[gridWidth * 5 + 15] = randomInitialStarColor(); // regular left arrow
			else
				bitmap[gridWidth * 3 + 17] = randomInitialStarColor(); // numlock numpad 4
			break;
		case VK_UP:
			if (isE0)
				bitmap[gridWidth * 4 + 15] = randomInitialStarColor(); // regular up arrow
			else
				bitmap[gridWidth * 2 + 18] = randomInitialStarColor(); // numlock numpad 8
			break;
		case VK_RIGHT:
			if (isE0)
				bitmap[gridWidth * 5 + 17] = randomInitialStarColor(); // regular right arrow
			else
				bitmap[gridWidth * 3 + 19] = randomInitialStarColor(); // numlock numpad 6
			break;
		case VK_DOWN:
			if (isE0)
				bitmap[gridWidth * 5 + 16] = randomInitialStarColor(); // regular down arrow
			else
				bitmap[gridWidth * 4 + 18] = randomInitialStarColor(); // numlock numpad 2
			break;
		case VK_SNAPSHOT: bitmap[gridWidth * 0 + 13] = randomInitialStarColor(); break;
		case VK_INSERT:
			if (isE0)
				bitmap[gridWidth * 1 + 14] = randomInitialStarColor(); // regular insert
			else
				bitmap[gridWidth * 5 + 18] = randomInitialStarColor(); // numlock numpad 0
			break;
		case VK_DELETE:
			if (isE0)
				bitmap[gridWidth * 2 + 14] = randomInitialStarColor(); // regular delete
			else
				bitmap[gridWidth * 5 + 19] = randomInitialStarColor(); // numlock numpad .
			break;

		// ASCII Number Keys
		case 0x30: bitmap[gridWidth * 1 + 10] = randomInitialStarColor(); break; // 0 - 9
		case 0x31: bitmap[gridWidth * 1 +  1] = randomInitialStarColor(); break;
		case 0x32: bitmap[gridWidth * 1 +  2] = randomInitialStarColor(); break;
		case 0x33: bitmap[gridWidth * 1 +  3] = randomInitialStarColor(); break;
		case 0x34: bitmap[gridWidth * 1 +  4] = randomInitialStarColor(); break;
		case 0x35: bitmap[gridWidth * 1 +  5] = randomInitialStarColor(); break;
		case 0x36: bitmap[gridWidth * 1 +  6] = randomInitialStarColor(); break;
		case 0x37: bitmap[gridWidth * 1 +  7] = randomInitialStarColor(); break;
		case 0x38: bitmap[gridWidth * 1 +  8] = randomInitialStarColor(); break;
		case 0x39: bitmap[gridWidth * 1 +  9] = randomInitialStarColor(); break;

		// ASCII Letter Keys
		case 0x41: bitmap[gridWidth * 3 +  1] = randomInitialStarColor(); break; // A
		case 0x42: bitmap[gridWidth * 4 +  6] = randomInitialStarColor(); break; // B
		case 0x43: bitmap[gridWidth * 4 +  4] = randomInitialStarColor(); break; // C
		case 0x44: bitmap[gridWidth * 3 +  3] = randomInitialStarColor(); break; // D
		case 0x45: bitmap[gridWidth * 2 +  3] = randomInitialStarColor(); break; // E
		case 0x46: bitmap[gridWidth * 3 +  4] = randomInitialStarColor(); break; // F
		case 0x47: bitmap[gridWidth * 3 +  5] = randomInitialStarColor(); break; // G
		case 0x48: bitmap[gridWidth * 3 +  6] = randomInitialStarColor(); break; // H
		case 0x49: bitmap[gridWidth * 2 +  8] = randomInitialStarColor(); break; // I
		case 0x4A: bitmap[gridWidth * 3 +  7] = randomInitialStarColor(); break; // J
		case 0x4B: bitmap[gridWidth * 3 +  8] = randomInitialStarColor(); break; // K
		case 0x4C: bitmap[gridWidth * 3 +  9] = randomInitialStarColor(); break; // L
		case 0x4D: bitmap[gridWidth * 4 +  8] = randomInitialStarColor(); break; // M
		case 0x4E: bitmap[gridWidth * 4 +  7] = randomInitialStarColor(); break; // N
		case 0x4F: bitmap[gridWidth * 2 +  9] = randomInitialStarColor(); break; // O
		case 0x50: bitmap[gridWidth * 2 + 10] = randomInitialStarColor(); break; // P
		case 0x51: bitmap[gridWidth * 2 +  1] = randomInitialStarColor(); break; // Q
		case 0x52: bitmap[gridWidth * 2 +  4] = randomInitialStarColor(); break; // R
		case 0x53: bitmap[gridWidth * 3 +  2] = randomInitialStarColor(); break; // S
		case 0x54: bitmap[gridWidth * 2 +  5] = randomInitialStarColor(); break; // T
		case 0x55: bitmap[gridWidth * 2 +  7] = randomInitialStarColor(); break; // U
		case 0x56: bitmap[gridWidth * 4 +  5] = randomInitialStarColor(); break; // V
		case 0x57: bitmap[gridWidth * 2 +  2] = randomInitialStarColor(); break; // W
		case 0x58: bitmap[gridWidth * 4 +  3] = randomInitialStarColor(); break; // X
		case 0x59: bitmap[gridWidth * 2 +  6] = randomInitialStarColor(); break; // Y
		case 0x5A: bitmap[gridWidth * 4 +  2] = randomInitialStarColor(); break; // Z

		// Windows Keys, Apps Key
		case VK_LWIN: bitmap[gridWidth * 5 +  1] = randomInitialStarColor(); break;
		case VK_RWIN: bitmap[gridWidth * 5 + 12] = randomInitialStarColor(); break;
		case VK_APPS: bitmap[gridWidth * 5 + 13] = randomInitialStarColor(); break;

		// Numpad Keys
		case VK_NUMPAD0:  bitmap[gridWidth * 5 + 18] = randomInitialStarColor(); break; // regular numpad 0
		case VK_NUMPAD1:  bitmap[gridWidth * 4 + 17] = randomInitialStarColor(); break;
		case VK_NUMPAD2:  bitmap[gridWidth * 4 + 18] = randomInitialStarColor(); break;
		case VK_NUMPAD3:  bitmap[gridWidth * 4 + 19] = randomInitialStarColor(); break;
		case VK_NUMPAD4:  bitmap[gridWidth * 3 + 17] = randomInitialStarColor(); break;
		case VK_NUMPAD5:  bitmap[gridWidth * 3 + 18] = randomInitialStarColor(); break; // regular numpad5
		case VK_CLEAR:    bitmap[gridWidth * 3 + 18] = randomInitialStarColor(); break; // numlock'd numpad5
		case VK_NUMPAD6:  bitmap[gridWidth * 3 + 19] = randomInitialStarColor(); break;
		case VK_NUMPAD7:  bitmap[gridWidth * 2 + 17] = randomInitialStarColor(); break;
		case VK_NUMPAD8:  bitmap[gridWidth * 2 + 18] = randomInitialStarColor(); break;
		case VK_NUMPAD9:  bitmap[gridWidth * 2 + 19] = randomInitialStarColor(); break;
		case VK_MULTIPLY: bitmap[gridWidth * 1 + 19] = randomInitialStarColor(); break;
		case VK_ADD:      bitmap[gridWidth * 2 + 20] = randomInitialStarColor(); break;
		case VK_SUBTRACT: bitmap[gridWidth * 1 + 20] = randomInitialStarColor(); break;
		case VK_DECIMAL:  bitmap[gridWidth * 5 + 19] = randomInitialStarColor(); break;
		case VK_DIVIDE:   bitmap[gridWidth * 1 + 18] = randomInitialStarColor(); break;

		// Function Keys
		case VK_F1:  bitmap[gridWidth * 0 +  1] = randomInitialStarColor(); break;
		case VK_F2:  bitmap[gridWidth * 0 +  2] = randomInitialStarColor(); break;
		case VK_F3:  bitmap[gridWidth * 0 +  3] = randomInitialStarColor(); break;
		case VK_F4:  bitmap[gridWidth * 0 +  4] = randomInitialStarColor(); break;
		case VK_F5:  bitmap[gridWidth * 0 +  5] = randomInitialStarColor(); break;
		case VK_F6:  bitmap[gridWidth * 0 +  6] = randomInitialStarColor(); break;
		case VK_F7:  bitmap[gridWidth * 0 +  7] = randomInitialStarColor(); break;
		case VK_F8:  bitmap[gridWidth * 0 +  8] = randomInitialStarColor(); break;
		case VK_F9:  bitmap[gridWidth * 0 +  9] = randomInitialStarColor(); break;
		case VK_F10: bitmap[gridWidth * 0 + 10] = randomInitialStarColor(); break;
		case VK_F11: bitmap[gridWidth * 0 + 11] = randomInitialStarColor(); break;
		case VK_F12: bitmap[gridWidth * 0 + 12] = randomInitialStarColor(); break;

		// Numlock, Scroll Lock
		case VK_NUMLOCK: bitmap[gridWidth * 1 + 17] = randomInitialStarColor(); break;
		case VK_SCROLL: bitmap[gridWidth * 0 + 14] = randomInitialStarColor(); break;

		// Both Shift Keys
		case VK_LSHIFT: bitmap[gridWidth * 4 + 0] = randomInitialStarColor(); break;
		case VK_RSHIFT: bitmap[gridWidth * 4 + 13] = randomInitialStarColor(); break;

		// Various OEM Keys (punctuation)
		case VK_OEM_1: bitmap[gridWidth * 3 + 10] = randomInitialStarColor(); break; // '; break;:' for US
		case VK_OEM_PLUS: bitmap[gridWidth * 1 + 12] = randomInitialStarColor(); break; // '+' any country
		case VK_OEM_COMMA: bitmap[gridWidth * 4 + 9] = randomInitialStarColor(); break; // ',' any country
		case VK_OEM_MINUS: bitmap[gridWidth * 1 + 11] = randomInitialStarColor(); break; // '-' any country
		case VK_OEM_PERIOD: bitmap[gridWidth * 4 + 10] = randomInitialStarColor(); break; // '.' any country
		case VK_OEM_2: bitmap[gridWidth * 4 + 11] = randomInitialStarColor(); break; // '/?' for US
		case VK_OEM_3: bitmap[gridWidth * 1 + 0] = randomInitialStarColor(); break; // '`~' for US
		case VK_OEM_4: bitmap[gridWidth * 2 + 11] = randomInitialStarColor(); break; // '[{' for US
		case VK_OEM_5: bitmap[gridWidth * 2 + 13] = randomInitialStarColor(); break; // '\|' for US
		case VK_OEM_6: bitmap[gridWidth * 2 + 12] = randomInitialStarColor(); break; // ']}' for US
		case VK_OEM_7: bitmap[gridWidth * 3 + 11] = randomInitialStarColor(); break; // ''"' for US
		}
	}
}