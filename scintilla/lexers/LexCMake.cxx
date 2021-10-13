// This file is part of Notepad2.
// See License.txt for details about distribution and modification.
//! Lexer for CMake.

#include <cassert>
#include <cstring>

#include <string>
#include <string_view>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "StringUtils.h"
#include "LexerModule.h"

using namespace Lexilla;

namespace {

constexpr bool IsCMakeOperator(int ch) noexcept {
	return AnyOf(ch, '(', ')', '=', ':', ';',
					 '$', '<', '>', ','); // Generator expressions
}

constexpr bool IsCMakeChar(int ch) noexcept {
	return IsIdentifierChar(ch) || ch == '.' || ch == '-' || ch == '+';
}

bool IsBracketArgument(Accessor &styler, Sci_PositionU pos, bool start, int &bracketNumber) noexcept {
	int offset = 0;
	char ch;
	++pos; // bracket
	while ((ch = styler.SafeGetCharAt(pos)) == '=') {
		++offset;
		++pos;
	}

	if (start) {
		if (ch == '[') {
			bracketNumber = offset;
			return true;
		}
	} else {
		if (ch == ']' && offset == bracketNumber) {
			return true;
		}
	}
	return false;
}

void ColouriseCMakeDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	int lineStateLineComment = 0;

	int outerStyle = SCE_CMAKE_DEFAULT;
	int varNestedLevel = 0; // nested variable: ${${}}
	int generatorExpr = 0; // nested generator expressions: $<$<>>
	int bracketNumber = 0; // number of '=' in bracket: [[]]
	int userDefType = SCE_CMAKE_DEFAULT;
	int chBeforeNumber = 0;
	int chIdentifierStart = 0;
	int visibleChars = 0;

	StyleContext sc(startPos, lengthDoc, initStyle, styler);
	if (sc.currentLine > 0) {
		const int lineState = styler.GetLineState(sc.currentLine - 1);
		/*
		1: lineStateLineComment
		7: outerStyle
		8: bracketNumber
		*/
		outerStyle = (lineState >> 1) & 0x7f;
		bracketNumber = (lineState >> 8) & 0xff;
		if (outerStyle != SCE_CMAKE_DEFAULT) {
			sc.SetState(outerStyle);
		}
	}

