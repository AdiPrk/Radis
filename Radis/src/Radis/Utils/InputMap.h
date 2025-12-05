/***************************************************************************************
* The numbers used in the mapping are from GLFW, since glfw is used for the input system 
****************************************************************************************/

// there were many reasons why it's all hard coded numbers
// those reasons don't exist anymore but the hard coded numbers do '.'

#pragma once

namespace Dog {

    enum class Key
    {
        // Unknown Key
        UNKNOWN = -1,

        // Printable Keys
        SPACE = 32,
        APOSTROPHE = 39,
        COMMA = 44,
        MINUS = 45,
        PERIOD = 46,
        SLASH = 47,
        NUM0 = 48,
        NUM1 = 49,
        NUM2 = 50,
        NUM3 = 51,
        NUM4 = 52,
        NUM5 = 53,
        NUM6 = 54,
        NUM7 = 55,
        NUM8 = 56,
        NUM9 = 57,
        SEMICOLON = 59,
        EQUAL = 61,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        LEFTBRACKET = 91,
        BACKSLASH = 92,
        RIGHTBRACKET = 93,
        GRAVEACCENT = 96,

        // Function Keys
        ESCAPE = 256,
        ENTER = 257,
        TAB = 258,
        BACKSPACE = 259,
        INSERT = 260,
        DEL = 261, // Delete
        RIGHT = 262,
        LEFT = 263,
        DOWN = 264,
        UP = 265,
        PAGEUP = 266,
        PAGEDOWN = 267,
        HOME = 268,
        END = 269,
        CAPSLOCK = 280,
        SCROLLLOCK = 281,
        NUMLOCK = 282,
        PRINTSCREEN = 283,
        BREAK = 284,
        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,
        KEYPAD0 = 320,
        KEYPAD1 = 321,
        KEYPAD2 = 322,
        KEYPAD3 = 323,
        KEYPAD4 = 324,
        KEYPAD5 = 325,
        KEYPAD6 = 326,
        KEYPAD7 = 327,
        KEYPAD8 = 328,
        KEYPAD9 = 329,
        KEYPADDECIMAL = 330,
        KEYPADMULTIPLY = 332,
        KEYPADDIVIDE = 331,
        KEYPADMINUS = 333,
        KEYPADPLUS = 334,
        KEYPADENTER = 335,
        KEYPADEQUAL = 336,
        LEFTSHIFT = 340,
        LEFTCONTROL = 341,
        LEFTALT = 342,
        LEFTSUPER = 343,
        RIGHTSHIFT = 344,
        RIGHTCONTROL = 345,
        RIGHTALT = 346,
        RIGHTSUPER = 347,
        MENU = 348,

        LAST = 349
    };

    enum class Mouse
    {
        LEFT = 0,
        RIGHT = 1,
        MIDDLE = 2,
        BUTTON4 = 3,
        BUTTON5 = 4,
        BUTTON6 = 5,
        BUTTON7 = 6,
        BUTTON8 = 7,
        LAST = 8
    };

} // namespace Dog
