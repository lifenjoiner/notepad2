// This file is part of Notepad2.
// See License.txt for details about distribution and modification.
//! Lexer for PowerShell.

#include <cassert>

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
#include "LexerModule.h"

using namespace Lexilla;

namespace {

// Extended to accept accented characters
constexpr bool IsPSWordChar(int ch) noexcept {
	return ch >= 0x80 || IsAlphaNumeric(ch) || ch == '-' || ch == '_';
}

/*const char * const powershellWordLists[] = {
	"Commands",
	"Cmdlets",
	"Aliases",
	"Functions",
	"User1",
	0
};*/

void ColourisePowerShellDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	const WordList &keywords = *keywordLists[0];
	const WordList &keywords2 = *keywordLists[1];
	const WordList &keywords3 = *keywordLists[2];
	const WordList &keywords4 = *keywordLists[3];
	const WordList &keywords5 = *keywordLists[4];

	StyleContext sc(startPos, length, initStyle, styler);

	for (; sc.More(); sc.Forward()) {
		switch (sc.state) {
		case SCE_POWERSHELL_COMMENT:
			if (sc.atLineStart) {
				sc.SetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_COMMENTSTREAM:
			if (sc.ch == '>' && sc.chPrev == '#') {
				sc.ForwardSetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_STRING_DQ:
			// This is a doubles quotes string
			if (sc.ch == '\"') {
				sc.ForwardSetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_STRING_SQ:
			// This is a single quote string
			if (sc.ch == '\'') {
				sc.ForwardSetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_NUMBER:
			if (!IsADigit(sc.ch)) {
				sc.SetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_VARIABLE:
			if (!IsPSWordChar(sc.ch)) {
				sc.SetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_OPERATOR:
			if (!isoperator(sc.ch)) {
				sc.SetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		case SCE_POWERSHELL_IDENTIFIER:
			if (!IsPSWordChar(sc.ch)) {
				char s[128];
				sc.GetCurrentLowered(s, sizeof(s));

				if (keywords.InList(s)) {
					sc.ChangeState(SCE_POWERSHELL_KEYWORD);
				} else if (keywords2.InList(s)) {
					sc.ChangeState(SCE_POWERSHELL_CMDLET);
				} else if (keywords3.InList(s)) {
					sc.ChangeState(SCE_POWERSHELL_ALIAS);
				} else if (keywords4.InList(s)) {
					sc.ChangeState(SCE_POWERSHELL_FUNCTION);
				} else if (keywords5.InList(s)) {
					sc.ChangeState(SCE_POWERSHELL_USER1);
				}
				sc.SetState(SCE_POWERSHELL_DEFAULT);
			}
			break;
		}

		// Determine if a new state should be entered.
		if (sc.state == SCE_POWERSHELL_DEFAULT) {
			if (sc.ch == '#') {
				sc.SetState(SCE_POWERSHELL_COMMENT);
			} else if (sc.ch == '<' && sc.chNext == '#') {
				sc.SetState(SCE_POWERSHELL_COMMENTSTREAM);
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_POWERSHELL_STRING_DQ);
			} else if (sc.ch == '\'') {
				sc.SetState(SCE_POWERSHELL_STRING_SQ);
			} else if (sc.ch == '$') {
				sc.SetState(SCE_POWERSHELL_VARIABLE);
			} else if (IsNumberStart(sc.ch, sc.chNext)) {
				sc.SetState(SCE_POWERSHELL_NUMBER);
			} else if (isoperator(sc.ch)) {
				sc.SetState(SCE_POWERSHELL_OPERATOR);
			} else if (IsPSWordChar(sc.ch)) {
				sc.SetState(SCE_POWERSHELL_IDENTIFIER);
			}
		}
	}

	sc.Complete();
}

// Store both the current line's fold level and the next lines in the
// level store to make it easy to pick up with each increment
// and to make it possible to fiddle the current level for "} else {".
void FoldPowerShellDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList, Accessor &styler) {
	const Sci_PositionU endPos = startPos + length;
	Sci_Line lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0)
		levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
	int levelNext = levelCurrent;
	char chNext = styler[startPos];
	int styleNext = styler.StyleAt(startPos);
	int style = initStyle;
	for (Sci_PositionU i = startPos; i < endPos; i++) {
		const char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		const int stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		const bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (style == SCE_POWERSHELL_OPERATOR) {
			if (ch == '{') {
				levelNext++;
			} else if (ch == '}') {
				levelNext--;
			}
		} else if (style == SCE_POWERSHELL_COMMENTSTREAM) {
			if (stylePrev != SCE_POWERSHELL_COMMENTSTREAM) {
				levelNext++;
			} else if (styleNext != SCE_POWERSHELL_COMMENTSTREAM) {
				levelNext--;
			}
		}
		if (atEOL || (i == endPos - 1)) {
			const int levelUse = levelCurrent;
			int lev = levelUse | levelNext << 16;
			if (levelUse < levelNext)
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelCurrent = levelNext;
		}
	}
}

}

LexerModule lmPowerShell(SCLEX_POWERSHELL, ColourisePowerShellDoc, "powershell", FoldPowerShellDoc);
