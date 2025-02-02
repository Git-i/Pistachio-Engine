#pragma once
typedef int KeyCode;
namespace Pistachio
{
	//these are consistent across glfw and win32
	constexpr KeyCode KEY_0 = 48;
	constexpr KeyCode KEY_1 = 49;
	constexpr KeyCode KEY_2 = 50;
	constexpr KeyCode KEY_3 = 51;
	constexpr KeyCode KEY_4 = 52;
	constexpr KeyCode KEY_5 = 53;
	constexpr KeyCode KEY_6 = 54;
	constexpr KeyCode KEY_7 = 55;
	constexpr KeyCode KEY_8 = 56;
	constexpr KeyCode KEY_9 = 57;
	constexpr KeyCode KEY_A = 65;
	constexpr KeyCode KEY_B = 66;
	constexpr KeyCode KEY_C = 67;
	constexpr KeyCode KEY_D = 68;
	constexpr KeyCode KEY_E = 69;
	constexpr KeyCode KEY_F = 70;
	constexpr KeyCode KEY_G = 71;
	constexpr KeyCode KEY_H = 72;
	constexpr KeyCode KEY_I = 73;
	constexpr KeyCode KEY_J = 74;
	constexpr KeyCode KEY_K = 75;
	constexpr KeyCode KEY_L = 76;
	constexpr KeyCode KEY_M = 77;
	constexpr KeyCode KEY_N = 78;
	constexpr KeyCode KEY_O = 79;
	constexpr KeyCode KEY_P = 80;
	constexpr KeyCode KEY_Q = 81;
	constexpr KeyCode KEY_R = 82;
	constexpr KeyCode KEY_S = 83;
	constexpr KeyCode KEY_T = 84;
	constexpr KeyCode KEY_U = 85;
	constexpr KeyCode KEY_V = 86;
	constexpr KeyCode KEY_W = 87;
	constexpr KeyCode KEY_X = 88;
	constexpr KeyCode KEY_Y = 89;
	constexpr KeyCode KEY_Z = 90;
#if defined(PT_PLATFORM_LINUX)
	constexpr KeyCode KEY_UNKNOWN = -1;

	//ripped straight from glfw
	constexpr KeyCode KEY_SPACE              = 32;
	constexpr KeyCode KEY_APOSTROPHE         = 39;  /* ' */
	constexpr KeyCode KEY_COMMA              = 44;  /* , */
	constexpr KeyCode KEY_MINUS              = 45;  /* - */
	constexpr KeyCode KEY_PERIOD             = 46;  /* . */
	constexpr KeyCode KEY_SLASH              = 47;  /* / */
	constexpr KeyCode KEY_SEMICOLON          = 59;  /* ; */
	constexpr KeyCode KEY_EQUAL              = 61;  /* = */
	constexpr KeyCode KEY_LEFT_BRACKET       = 91;  /* [ */
	constexpr KeyCode KEY_BACKSLASH          = 92;  /* \ */
	constexpr KeyCode KEY_RIGHT_BRACKET      = 93;  /* ] */
	constexpr KeyCode KEY_GRAVE_ACCENT       = 96;  /* ` */
	constexpr KeyCode KEY_WORLD_1            = 161; /* non-US #1 */
	constexpr KeyCode KEY_WORLD_2            = 162; /* non-US #2 */
	
