#pragma once
// Font Awesome 5 Solid icon glyphs used by SuiteSpot.
// Encoded as explicit UTF-8 hex bytes to avoid \uXXXX escape ambiguity on MSVC.
// Font file: fa-solid-900.ttf (glyph range 0xF000-0xF8D9)
//
// UTF-8 encoding for U+F000-U+FFFF: EF 8x xx
//   byte1 = 0xEF
//   byte2 = 0x80 | (codepoint >> 6 & 0x3F)
//   byte3 = 0x80 | (codepoint       & 0x3F)

#define ICON_FA_SEARCH          "\xef\x80\x82"  // U+F002
#define ICON_FA_CLOCK           "\xef\x80\x97"  // U+F017
#define ICON_FA_DOWNLOAD        "\xef\x80\x99"  // U+F019
#define ICON_FA_LIST            "\xef\x80\xba"  // U+F03A
#define ICON_FA_PLAY            "\xef\x81\x8b"  // U+F04B
#define ICON_FA_STOP            "\xef\x81\x8d"  // U+F04D
#define ICON_FA_GLOBE           "\xef\x82\xac"  // U+F0AC
#define ICON_FA_COGS            "\xef\x82\x85"  // U+F085
#define ICON_FA_COPY            "\xef\x83\x85"  // U+F0C5
#define ICON_FA_SAVE            "\xef\x83\x87"  // U+F0C7
#define ICON_FA_CIRCLE          "\xef\x84\x91"  // U+F111
#define ICON_FA_KEYBOARD        "\xef\x84\x9c"  // U+F11C
#define ICON_FA_GRADUATION_CAP  "\xef\x86\x9d"  // U+F19D
#define ICON_FA_MAP             "\xef\x89\xb9"  // U+F279
