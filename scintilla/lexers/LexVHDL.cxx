// Scintilla source code edit control
/** @file LexVHDL.cxx
 ** Lexer for VHDL
 ** Written by Phil Reid,
 ** Based on:
 **  - The Verilog Lexer by Avi Yegudin
 **  - The Fortran Lexer by Chuan-jian Shen
 **  - The C++ lexer by Neil Hodgson
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

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

/*static const char * const VHDLWordLists[] = {
	"Keywords",
	"Operators",
	"Attributes",
	"Standard Functions",
	"Standard Packages",
	"Standard Types",
	"User Words",
	"User2",
	"Fold",
	0,
};*/

static void ColouriseVHDLDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	const WordList &Keywords = *keywordLists[0];
	const WordList &Operators = *keywordLists[1];
	const WordList &Attributes = *keywordLists[2];
	const WordList &Functions = *keywordLists[3];
	const WordList &Packages = *keywordLists[4];
	const WordList &Types = *keywordLists[5];
	const WordList &User = *keywordLists[6];
	//const WordList &User2 = *keywordLists[7];

	StyleContext sc(startPos, length, initStyle, styler);

	for (; sc.More(); sc.Forward()) {

		// Determine if the current state should terminate.
		if (sc.state == SCE_VHDL_OPERATOR) {
			sc.SetState(SCE_VHDL_DEFAULT);
		} else if (sc.state == SCE_VHDL_NUMBER) {
			if (!iswordchar(sc.ch) && (sc.ch != '#')) {
				sc.SetState(SCE_VHDL_DEFAULT);
			}
		} else if (sc.state == SCE_VHDL_IDENTIFIER) {
			if (!iswordstart(sc.ch)) {
				char s[128];
				sc.GetCurrentLowered(s, sizeof(s));
				if (Keywords.InList(s)) {
					sc.ChangeState(SCE_VHDL_KEYWORD);
				} else if (Operators.InList(s)) {
					sc.ChangeState(SCE_VHDL_STDOPERATOR);
				} else if (Attributes.InList(s)) {
					sc.ChangeState(SCE_VHDL_ATTRIBUTE);
				} else if (Functions.InList(s)) {
					sc.ChangeState(SCE_VHDL_STDFUNCTION);
				} else if (Packages.InList(s)) {
					sc.ChangeState(SCE_VHDL_STDPACKAGE);
				} else if (Types.InList(s)) {
					sc.ChangeState(SCE_VHDL_STDTYPE);
				} else if (User.InList(s)) {
					sc.ChangeState(SCE_VHDL_USERWORD);
				}
				sc.SetState(SCE_VHDL_DEFAULT);
			}
		} else if (sc.state == SCE_VHDL_COMMENT || sc.state == SCE_VHDL_COMMENTLINEBANG) {
			if (sc.atLineStart) {
				sc.SetState(SCE_VHDL_DEFAULT);
			}
		} else if (sc.state == SCE_VHDL_STRING) {
			if (sc.ch == '\\') {
				if (sc.chNext == '\"' || sc.chNext == '\'' || sc.chNext == '\\') {
					sc.Forward();
				}
			} else if (sc.ch == '\"') {
				sc.ForwardSetState(SCE_VHDL_DEFAULT);
			} else if (sc.atLineEnd) {
				sc.ChangeState(SCE_VHDL_STRINGEOL);
				sc.ForwardSetState(SCE_VHDL_DEFAULT);
			}
		} else if (sc.state == SCE_VHDL_BLOCK_COMMENT) {
			if (sc.Match('*', '/')) {
				sc.Forward();
				sc.ForwardSetState(SCE_VHDL_DEFAULT);
			}
		}

		// Determine if a new state should be entered.
		if (sc.state == SCE_VHDL_DEFAULT) {
			if (IsNumberStart(sc.ch, sc.chNext)) {
				sc.SetState(SCE_VHDL_NUMBER);
			} else if (iswordstart(sc.ch)) {
				sc.SetState(SCE_VHDL_IDENTIFIER);
			} else if (sc.Match('-', '-')) {
				if (sc.GetRelative(2) == '!')	// Nice to have a different comment style
					sc.SetState(SCE_VHDL_COMMENTLINEBANG);
				else
					sc.SetState(SCE_VHDL_COMMENT);
			} else if (sc.Match('/', '*')) {
				sc.SetState(SCE_VHDL_BLOCK_COMMENT);
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_VHDL_STRING);
			} else if (isoperator(sc.ch)) {
				sc.SetState(SCE_VHDL_OPERATOR);
			}
		}
	}

	sc.Complete();
}