	/* Function keys */
	constexpr KeyCode KEY_ESCAPE             = 256;
	constexpr KeyCode KEY_ENTER              = 257;
	constexpr KeyCode KEY_TAB                = 258;
	constexpr KeyCode KEY_BACKSPACE          = 259;
	constexpr KeyCode KEY_INSERT             = 260;
	constexpr KeyCode KEY_DELETE             = 261;
	constexpr KeyCode KEY_RIGHT              = 262;
	constexpr KeyCode KEY_LEFT               = 263;
	constexpr KeyCode KEY_DOWN               = 264;
	constexpr KeyCode KEY_UP                 = 265;
	constexpr KeyCode KEY_PAGE_UP            = 266;
	constexpr KeyCode KEY_PAGE_DOWN          = 267;
	constexpr KeyCode KEY_HOME               = 268;
	constexpr KeyCode KEY_END                = 269;
	constexpr KeyCode KEY_CAPS_LOCK          = 280;
	constexpr KeyCode KEY_SCROLL_LOCK        = 281;
	constexpr KeyCode KEY_NUM_LOCK           = 282;
	constexpr KeyCode KEY_PRINT_SCREEN       = 283;
	constexpr KeyCode KEY_PAUSE              = 284;
	constexpr KeyCode KEY_F1                 = 290;
	constexpr KeyCode KEY_F2                 = 291;
	constexpr KeyCode KEY_F3                 = 292;
	constexpr KeyCode KEY_F4                 = 293;
	constexpr KeyCode KEY_F5                 = 294;
	constexpr KeyCode KEY_F6                 = 295;
	constexpr KeyCode KEY_F7                 = 296;
	constexpr KeyCode KEY_F8                 = 297;
	constexpr KeyCode KEY_F9                 = 298;
	constexpr KeyCode KEY_F10                = 299;
	constexpr KeyCode KEY_F11                = 300;
	constexpr KeyCode KEY_F12                = 301;
	constexpr KeyCode KEY_F13                = 302;
	constexpr KeyCode KEY_F14                = 303;
	constexpr KeyCode KEY_F15                = 304;
	constexpr KeyCode KEY_F16                = 305;
	constexpr KeyCode KEY_F17                = 306;
	constexpr KeyCode KEY_F18                = 307;
	constexpr KeyCode KEY_F19                = 308;
	constexpr KeyCode KEY_F20                = 309;
	constexpr KeyCode KEY_F21                = 310;
	constexpr KeyCode KEY_F22                = 311;
	constexpr KeyCode KEY_F23                = 312;
	constexpr KeyCode KEY_F24                = 313;
	constexpr KeyCode KEY_F25                = 314;
	constexpr KeyCode KEY_KP_0               = 320;
	constexpr KeyCode KEY_KP_1               = 321;
	constexpr KeyCode KEY_KP_2               = 322;
	constexpr KeyCode KEY_KP_3               = 323;
	constexpr KeyCode KEY_KP_4               = 324;
	constexpr KeyCode KEY_KP_5               = 325;
	constexpr KeyCode KEY_KP_6               = 326;
	constexpr KeyCode KEY_KP_7               = 327;
	constexpr KeyCode KEY_KP_8               = 328;
	constexpr KeyCode KEY_KP_9               = 329;
	constexpr KeyCode KEY_KP_DECIMAL         = 330;
	constexpr KeyCode KEY_KP_DIVIDE          = 331;
	constexpr KeyCode KEY_KP_MULTIPLY        = 332;
	constexpr KeyCode KEY_KP_SUBTRACT        = 333;
	constexpr KeyCode KEY_KP_ADD             = 334;
	constexpr KeyCode KEY_KP_ENTER           = 335;
	constexpr KeyCode KEY_KP_EQUAL           = 336;
	constexpr KeyCode KEY_LEFT_SHIFT         = 340;
	constexpr KeyCode KEY_LEFT_CONTROL       = 341;
	constexpr KeyCode KEY_LEFT_ALT           = 342;
	constexpr KeyCode KEY_LEFT_SUPER         = 343;
	constexpr KeyCode KEY_RIGHT_SHIFT        = 344;
	constexpr KeyCode KEY_RIGHT_CONTROL      = 345;
	constexpr KeyCode KEY_RIGHT_ALT          = 346;
	constexpr KeyCode KEY_RIGHT_SUPER        = 347;
	constexpr KeyCode KEY_MENU               = 348;
	
	constexpr KeyCode KEY_LAST			     = KEY_MENU;
	