	while (sc.More()) {
		switch (sc.state) {
		case SCE_CMAKE_OPERATOR:
			sc.SetState(SCE_CMAKE_DEFAULT);
			break;

		case SCE_CMAKE_NUMBER:
			if (!IsNumberStart(sc.ch, sc.chNext)) {
				if (IsCMakeChar(sc.ch) || IsCMakeChar(chBeforeNumber)) {
					sc.ChangeState(SCE_CMAKE_DEFAULT);
				}
				sc.SetState(SCE_CMAKE_DEFAULT);
			}
			break;

		case SCE_CMAKE_IDENTIFIER:
			if (!(IsIdentifierChar(sc.ch) || sc.ch == '-')) {
				const int chNext = sc.GetDocNextChar();
				if (chNext == '(') {
					// command, function and macro are case insensitive
					// see Command Invocations: space* identifier space* '(' arguments ')'
					char s[128];
					sc.GetCurrentLowered(s, sizeof(s));
					userDefType = SCE_CMAKE_DEFAULT;
					if (keywordLists[0]->InListPrefixed(s, '(')) {
						sc.ChangeState(SCE_CMAKE_WORD);
						if (StrEqual(s, "function")) {
							userDefType = SCE_CMAKE_FUNCATION;
						} else if (StrEqual(s, "macro")) {
							userDefType = SCE_CMAKE_MACRO;
						}
					} else if (keywordLists[1]->InListPrefixed(s, '(')) {
						sc.ChangeState(SCE_CMAKE_COMMANDS);
					} else {
						sc.ChangeState(SCE_CMAKE_FUNCATION);
					}
				} else if (userDefType != SCE_CMAKE_DEFAULT) {
					sc.ChangeState(userDefType);
					userDefType = SCE_CMAKE_DEFAULT;
				} else if (IsUpperCase(chIdentifierStart)) {
					// case sensitive
					char s[128];
					sc.GetCurrent(s, sizeof(s));
					if (keywordLists[2]->InList(s)) {
						sc.ChangeState(SCE_CMAKE_PARAMETERS);
					} else if (keywordLists[3]->InList(s)) {
						sc.ChangeState(SCE_CMAKE_PROPERTIES);
					} else if (keywordLists[4]->InList(s)) {
						sc.ChangeState(SCE_CMAKE_VARIABLE);
					} else if (keywordLists[5]->InList(s)) {
						sc.ChangeState(SCE_CMAKE_VALUES);
					}
				}
				sc.SetState(SCE_CMAKE_DEFAULT);
			}
			break;

		case SCE_CMAKE_COMMENT:
			if (sc.atLineStart) {
				sc.SetState(SCE_CMAKE_DEFAULT);
			}
			break;

		case SCE_CMAKE_BLOCK_COMMENT:
			if (sc.ch == ']' && (sc.chNext == '=' || sc.chNext == ']')) {
				if (IsBracketArgument(styler, sc.currentPos, false, bracketNumber)) {
					sc.Advance(1 + bracketNumber);
					sc.ForwardSetState(SCE_CMAKE_DEFAULT);
					bracketNumber = 0;
				}
			}
			break;

		case SCE_CMAKE_STRING:
			if (sc.ch == '\\') {
				if (IsEOLChar(sc.chNext)) {
					sc.SetState(SCE_CMAKE_LINE_CONTINUE);
					sc.ForwardSetState(SCE_CMAKE_STRING);
				} else {
					sc.SetState(SCE_CMAKE_ESCAPE_SEQUENCE);
					sc.Forward();
				}
			} else if (sc.Match('$', '{')) {
				varNestedLevel = 1;
				sc.SetState(SCE_CMAKE_VARIABLE);
			} else if (sc.Match('$', '<')) {
				generatorExpr = 1;
				sc.SetState(SCE_CMAKE_OPERATOR);
			} else if ((sc.ch == '$' || sc.ch == '@') && IsIdentifierStart(sc.chNext)) {
				sc.SetState((sc.ch == '$') ? SCE_CMAKE_VARIABLE_DOLLAR : SCE_CMAKE_VARIABLE_AT);
			} else if (generatorExpr && IsCMakeOperator(sc.ch)) {
				if (sc.ch == '>') {
					--generatorExpr;
				}
				sc.SetState(SCE_CMAKE_OPERATOR);
				sc.ForwardSetState(SCE_CMAKE_STRING);
				continue;
			} else if (sc.ch == '\"') {
				sc.ForwardSetState(SCE_CMAKE_DEFAULT);
				outerStyle = SCE_CMAKE_DEFAULT;
			}
			break;

		case SCE_CMAKE_ESCAPE_SEQUENCE:
			if (sc.ch == '\\') {
				if (IsEOLChar(sc.chNext)) {
					sc.SetState(SCE_CMAKE_LINE_CONTINUE);
					sc.ForwardSetState(outerStyle);
				} else {
					sc.Forward();
				}
			} else {
				sc.SetState(outerStyle);
				if (outerStyle != SCE_CMAKE_DEFAULT) {
					continue;
				}
			}
			break;

		case SCE_CMAKE_BRACKET_ARGUMENT:
			if (sc.ch == ']' && (sc.chNext == '=' || sc.chNext == ']')) {
				if (IsBracketArgument(styler, sc.currentPos, false, bracketNumber)) {
					sc.Advance(1 + bracketNumber);
					sc.ForwardSetState(SCE_CMAKE_DEFAULT);
					bracketNumber = 0;
				}
			}
			break;

		case SCE_CMAKE_VARIABLE:
			if (sc.ch == '}') {
				--varNestedLevel;
				if (varNestedLevel == 0) {
					sc.ForwardSetState(outerStyle);
					if (outerStyle != SCE_CMAKE_DEFAULT) {
						continue;
					}
				}
			} else if (sc.Match('$', '{')) {
				++varNestedLevel;
			}
			break;

		case SCE_CMAKE_VARIABLE_DOLLAR:
		case SCE_CMAKE_VARIABLE_AT:
			if (!IsIdentifierChar(sc.ch)) {
				bool done = false;
				if (sc.state == SCE_CMAKE_VARIABLE_AT) {
					if (sc.ch == '@') {
						sc.Forward();
					}
				} else if (sc.ch == '{') {
					char s[8];
					sc.GetCurrent(s, sizeof(s));
					if (StrEqualsAny(s, "$ENV", "$CACHE")) {
						sc.SetState(SCE_CMAKE_VARIABLE);
						varNestedLevel = 1;
						done = true;
					}
				}
				if (!done) {
					sc.SetState(outerStyle);
					if (outerStyle != SCE_CMAKE_DEFAULT) {
						continue;
					}
				}
			}
			break;
		}

		if (sc.state == SCE_CMAKE_DEFAULT) {
			if (sc.ch == '#') {
				if (sc.chNext == '[' && IsBracketArgument(styler, sc.currentPos + 1, true, bracketNumber)) {
					sc.SetState(SCE_CMAKE_BLOCK_COMMENT);
					sc.Advance(2 + bracketNumber);
				} else {
					sc.SetState(SCE_CMAKE_COMMENT);
					if (visibleChars == 0) {
						lineStateLineComment = SimpleLineStateMaskLineComment;
					}
				}
			} else if (sc.ch == '[' && (sc.chNext == '=' || sc.chNext == '[')) {
				if (IsBracketArgument(styler, sc.currentPos, true, bracketNumber)) {
					sc.SetState(SCE_CMAKE_BRACKET_ARGUMENT);
					sc.Advance(2 + bracketNumber);
				}
			} else if (sc.Match('/', '/')) { // CMakeCache.txt
				sc.SetState(SCE_CMAKE_COMMENT);
				if (visibleChars == 0) {
					lineStateLineComment = SimpleLineStateMaskLineComment;
				}
			} else if (sc.ch == '\"') {
				outerStyle = SCE_CMAKE_STRING;
				sc.SetState(SCE_CMAKE_STRING);
			} else if (sc.Match('$', '{')) {
				varNestedLevel = 1;
				outerStyle = generatorExpr ? outerStyle : SCE_CMAKE_DEFAULT;
				sc.SetState(SCE_CMAKE_VARIABLE);
			} else if ((sc.ch == '$' || sc.ch == '@') && IsIdentifierStart(sc.chNext)) {
				outerStyle = generatorExpr ? outerStyle : SCE_CMAKE_DEFAULT;
				sc.SetState((sc.ch == '$') ? SCE_CMAKE_VARIABLE_DOLLAR : SCE_CMAKE_VARIABLE_AT);
				sc.Forward();
			} else if (sc.ch == '\\') {
				sc.SetState(SCE_CMAKE_ESCAPE_SEQUENCE);
				sc.Forward();
			} else if (IsIdentifierStart(sc.ch)) {
				chIdentifierStart = sc.ch;
				sc.SetState(SCE_CMAKE_IDENTIFIER);
			} else if (IsADigit(sc.ch) || (sc.ch == '-' && IsADigit(sc.chNext))) {
				sc.SetState(SCE_CMAKE_NUMBER);
				chBeforeNumber = sc.chPrev;
			} else if (IsCMakeOperator(sc.ch)) {
				sc.SetState(SCE_CMAKE_OPERATOR);
				if (generatorExpr) {
					if (sc.Match('$', '<')) {
						++generatorExpr;
					} else if (sc.ch == '>') {
						--generatorExpr;
						sc.ForwardSetState(generatorExpr ? SCE_CMAKE_DEFAULT : outerStyle);
						continue;
					}
				}
			}
		}

		if (visibleChars == 0 && !isspacechar(sc.ch)) {
			visibleChars++;
		}
		if (sc.atLineEnd) {
			styler.SetLineState(sc.currentLine, (bracketNumber << 8) | (outerStyle << 1) | lineStateLineComment);
			lineStateLineComment = 0;
			visibleChars = 0;
		}
		sc.Forward();
	}

