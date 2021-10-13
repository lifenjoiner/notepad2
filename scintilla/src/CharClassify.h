// Scintilla source code edit control
/** @file CharClassify.h
 ** Character classifications used by Document and RESearch.
 **/
// Copyright 2006-2009 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.
#pragma once

namespace Scintilla::Internal {

constexpr bool IsDBCSCodePage(int codePage) noexcept {
	return codePage == 932
		|| codePage == 936
		|| codePage == 949
		|| codePage == 950
		|| codePage == 1361;
}

constexpr bool IsDBCSValidSingleByte(int codePage, int ch) noexcept {
	switch (codePage) {
	case 932:
		return ch == 0x80
			|| (ch >= 0xA0 && ch <= 0xDF)
			|| (ch >= 0xFD);

	default:
		return false;
	}
}

enum class CharacterClass : unsigned char { space, newLine, word, punctuation, cjkWord };

class CharClassify {
public:
	CharClassify() noexcept;

	void SetDefaultCharClasses(bool includeWordClass) noexcept;
	void SetCharClasses(const unsigned char *chars, CharacterClass newCharClass) noexcept;
	void SetCharClassesEx(const unsigned char *chars, size_t length) noexcept;
	int GetCharsOfClass(CharacterClass characterClass, unsigned char *buffer) const noexcept;
	CharacterClass GetClass(unsigned char ch) const noexcept {
		return charClass[ch];
	}
	bool IsWord(unsigned char ch) const noexcept {
		return charClass[ch] == CharacterClass::word;
	}

	static void InitUnicodeData() noexcept;

//++Autogenerated -- start of section automatically generated
// Created with Python 3.11.0a1, Unicode 14.0.0
	static CharacterClass ClassifyCharacter(unsigned int ch) noexcept {
		if (ch < sizeof(classifyMap)) {
			return static_cast<CharacterClass>(classifyMap[ch]);
		}
		if (ch > maxUnicode) {
			// Cn
			return CharacterClass::space;
		}

		ch -= sizeof(classifyMap);
		ch = (CharClassifyTable[ch >> 9] << 7) | (ch & 511);
		ch = (CharClassifyTable[2048 + (ch >> 3)] << 2) | (ch & 7);
		return static_cast<CharacterClass>(CharClassifyTable[5824 + ch]);
	}
//--Autogenerated -- end of section automatically generated

private:
	static constexpr unsigned int maxUnicode = 0x10ffff;
	static const unsigned char CharClassifyTable[];
	static unsigned char classifyMap[0xffff + 1];

	static constexpr int maxChar = 256;
	CharacterClass charClass[maxChar];
};

class DBCSCharClassify {
public:
	static const DBCSCharClassify* Get(int codePage);

	bool IsLeadByte(unsigned char ch) const noexcept {
		return leadByte[ch];
	}
	bool IsTrailByte(unsigned char ch) const noexcept {
		return trailByte[ch];
	}

	CharacterClass ClassifyCharacter(unsigned int ch) const noexcept {
		if (ch < sizeof(classifyMap)) {
			return static_cast<CharacterClass>(classifyMap[ch]);
		}
		// Cn
		return CharacterClass::space;
	}

	constexpr int CodePage() const noexcept {
		return codePage;
	}
	constexpr int MinTrailByte() const noexcept {
		return minTrailByte;
	}

private:
	explicit DBCSCharClassify(int codePage_) noexcept;

	const int codePage;
	int minTrailByte;
	bool leadByte[256];
	bool trailByte[256];
	unsigned char classifyMap[0xffff + 1];
};

}