	constexpr KeyCode MOUSE_BUTTON_1         = 0;
	constexpr KeyCode MOUSE_BUTTON_2         = 1;
	constexpr KeyCode MOUSE_BUTTON_3         = 2;
	constexpr KeyCode MOUSE_BUTTON_4         = 3;
	constexpr KeyCode MOUSE_BUTTON_5         = 4;
	constexpr KeyCode MOUSE_BUTTON_6         = 5;
	constexpr KeyCode MOUSE_BUTTON_7         = 6;
	constexpr KeyCode MOUSE_BUTTON_8         = 7;
	constexpr KeyCode MOUSE_BUTTON_LAST      = MOUSE_BUTTON_8;
	constexpr KeyCode MOUSE_BUTTON_LEFT      = MOUSE_BUTTON_1;
	constexpr KeyCode MOUSE_BUTTON_RIGHT     = MOUSE_BUTTON_2;
	constexpr KeyCode MOUSE_BUTTON_MIDDLE    = MOUSE_BUTTON_3;
#elif defined(PT_PLATFORM_WINDOWS)
constexpr KeyCode KEY_UNKNOWN = -1;

//ripped straight from win32
constexpr KeyCode KEY_SPACE = 32;
constexpr KeyCode KEY_APOSTROPHE = 39;  /* ' */
constexpr KeyCode KEY_COMMA = 44;  /* , */
constexpr KeyCode KEY_MINUS = 45;  /* - */
constexpr KeyCode KEY_PERIOD = 46;  /* . */
constexpr KeyCode KEY_SLASH = 47;  /* / */

constexpr KeyCode KEY_SEMICOLON = 59;  /* ; */
constexpr KeyCode KEY_EQUAL = 61;  /* = */

constexpr KeyCode KEY_LEFT_BRACKET = 91;  /* [ */
constexpr KeyCode KEY_BACKSLASH = 92;  /* \ */
constexpr KeyCode KEY_RIGHT_BRACKET = 93;  /* ] */
constexpr KeyCode KEY_GRAVE_ACCENT = 96;  /* ` */
constexpr KeyCode KEY_WORLD_1 = 161; /* non-US #1 */
constexpr KeyCode KEY_WORLD_2 = 162; /* non-US #2 */

/* Function keys */
constexpr KeyCode KEY_ESCAPE = 256;
constexpr KeyCode KEY_ENTER = 257;
constexpr KeyCode KEY_TAB = 258;
constexpr KeyCode KEY_BACKSPACE = 259;
constexpr KeyCode KEY_INSERT = 260;
constexpr KeyCode KEY_DELETE = 261;
constexpr KeyCode KEY_RIGHT = 262;
constexpr KeyCode KEY_LEFT = 263;
constexpr KeyCode KEY_DOWN = 264;
constexpr KeyCode KEY_UP = 265;
constexpr KeyCode KEY_PAGE_UP = 266;
constexpr KeyCode KEY_PAGE_DOWN = 267;
constexpr KeyCode KEY_HOME = 268;
constexpr KeyCode KEY_END = 269;
constexpr KeyCode KEY_CAPS_LOCK = 280;
constexpr KeyCode KEY_SCROLL_LOCK = 281;
constexpr KeyCode KEY_NUM_LOCK = 282;
constexpr KeyCode KEY_PRINT_SCREEN = 283;
constexpr KeyCode KEY_PAUSE = 284;
constexpr KeyCode KEY_F1 = 290;
constexpr KeyCode KEY_F2 = 291;
constexpr KeyCode KEY_F3 = 292;
constexpr KeyCode KEY_F4 = 293;
constexpr KeyCode KEY_F5 = 294;
constexpr KeyCode KEY_F6 = 295;
constexpr KeyCode KEY_F7 = 296;
constexpr KeyCode KEY_F8 = 297;
constexpr KeyCode KEY_F9 = 298;
constexpr KeyCode KEY_F10 = 299;
constexpr KeyCode KEY_F11 = 300;
constexpr KeyCode KEY_F12 = 301;
constexpr KeyCode KEY_F13 = 302;
constexpr KeyCode KEY_F14 = 303;
constexpr KeyCode KEY_F15 = 304;
constexpr KeyCode KEY_F16 = 305;
constexpr KeyCode KEY_F17 = 306;
constexpr KeyCode KEY_F18 = 307;
constexpr KeyCode KEY_F19 = 308;
constexpr KeyCode KEY_F20 = 309;
constexpr KeyCode KEY_F21 = 310;
constexpr KeyCode KEY_F22 = 311;
constexpr KeyCode KEY_F23 = 312;
constexpr KeyCode KEY_F24 = 313;
constexpr KeyCode KEY_F25 = 314;
constexpr KeyCode KEY_KP_0 = 320;
constexpr KeyCode KEY_KP_1 = 321;
constexpr KeyCode KEY_KP_2 = 322;
constexpr KeyCode KEY_KP_3 = 323;
constexpr KeyCode KEY_KP_4 = 324;
constexpr KeyCode KEY_KP_5 = 325;
constexpr KeyCode KEY_KP_6 = 326;
constexpr KeyCode KEY_KP_7 = 327;
constexpr KeyCode KEY_KP_8 = 328;
constexpr KeyCode KEY_KP_9 = 329;
constexpr KeyCode KEY_KP_DECIMAL = 330;
constexpr KeyCode KEY_KP_DIVIDE = 331;
constexpr KeyCode KEY_KP_MULTIPLY = 332;
constexpr KeyCode KEY_KP_SUBTRACT = 333;
constexpr KeyCode KEY_KP_ADD = 334;
constexpr KeyCode KEY_KP_ENTER = 335;
constexpr KeyCode KEY_KP_EQUAL = 336;
constexpr KeyCode KEY_LEFT_SHIFT = 0xA0;
constexpr KeyCode KEY_LEFT_CONTROL = 0xA2;
constexpr KeyCode KEY_LEFT_ALT = 0XA4;
constexpr KeyCode KEY_LEFT_SUPER = 343;
constexpr KeyCode KEY_RIGHT_SHIFT = 0xA1;
constexpr KeyCode KEY_RIGHT_CONTROL = 0xA3;
constexpr KeyCode KEY_RIGHT_ALT = 0xA5;
constexpr KeyCode KEY_RIGHT_SUPER = 347;
constexpr KeyCode KEY_MENU = 348;

constexpr KeyCode KEY_LAST = KEY_MENU;

constexpr KeyCode MOUSE_BUTTON_1 = 1;
constexpr KeyCode MOUSE_BUTTON_2 = 2;
constexpr KeyCode MOUSE_BUTTON_3 = 4;
constexpr KeyCode MOUSE_BUTTON_4 = 5;
constexpr KeyCode MOUSE_BUTTON_5 = 6;
constexpr KeyCode MOUSE_BUTTON_6 = -1;
constexpr KeyCode MOUSE_BUTTON_7 = -1;
constexpr KeyCode MOUSE_BUTTON_8 = -1;
constexpr KeyCode MOUSE_BUTTON_LAST = MOUSE_BUTTON_8;
constexpr KeyCode MOUSE_BUTTON_LEFT = MOUSE_BUTTON_1;
constexpr KeyCode MOUSE_BUTTON_RIGHT = MOUSE_BUTTON_2;
constexpr KeyCode MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_3;
#endif
}