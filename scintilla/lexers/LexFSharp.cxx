// This file is part of Notepad4.
// See License.txt for details about distribution and modification.
//! Lexer for F#

#include <cassert>
#include <cstring>

#include <string>
#include <string_view>
#include <vector>

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

// https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/strings
struct EscapeSequence {
	int outerState = SCE_FSHARP_DEFAULT;
	int digitsLeft = 0;
	bool hex = false;

	// highlight any character as escape sequence.
	void resetEscapeState(int state, int chNext) noexcept {
		outerState = state;
		digitsLeft = 1;
		hex = true;
		if (chNext == 'x') {
			digitsLeft = 3;
		} else if (chNext == 'u') {
			digitsLeft = 5;
		} else if (chNext == 'U') {
			digitsLeft = 9;
		} else if (IsADigit(chNext)) {
			digitsLeft = 3;
			hex = false;
		}
	}
	void resetEscapeState(int state) noexcept {
		outerState = state;
		digitsLeft = 1;
	}
	bool atEscapeEnd(int ch) noexcept {
		--digitsLeft;
		return digitsLeft <= 0 || !IsDecimalOrHex(ch, hex);
	}
};

constexpr bool IsFSharpIdentifierChar(int ch) noexcept {
	return IsIdentifierCharEx(ch) || ch == '\'';
}

// https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/plaintext-formatting
constexpr bool IsPercentFormatSpecifier(char ch) noexcept {
	return AnyOf(ch, 'a', 'A',
					'b', 'B',
					'c',
					'd',
					'e', 'E',
					'f', 'F',
					'g', 'G',
					'i',
					'M',
					'o', 'O',
					'P',
					's',
					't',
					'u',
					'x', 'X');
}

inline Sci_Position CheckPercentFormatSpecifier(const StyleContext &sc, LexAccessor &styler, bool insideUrl) noexcept {
	if (sc.chNext == '%') {
		return 2;
	}
	if (insideUrl && IsHexDigit(sc.chNext)) {
		// percent encoded URL string
		return 0;
	}
	if (IsASpaceOrTab(sc.chNext) && IsADigit(sc.chPrev)) {
		// ignore word after percent: "5% x"
		return 0;
	}

	Sci_PositionU pos = sc.currentPos + 1;
	char ch = styler[pos];
	// flags
	while (AnyOf(ch, '-', '+', ' ', '0')) {
		ch = styler[++pos];
	}
	// [width]
	if (ch == '*') {
		ch = styler[++pos];
	} else {
		while (IsADigit(ch)) {
			ch = styler[++pos];
		}
	}
	// [.precision]
	if (ch == '.') {
		ch = styler[++pos];
		if (ch == '*') {
			ch = styler[++pos];
		} else {
			while (IsADigit(ch)) {
				ch = styler[++pos];
			}
		}
	}
	// [type]
	if (IsPercentFormatSpecifier(ch)) {
		++pos;
		return pos - sc.currentPos;
	}
	return 0;
}

// similar to C#
constexpr bool IsPlainString(int state) noexcept {
	return state < SCE_FSHARP_TRIPLE_STRING;
}

constexpr bool IsVerbatimString(int state) noexcept {
	return state == SCE_FSHARP_VERBATIM_STRING || state == SCE_FSHARP_INTERPOLATED_VERBATIM_STRING;
}

constexpr bool IsInterpolatedString(int state) noexcept {
	if constexpr (SCE_FSHARP_INTERPOLATED_STRING & 1) {
		return state & true;
	} else {
		return (state & 1) == 0;
	}
}

struct InterpolatedStringState {
	int state;
	int parenCount;
	int interpolatorCount;
};

// https://docs.microsoft.com/en-us/dotnet/standard/base-types/composite-formatting
constexpr bool IsInvalidFormatSpecifier(int ch) noexcept {
	// Custom format strings allows any characters
	return (ch >= '\0' && ch < ' ') || ch == '\"' || ch == '{' || ch == '}';
}