static constexpr bool IsCommentStyle(int style) noexcept {
	return style == SCE_VHDL_BLOCK_COMMENT || style == SCE_VHDL_COMMENT || style == SCE_VHDL_COMMENTLINEBANG;
}

static constexpr bool IsStreamCommentStyle(int style) noexcept {
	return style == SCE_VHDL_BLOCK_COMMENT;
}

#define IsCommentLine(line) IsLexCommentLine(styler, line, MultiStyle(SCE_VHDL_COMMENT, SCE_VHDL_COMMENTLINEBANG))

// Folding the code
static void FoldVHDLDoc(Sci_PositionU startPos, Sci_Position length, int /*initStyle*/, LexerWordList keywordLists, Accessor &styler) {
	const WordList &kwFold = *keywordLists[8];
	const bool foldAtBegin = styler.GetPropertyBool("fold.at.Begin", true);
	const Sci_PositionU endPos = startPos + length;

	Sci_Line lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0)
		levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
	int levelMinCurrentBegin = levelCurrent; //< Used for folding at 'begin'
	int levelNext = levelCurrent;

	/***************************************/
	Sci_PositionU lastStart = 0;
	char prevWord[32] = "";

	/***************************************/
	// Find prev word
	// The logic for going up or down a level depends on a the previous keyword
	// This code could be cleaned up.
	Sci_Position end = 0;
	Sci_PositionU j;
	for (j = startPos; j > 0; j--) {
		const char ch = styler.SafeGetCharAt(j);
		const char chPrev = styler.SafeGetCharAt(j - 1);
		const int style = styler.StyleAt(j);
		const int stylePrev = styler.StyleAt(j - 1);
		if ((!IsCommentStyle(stylePrev)) && (stylePrev != SCE_VHDL_STRING)) {
			if (iswordchar(chPrev) && !iswordchar(ch)) {
				end = j - 1;
			}
		}
		if ((!IsCommentStyle(style)) && (style != SCE_VHDL_STRING)) {
			if (!iswordchar(chPrev) && iswordstart(ch) && (end != 0)) {
				char s[32];
				unsigned int k;
				for (k = 0; (k < 31) && (k < end - j + 1); k++) {
					s[k] = MakeLowerCase(styler[j + k]);
				}
				s[k] = '\0';

				if (kwFold.InList(s)) {
					strcpy(prevWord, s);
					break;
				}
			}
		}
	}
	for (j = j + strlen(prevWord); j < endPos; j++) {
		const char ch = styler.SafeGetCharAt(j);
		const int style = styler.StyleAt(j);
		if ((!IsCommentStyle(style)) && (style != SCE_VHDL_STRING)) {
			if (ch == ';' && StrEqual(prevWord, "end")) {
				strcpy(prevWord, ";");
			}
		}
	}

	char ch = 0;
	char chNext;
	int style = SCE_VHDL_DEFAULT;
	int styleNext;
	chNext = styler[startPos];
	styleNext = styler.StyleAt(startPos);

	for (Sci_PositionU i = startPos; i < endPos; i++) {
		const char chPrev = ch;
		ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		char chNextNonBlank = chNext;
		j = i + 1;
		while (isspacechar(chNextNonBlank) && j < endPos) {
			j++;
			chNextNonBlank = styler.SafeGetCharAt(j);
		}
		const int stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		const bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');

		if (atEOL && IsCommentLine(lineCurrent)) {
			levelNext += IsCommentLine(lineCurrent + 1) - IsCommentLine(lineCurrent - 1);
		}
		if (IsStreamCommentStyle(style) && !IsCommentLine(lineCurrent)) {
			if (!IsStreamCommentStyle(stylePrev)) {
				levelNext++;
			} else if (!IsStreamCommentStyle(styleNext) && !atEOL) {
				levelNext--;
			}
		}

		if (style == SCE_VHDL_OPERATOR) {
			if (ch == '(') {
				levelNext++;
			} else if (ch == ')') {
				levelNext--;
			}
		}

		if ((!IsCommentStyle(style)) && (style != SCE_VHDL_STRING)) {
			if (ch == ';' && StrEqual(prevWord, "end")) {
				strcpy(prevWord, ";");
			}

			if (!iswordchar(chPrev) && iswordstart(ch)) {
				lastStart = i;
			}

			if (iswordchar(ch) && !iswordchar(chNext)) {
				char s[32];
				unsigned int k;
				for (k = 0; (k < 31) && (k < i - lastStart + 1); k++) {
					s[k] = MakeLowerCase(styler[lastStart + k]);
				}
				s[k] = '\0';

				if (kwFold.InList(s)) {
					if (StrEqualsAny(s, "architecture", "case", "generate", "loop", "block", "package", "process", "record", "then", "units")) {
						if (!StrEqual(prevWord, "end")) {
							levelNext++;
						}
					} else if (StrEqualsAny(s, "component", "entity", "configuration")) {
						if (!StrEqual(prevWord, "end")) {
							// check for instantiated unit by backward searching for the colon.
							Sci_PositionU pos = lastStart;
							char chAtPos = 0;
							int styleAtPos;
							do { // skip white spaces
								if (!pos)
									break;
								pos--;
								styleAtPos = styler.StyleAt(pos);
								chAtPos = styler.SafeGetCharAt(pos);
							} while (pos &&
								(chAtPos == ' ' || chAtPos == '\t' ||
									chAtPos == '\n' || chAtPos == '\r' ||
									IsCommentStyle(styleAtPos)));

							// check for a colon (':') before the instantiated units "entity", "component" or "configuration". Don't fold thereafter.
							if (chAtPos != ':') {
								levelNext++;
							}
						}
					} else if (StrEqualsAny(s, "procedure", "function")) {
						if (!StrEqual(prevWord, "end")) {
							// check for "end procedure" etc.
							// This code checks to see if the procedure / function is a definition within a "package"
								// rather than the actual code in the body.
							int BracketLevel = 0;
							for (Sci_Position pos = i + 1; pos < styler.Length(); pos++) {
								const int styleAtPos = styler.StyleAt(pos);
								const char chAtPos = styler.SafeGetCharAt(pos);
								if (chAtPos == '(') BracketLevel++;
								if (chAtPos == ')') BracketLevel--;
								if (
									(BracketLevel == 0) &&
									(!IsCommentLine(styleAtPos)) &&
									(styleAtPos != SCE_VHDL_STRING) &&
									!iswordchar(styler.SafeGetCharAt(pos - 1)) &&
									(chAtPos == 'i' || chAtPos == 'I') && styler.MatchAny(pos + 1, 's', 'S') &&
									!iswordchar(styler.SafeGetCharAt(pos + 2))) {
									levelNext++;
									break;
								}
								if ((BracketLevel == 0) && (chAtPos == ';')) {
									break;
								}
							}
						}

					} else if (StrEqual(s, "end")) {
						levelNext--;
					} else if (StrEqual(s, "elsif")) { // elsif is followed by then so folding occurs correctly
						levelNext--;
					} else if (StrEqual(s, "begin") && StrEqualsAny(prevWord, "architecture", "function", "procedure")) {
						levelMinCurrentBegin = levelNext - 1;
					}
					strcpy(prevWord, s);
				}
			}
		}

		if (atEOL || (i == endPos - 1)) {
			int levelUse = levelCurrent;
			if (foldAtBegin && (levelMinCurrentBegin < levelUse)) {
				levelUse = levelMinCurrentBegin;
			}
			int lev = levelUse | levelNext << 16;
			if (levelUse < levelNext)
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}

			lineCurrent++;
			levelCurrent = levelNext;
			levelMinCurrentBegin = levelCurrent;
		}
	}
}

LexerModule lmVHDL(SCLEX_VHDL, ColouriseVHDLDoc, "vhdl", FoldVHDLDoc);
