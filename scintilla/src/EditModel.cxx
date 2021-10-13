// Scintilla source code edit control
/** @file EditModel.cxx
 ** Defines the editor state that must be visible to EditorView.
 **/
// Copyright 1998-2014 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cmath>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <algorithm>
#include <memory>

#include "ScintillaTypes.h"
#include "ILoader.h"
#include "ILexer.h"

#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"

//#include "CharacterCategory.h"

#include "Position.h"
#include "UniqueString.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "Indicator.h"
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "Document.h"
#include "UniConversion.h"
#include "Selection.h"
#include "PositionCache.h"
#include "EditModel.h"

using namespace Scintilla;
using namespace Scintilla::Internal;

Caret::Caret() noexcept :
	active(false), on(false), period(500) {}

EditModel::EditModel() : braces{} {
	inOverstrike = false;
	trackLineWidth = false;
	xOffset = 0;
	posDrag = SelectionPosition(Sci::invalidPosition);
	braces[0] = Sci::invalidPosition;
	braces[1] = Sci::invalidPosition;
	bracesMatchStyle = StyleBraceBad;
	highlightGuideColumn = 0;
	hasFocus = false;
	primarySelection = true;
	imeInteraction = IMEInteraction::Windowed;
	bidirectional = Bidirectional::Disabled;
	foldFlags = FoldFlag::None;
	foldDisplayTextStyle = FoldDisplayTextStyle::Hidden;
	hotspot = Range(Sci::invalidPosition);
	hotspotSingleLine = true;
	hoverIndicatorPos = Sci::invalidPosition;
	wrapWidth = LineLayout::wrapWidthInfinite;
	// before setting a lexer, style buffer is useless.
	pdoc = new Document(DocumentOption::StylesNone);
	pdoc->AddRef();
	pcs = ContractionStateCreate(pdoc->IsLarge());
}

EditModel::~EditModel() {
	pdoc->Release();
	pdoc = nullptr;
}

bool EditModel::BidirectionalEnabled() const noexcept {
	return (bidirectional != Bidirectional::Disabled) &&
		(CpUtf8 == pdoc->dbcsCodePage);
}

bool EditModel::BidirectionalR2L() const noexcept {
	return bidirectional == Bidirectional::R2L;
}

void EditModel::SetDefaultFoldDisplayText(const char *text) {
	defaultFoldDisplayText = UniqueStringCopy(text);
}

const char *EditModel::GetDefaultFoldDisplayText() const noexcept {
	return defaultFoldDisplayText.get();
}

const char *EditModel::GetFoldDisplayText(Sci::Line lineDoc) const noexcept {
	if (foldDisplayTextStyle == FoldDisplayTextStyle::Hidden || pcs->GetExpanded(lineDoc)) {
		return nullptr;
	}

#if EnablePerLineFoldDisplayText
	const char *text = pcs->GetFoldDisplayText(lineDoc);
	return text ? text : defaultFoldDisplayText.get();
#else
	return defaultFoldDisplayText.get();
#endif
}

InSelection EditModel::LineEndInSelection(Sci::Line lineDoc) const noexcept {
	const Sci::Position posAfterLineEnd = pdoc->LineStart(lineDoc + 1);
	return sel.InSelectionForEOL(posAfterLineEnd);
}
