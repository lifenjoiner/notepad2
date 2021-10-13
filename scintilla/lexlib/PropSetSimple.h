// Scintilla source code edit control
/** @file PropSetSimple.h
 ** A basic string to string map.
 **/
// Copyright 1998-2009 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.
#pragma once

namespace Lexilla {

class PropSetSimple final {
	void *impl;
public:
	PropSetSimple();
	// Deleted so PropSetSimple objects can not be copied.
	PropSetSimple(const PropSetSimple&) = delete;
	PropSetSimple(PropSetSimple&&) = delete;
	PropSetSimple &operator=(const PropSetSimple&) = delete;
	PropSetSimple &operator=(PropSetSimple&&) = delete;
	~PropSetSimple();

	bool Set(std::string_view key, std::string_view val);
	const char *Get(std::string_view key) const;
	int GetInt(const char *key, size_t keyLen, int defaultValue = 0) const;

	template <size_t N>
	int GetInt(const char (&key)[N], int defaultValue = 0) const {
		return GetInt(key, N - 1, defaultValue);
	}
};

}