	sc.Complete();
}

constexpr int GetLineCommentState(int lineState) noexcept {
	return lineState & SimpleLineStateMaskLineComment;
}

void FoldCMakeDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList, Accessor &styler) {
	const Sci_PositionU endPos = startPos + lengthDoc;
	Sci_Line lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	int lineCommentPrev = 0;
	if (lineCurrent > 0) {
		levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
		lineCommentPrev = GetLineCommentState(styler.GetLineState(lineCurrent - 1));
	}

	int levelNext = levelCurrent;
	int lineCommentCurrent = GetLineCommentState(styler.GetLineState(lineCurrent));
	Sci_PositionU lineStartNext = styler.LineStart(lineCurrent + 1);
	Sci_PositionU lineEndPos = sci::min(lineStartNext, endPos) - 1;

	int styleNext = styler.StyleAt(startPos);
	int style = initStyle;

	char buf[16]; // function
	constexpr int MaxFoldWordLength = sizeof(buf) - 1;
	int wordLen = 0;

	for (Sci_PositionU i = startPos; i < endPos; i++) {
		const int stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);

		switch (style) {
		case SCE_CMAKE_BLOCK_COMMENT:
		case SCE_CMAKE_BRACKET_ARGUMENT:
			if (style != stylePrev) {
				levelNext++;
			} else if (style != styleNext) {
				levelNext--;
			}
			break;

		case SCE_CMAKE_OPERATOR: {
			const char ch = styler[i];
			if (ch == '(') {
				levelNext++;
			} else if (ch == ')') {
				levelNext--;
			}
		} break;

		case SCE_CMAKE_WORD:
			if (wordLen < MaxFoldWordLength) {
				buf[wordLen++] = MakeLowerCase(styler[i]);
			}
			if (styleNext != SCE_CMAKE_WORD) {
				buf[wordLen] = '\0';
				wordLen = 0;
				if (StrStartsWith(buf, "end")) {
					levelNext--;
				} else if (StrEqualsAny(buf, "if", "function", "macro", "foreach", "while")) {
					levelNext++;
				}
			}
			break;
		}

		if (i == lineEndPos) {
			const int lineCommentNext = GetLineCommentState(styler.GetLineState(lineCurrent + 1));
			if (lineCommentCurrent) {
				levelNext += lineCommentNext - lineCommentPrev;
			}

			const int levelUse = levelCurrent;
			int lev = levelUse | levelNext << 16;
			if (levelUse < levelNext) {
				lev |= SC_FOLDLEVELHEADERFLAG;
			}
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}

			lineCurrent++;
			lineStartNext = styler.LineStart(lineCurrent + 1);
			lineEndPos = sci::min(lineStartNext, endPos) - 1;
			levelCurrent = levelNext;
			lineCommentPrev = lineCommentCurrent;
			lineCommentCurrent = lineCommentNext;
		}
	}
}

}

LexerModule lmCMake(SCLEX_CMAKE, ColouriseCMakeDoc, "cmake", FoldCMakeDoc);