inline bool IsInterpolatedStringEnd(const StyleContext &sc) noexcept {
	return sc.ch == '}' || sc.ch == ':'
		|| (sc.ch == ',' && (IsADigit(sc.chNext) || (sc.chNext == '-' && IsADigit(sc.GetRelative(2)))));
}

//KeywordIndex++Autogenerated -- start of section automatically generated
enum {
	KeywordIndex_Keyword = 0,
	KeywordIndex_Type = 1,
	MaxKeywordSize = 16,
};
//KeywordIndex--Autogenerated -- end of section automatically generated

constexpr bool IsMultilineStyle(int style) noexcept {
	return (style >= SCE_FSHARP_STRING && style <= SCE_FSHARP_INTERPOLATED_TRIPLE_STRING)
		|| style == SCE_FSHARP_QUOTATION;
}

void ColouriseFSharpDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	int commentLevel = 0;
	int visibleChars = 0;
	int indentCount = 0;
	int lineState = 0;
	int stringInterpolatorCount = 0;
	bool insideUrl = false;
	bool insideAttribute = false;
	EscapeSequence escSeq;
	std::vector<InterpolatedStringState> nestedState;

	if (startPos != 0) {
		// backtrack to the line starts expression inside interpolated string literal.
		BacktrackToStart(styler, PyLineStateStringInterpolation, startPos, lengthDoc, initStyle);
	}

	StyleContext sc(startPos, lengthDoc, initStyle, styler);
	if (sc.currentLine > 0) {
		lineState = styler.GetLineState(sc.currentLine - 1);
		stringInterpolatorCount = (lineState >> 8) & 0xf;
		commentLevel = (lineState >> 12) & 0xf;
		lineState = 0;
	}

	if (startPos == 0 && sc.Match('#', '!')) {
		// F# shebang
		lineState = PyLineStateMaskCommentLine;
		sc.SetState(SCE_FSHARP_COMMENTLINE);
		sc.Forward();
	}

	while  (sc.More()) {
		switch (sc.state) {
		case SCE_FSHARP_OPERATOR:
		case SCE_FSHARP_OPERATOR2:
			sc.SetState(SCE_FSHARP_DEFAULT);
			break;

		case SCE_FSHARP_NUMBER:
			if (!IsDecimalNumber(sc.chPrev, sc.ch, sc.chNext)) {
				sc.SetState(SCE_FSHARP_DEFAULT);
			}
			break;

		case SCE_FSHARP_IDENTIFIER:
		case SCE_FSHARP_PREPROCESSOR:
			if (!IsFSharpIdentifierChar(sc.ch)) {
				if (sc.state == SCE_FSHARP_IDENTIFIER) {
					char s[MaxKeywordSize];
					sc.GetCurrent(s, sizeof(s));
					if (keywordLists[KeywordIndex_Keyword].InList(s)) {
						sc.ChangeState(SCE_FSHARP_KEYWORD);
						if ((visibleChars == 3 && StrEqual(s, "end")) || (visibleChars == 4 && StrEqual(s, "done"))) {
							lineState |= PyLineStateMaskCloseBrace;
						}
					} else if (keywordLists[KeywordIndex_Type].InList(s)) {
						sc.ChangeState(SCE_FSHARP_TYPE);
					} else if (insideAttribute) {
						const int chNext = sc.GetLineNextChar();
						if (chNext == ':' || chNext == '(' || chNext == '>') {
							sc.ChangeState(SCE_FSHARP_ATTRIBUTE);
						}
					}
				}
				sc.SetState(SCE_FSHARP_DEFAULT);
			}
			break;

		case SCE_FSHARP_COMMENT:
			if (sc.atLineStart) {
				lineState = PyLineStateMaskCommentLine;
			}
			if (sc.Match('(', '*')) {
				commentLevel++;
				sc.Forward();
			} else if (sc.Match('*', ')')) {
				sc.Forward();
				commentLevel--;
				if (commentLevel == 0) {
					sc.ForwardSetState(SCE_FSHARP_DEFAULT);
					if (lineState == PyLineStateMaskCommentLine && sc.GetLineNextChar() != '\0') {
						lineState = 0;
					}
				}
			}
			break;

		case SCE_FSHARP_COMMENTLINE:
		case SCE_FSHARP_COMMENTLINEDOC:
			if (sc.atLineStart) {
				sc.SetState(SCE_FSHARP_DEFAULT);
			}
			break;

		case SCE_FSHARP_BACKTICK:
			if (sc.Match('`', '`')) {
				sc.Forward();
				sc.ForwardSetState(SCE_FSHARP_DEFAULT);
			}
			break;

		case SCE_FSHARP_QUOTATION:
			if (sc.Match('@', '>')) {
				sc.Forward();
				sc.ForwardSetState(SCE_FSHARP_DEFAULT);
			}
			break;

		case SCE_FSHARP_CHARACTER:
		case SCE_FSHARP_STRING:
		case SCE_FSHARP_INTERPOLATED_STRING:
		case SCE_FSHARP_VERBATIM_STRING:
		case SCE_FSHARP_INTERPOLATED_VERBATIM_STRING:
		case SCE_FSHARP_TRIPLE_STRING:
		case SCE_FSHARP_INTERPOLATED_TRIPLE_STRING:
			if (sc.state == SCE_FSHARP_CHARACTER && sc.atLineStart) {
				sc.SetState(SCE_FSHARP_DEFAULT);
			} else if (sc.ch == '\\') {
				if (sc.state < SCE_FSHARP_VERBATIM_STRING && !IsEOLChar(sc.chNext)) {
					escSeq.resetEscapeState(sc.state, sc.chNext);
					sc.SetState(SCE_FSHARP_ESCAPECHAR);
					sc.Forward();
				}
			} else if (sc.ch == ((sc.state == SCE_FSHARP_CHARACTER) ? '\'' : '\"')) {
				if (sc.chNext == '\"' && IsVerbatimString(sc.state)) {
					escSeq.resetEscapeState(sc.state);
					sc.SetState(SCE_FSHARP_ESCAPECHAR);
					sc.Forward();
				} else if (IsPlainString(sc.state) || sc.MatchNext('\"', '\"')) {
					if (!IsPlainString(sc.state)) {
						sc.Advance(2);
					}
					if (sc.chNext == 'B') {
						sc.Forward();
					}
					stringInterpolatorCount = 0;
					sc.ForwardSetState(SCE_FSHARP_DEFAULT);
				}
			} else if (sc.state != SCE_FSHARP_CHARACTER) {
				if (sc.Match(':', '/', '/') && IsLowerCase(sc.chPrev)) {
					insideUrl = true;
				} else if (insideUrl && IsInvalidUrlChar(sc.ch)) {
					insideUrl = false;
				} else if (sc.ch == '%') {
					const int state = sc.state;
					if (state == SCE_FSHARP_INTERPOLATED_TRIPLE_STRING && stringInterpolatorCount > 1) {
						// https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/interpolated-strings
						const int interpolatorCount = GetMatchedDelimiterCount(styler, sc.currentPos, '%');
						if (interpolatorCount == stringInterpolatorCount) {
							insideUrl = false;
							sc.SetState(SCE_FSHARP_FORMAT_SPECIFIER);
							sc.Advance(interpolatorCount - 2);
							sc.Forward();
						} else {
							// content or syntax error
							sc.Advance(interpolatorCount);
							continue;
						}
					}
					const Sci_Position length = CheckPercentFormatSpecifier(sc, styler, insideUrl);
					if (length != 0 || sc.state == SCE_FSHARP_FORMAT_SPECIFIER) {
						sc.SetState(SCE_FSHARP_FORMAT_SPECIFIER);
						sc.Advance(length);
						sc.SetState(state);
						continue;
					}
				}
				if (IsInterpolatedString(sc.state)) {
					if (sc.ch == '{') {
						if (sc.chNext == '{' && IsPlainString(sc.state)) {
							escSeq.resetEscapeState(sc.state);
							sc.SetState(SCE_FSHARP_ESCAPECHAR);
							sc.Forward();
						} else {
							const int interpolatorCount = GetMatchedDelimiterCount(styler, sc.currentPos, '{');
							if (IsPlainString(sc.state) || interpolatorCount >= stringInterpolatorCount) {
								nestedState.push_back({sc.state, 0, stringInterpolatorCount});
								sc.Advance(interpolatorCount - stringInterpolatorCount); // outer content
								sc.SetState(SCE_FSHARP_OPERATOR2);
								sc.Advance(stringInterpolatorCount - 1); // inner interpolation
								sc.ForwardSetState(SCE_FSHARP_DEFAULT);
								stringInterpolatorCount = 0;
							}
						}
					} else if (sc.ch == '}') {
						const int interpolatorCount = IsPlainString(sc.state) ? 1 : GetMatchedDelimiterCount(styler, sc.currentPos, '}');
						const bool interpolating = !nestedState.empty() && (interpolatorCount >= stringInterpolatorCount);
						if (interpolating) {
							nestedState.pop_back();
						}
						if (interpolating || (sc.chNext != '}' && IsPlainString(sc.state))) {
							const int state = sc.state;
							sc.SetState(SCE_FSHARP_OPERATOR2);
							sc.Advance(stringInterpolatorCount - 1); // inner interpolation
							sc.ForwardSetState(state);
							sc.Advance(interpolatorCount - stringInterpolatorCount); // outer content
							continue;
						}
						if (sc.chNext == '}' && IsPlainString(sc.state)) {
							escSeq.resetEscapeState(sc.state);
							sc.SetState(SCE_FSHARP_ESCAPECHAR);
							sc.Forward();
						}
					}
				}
			}
			break;

		case SCE_FSHARP_ESCAPECHAR:
			if (escSeq.atEscapeEnd(sc.ch)) {
				sc.SetState(escSeq.outerState);
				continue;
			}
			break;

		case SCE_FSHARP_FORMAT_SPECIFIER:
			if (IsInvalidFormatSpecifier(sc.ch)) {
				sc.SetState(escSeq.outerState);
				continue;
			}
			break;
		}

		if (sc.state == SCE_FSHARP_DEFAULT) {
			if (sc.Match('(', '*')) {
				sc.SetState(SCE_FSHARP_COMMENT);
				sc.Forward();
				if (sc.chNext == ')') {
					// let (*) x y = x * y
					// TODO?: let (**) x y = x ** y
					sc.ChangeState(SCE_FSHARP_OPERATOR);
				} else {
					commentLevel = 1;
					if (visibleChars == 0) {
						lineState = PyLineStateMaskCommentLine;
					}
				}
			} else if (sc.Match('/', '/')) {
				if (visibleChars == 0) {
					lineState = PyLineStateMaskCommentLine;
				}
				sc.SetState(SCE_FSHARP_COMMENTLINE);
				sc.Forward();
				if (sc.chNext == '/') {
					sc.ChangeState(SCE_FSHARP_COMMENTLINEDOC);
				}
			} else if (sc.ch == '\"') {
				insideUrl = false;
				sc.SetState(SCE_FSHARP_STRING);
				if (sc.MatchNext('\"', '\"')) {
					sc.ChangeState(SCE_FSHARP_TRIPLE_STRING);
					sc.Advance(2);
				}
			} else if (sc.ch == '$' || sc.ch == '@') {
				insideUrl = false;
				sc.SetState(SCE_FSHARP_OPERATOR);
				if (sc.ch != sc.chNext && (sc.chNext == '$' || sc.chNext == '@')) {
					sc.Forward();
					if (sc.chNext == '\"') {
						stringInterpolatorCount = 1;
						sc.ChangeState(SCE_FSHARP_INTERPOLATED_VERBATIM_STRING);
						sc.Forward();
					}
				} else if (sc.chNext == '\"') {
					stringInterpolatorCount = sc.ch == '$';
					sc.ChangeState((sc.ch == '$') ? SCE_FSHARP_INTERPOLATED_STRING : SCE_FSHARP_VERBATIM_STRING);
					sc.Forward();
					if (stringInterpolatorCount != 0 && sc.MatchNext('\"', '\"')) {
						sc.ChangeState(SCE_FSHARP_INTERPOLATED_TRIPLE_STRING);
						sc.Advance(2);
					}
				} else if (sc.chNext == '$') {
					const int interpolatorCount = GetMatchedDelimiterCount(styler, sc.currentPos + 1, '$') + 1;
					sc.Advance(interpolatorCount);
					if (sc.Match('\"', '\"', '\"')) {
						stringInterpolatorCount = interpolatorCount;
						sc.ChangeState(SCE_FSHARP_INTERPOLATED_TRIPLE_STRING);
						sc.Advance(2);
					}
				}
			} else if (sc.ch == '\'') {
				int state = SCE_FSHARP_CHARACTER;
				if (IsEOLChar(sc.chNext)) {
					state = SCE_FSHARP_OPERATOR;
				} else if (sc.chNext != '\\') {
					const int after = sc.GetCharAfterNext();
					if (after != '\'') {
						state = IsIdentifierStartEx(sc.chNext) ? SCE_FSHARP_IDENTIFIER : SCE_FSHARP_OPERATOR;
					}
				}
				sc.SetState(state);
			} else if (sc.Match('`', '`')) {
				sc.SetState(SCE_FSHARP_BACKTICK);
				sc.Forward();
			} else if (sc.Match('<', '@')) {
				sc.SetState(SCE_FSHARP_QUOTATION);
				sc.Forward();
			} else if (IsNumberStartEx(sc.chPrev, sc.ch, sc.chNext)) {
				sc.SetState(SCE_FSHARP_NUMBER);
			} else if (sc.ch == '#' && visibleChars == 0) {
				sc.SetState(SCE_FSHARP_PREPROCESSOR);
			} else if (IsIdentifierStartEx(sc.ch)) {
				sc.SetState(SCE_FSHARP_IDENTIFIER);
			} else if (IsAGraphic(sc.ch)) {
				sc.SetState(SCE_FSHARP_OPERATOR);
				if (visibleChars == 0 && (sc.ch == '}' || sc.ch == ']' || sc.ch == ')')) {
					lineState |= PyLineStateMaskCloseBrace;
				} else if (sc.Match('[', '<')) {
					insideAttribute = true;
				} else if (sc.Match('>', ']')) {
					insideAttribute = false;
				}
				if (!nestedState.empty()) {
					sc.ChangeState(SCE_FSHARP_OPERATOR2);
					InterpolatedStringState &state = nestedState.back();
					if (sc.ch == '[' || sc.ch == '(') {
						++state.parenCount;
					} else if (sc.ch == ']' || sc.ch == ')') {
						--state.parenCount;
					}
					if (state.parenCount <= 0 && IsInterpolatedStringEnd(sc)) {
						escSeq.outerState = state.state;
						stringInterpolatorCount = state.interpolatorCount;
						sc.ChangeState((sc.ch == '}') ? state.state : SCE_FSHARP_FORMAT_SPECIFIER);
						continue;
					}
				}
			}
		}

		if (visibleChars == 0) {
			if (sc.ch == ' ') {
				++indentCount;
			} else if (sc.ch == '\t') {
				indentCount = GetTabIndentCount(indentCount);
			}
		}
		if (!isspacechar(sc.ch)) {
			visibleChars++;
		}
		if (sc.atLineEnd) {
			if (!nestedState.empty()) {
				lineState = PyLineStateStringInterpolation | PyLineStateMaskTripleQuote;
			} else if (IsMultilineStyle(sc.state)) {
				lineState = PyLineStateMaskTripleQuote;
			} else if (lineState == 0 && visibleChars == 0) {
				lineState = PyLineStateMaskEmptyLine;
			}
			lineState |= (indentCount << 16) | (commentLevel << 12) | (stringInterpolatorCount << 8);
			styler.SetLineState(sc.currentLine, lineState);
			lineState = 0;
			visibleChars = 0;
			indentCount = 0;
			insideUrl = false;
			insideAttribute = false;
		}
		sc.Forward();
	}

	sc.Complete();
}

}

extern const LexerModule lmFSharp(SCLEX_FSHARP, ColouriseFSharpDoc, "fsharp", FoldPyDoc);
