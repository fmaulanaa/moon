/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * layout.cpp: 
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2008 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include "moon-path.h"
#include "layout.h"
#include "debug.h"



#define BBOX_MARGIN 1.0
#define BBOX_PADDING 2.0

/*
 * Silverlight does not apply any kerning on a DOT, so we exclude them
 * 	U+002E FULL STOP
 * 	U+06D4 ARABIC FULL STOP
 *	U+3002 IDEOGRAPHIC FULL STOP
 * Note: this is different than using the "sliding dot" algorithm from
 * http://www.freetype.org/freetype2/docs/glyphs/glyphs-4.html
 */
#define APPLY_KERNING(uc)	((uc != 0x002E) && (uc != 0x06D4) && (uc != 3002))


static inline gunichar
utf8_getc (const char **in, size_t inlen)
{
	register const unsigned char *inptr = (const unsigned char *) *in;
	const unsigned char *inend = inptr + inlen;
	register unsigned char c, r;
	register gunichar m, u = 0;
	
	if (inlen == 0)
		return 0;
	
	r = *inptr++;
	if (r < 0x80) {
		*in = (const char *) inptr;
		u = r;
	} else if (r < 0xfe) { /* valid start char? */
		u = r;
		m = 0x7f80;    /* used to mask out the length bits */
		do {
			if (inptr >= inend)
				return 0;
			
			c = *inptr++;
			if ((c & 0xc0) != 0x80)
				goto error;
			
			u = (u << 6) | (c & 0x3f);
			r <<= 1;
			m <<= 5;
		} while (r & 0x40);
		
		*in = (const char *) inptr;
		
		u &= ~m;
	} else {
	 error:
		u = (gunichar) -1;
		*in = (*in) + 1;
	}
	
	return u;
}

static inline int
utf8_strlen (const char *str, int length)
{
	const char *inend = str + length;
	const char *inptr = str;
	int count = 0;
	
	while (inptr < inend) {
		// invalid chars do not count toward our total
		if (utf8_getc (&inptr, inend - inptr) != (gunichar) -1)
			count++;
	}
	
	return count;
}

static inline const char *
utf8_find_last_word (const char *str, int len)
{
	const char *last_word = str;
	const char *inend = str + len;
	const char *inptr = str;
	bool in_word = false;
	const char *pchar;
	gunichar c;
	
	while (inptr < inend) {
		pchar = inptr;
		if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
			continue;
		
		if (!g_unichar_isspace (c)) {
			if (!in_word) {
				last_word = pchar;
				in_word = true;
			}
		} else {
			in_word = false;
		}
	}
	
	return last_word;
}


//
// TextLayoutGlyphCluster
//

TextLayoutGlyphCluster::TextLayoutGlyphCluster (int _start, int _length)
{
	length = _length;
	start = _start;
	selected = false;
	advance = 0.0;
	path = NULL;
}

TextLayoutGlyphCluster::~TextLayoutGlyphCluster ()
{
	if (path)
		moon_path_destroy (path);
}


//
// TextLayoutRun
//

TextLayoutRun::TextLayoutRun (TextLayoutLine *_line, TextLayoutAttributes *_attrs, int _start)
{
	clusters = g_ptr_array_new ();
	attrs = _attrs;
	start = _start;
	line = _line;
	advance = 0.0;
	length = 0;
	count = 0;
}

TextLayoutRun::~TextLayoutRun ()
{
	for (guint i = 0; i < clusters->len; i++)
		delete (TextLayoutGlyphCluster *) clusters->pdata[i];
	
	g_ptr_array_free (clusters, true);
}

void
TextLayoutRun::ClearCache ()
{
	for (guint i = 0; i < clusters->len; i++)
		delete (TextLayoutGlyphCluster *) clusters->pdata[i];
	
	g_ptr_array_set_size (clusters, 0);
}


//
// TextLayoutLine
//

TextLayoutLine::TextLayoutLine (TextLayout *_layout, int _start, int _offset)
{
	runs = g_ptr_array_new ();
	layout = _layout;
	offset = _offset;
	start = _start;
	advance = 0.0;
	descend = 0.0;
	height = 0.0;
	width = 0.0;
	length = 0;
}

TextLayoutLine::~TextLayoutLine ()
{
	for (guint i = 0; i < runs->len; i++)
		delete (TextLayoutRun *) runs->pdata[i];
	
	g_ptr_array_free (runs, true);
}



//
// TextLayout
//

TextLayout::TextLayout ()
{
	// Note: TextBlock and TextBox assume their default values match these
	strategy = LineStackingStrategyMaxHeight;
	alignment = TextAlignmentLeft;
	wrapping = TextWrappingNoWrap;
	selection_length = 0;
	selection_start = 0;
	last_word = NULL;
	actual_height = -1.0;
	actual_width = -1.0;
	line_height = NAN;
	max_height = -1.0;
	max_width = -1.0;
	attributes = NULL;
	lines = g_ptr_array_new ();
	text = NULL;
	length = 0;
	count = 0;
}

TextLayout::~TextLayout ()
{
	if (attributes) {
		attributes->Clear (true);
		delete attributes;
	}
	
	ClearLines ();
	g_ptr_array_free (lines, true);
	
	g_free (text);
}

void
TextLayout::ClearLines ()
{
	for (guint i = 0; i < lines->len; i++)
		delete (TextLayoutLine *) lines->pdata[i];
	
	g_ptr_array_set_size (lines, 0);
}

bool
TextLayout::SetLineStackingStrategy (LineStackingStrategy mode)
{
	if (strategy == mode)
		return false;
	
	strategy = mode;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

bool
TextLayout::SetTextAlignment (TextAlignment align)
{
	if (alignment == align)
		return false;
	
	alignment = align;
	
	return false;
}

bool
TextLayout::SetTextWrapping (TextWrapping mode)
{
	if (wrapping == mode)
		return false;
	
	wrapping = mode;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

bool
TextLayout::SetLineHeight (double height)
{
	if (line_height == height)
		return false;
	
	line_height = height;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

bool
TextLayout::SetMaxHeight (double height)
{
	if (max_height == height)
		return false;
	
	max_height = height;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

bool
TextLayout::SetMaxWidth (double width)
{
	if (max_width == width)
		return false;
	
	max_width = width;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

bool
TextLayout::SetTextAttributes (List *attrs)
{
	if (attributes) {
		attributes->Clear (true);
		delete attributes;
	}
	
	attributes = attrs;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

bool
TextLayout::SetText (const char *str, int len)
{
	g_free (text);
	
	length = len == -1 ? strlen (str) : len;
	text = (char *) g_malloc (length + 1);
	memcpy (text, str, length);
	text[length] = '\0';
	last_word = NULL;
	count = -1;
	
	actual_height = -1.0;
	actual_width = -1.0;
	
	return true;
}

void
TextLayout::ClearCache ()
{
	TextLayoutLine *line;
	TextLayoutRun *run;
	
	for (guint i = 0; i < lines->len; i++) {
		line = (TextLayoutLine *) lines->pdata[i];
		
		for (guint j = 0; j < line->runs->len; j++) {
			run = (TextLayoutRun *) line->runs->pdata[j];
			run->ClearCache ();
		}
	}
}

struct TextRegion {
	int start, length;
	bool select;
};

static void
UpdateSelection (GPtrArray *lines, TextRegion *pre, TextRegion *post)
{
	TextLayoutGlyphCluster *cluster;
	TextLayoutLine *line;
	TextLayoutRun *run;
	guint i, j;
	
	// first update pre-region
	for (i = 0; i < lines->len; i++) {
		line = (TextLayoutLine *) lines->pdata[i];
		
		if (pre->start >= line->start + line->length) {
			// pre-region not on this line...
			continue;
		}
		
		for (j = 0; j < line->runs->len; j++) {
			run = (TextLayoutRun *) line->runs->pdata[j];
			
			if (pre->start >= run->start + run->length) {
				// pre-region not in this run...
				continue;
			}
			
			if (pre->start <= run->start) {
				if (pre->start + pre->length >= run->start + run->length) {
					// run is fully contained within the pre-region
					if (run->clusters->len == 1) {
						cluster = (TextLayoutGlyphCluster *) run->clusters->pdata[0];
						cluster->selected = pre->select;
					} else {
						run->ClearCache ();
					}
				} else {
					run->ClearCache ();
				}
			} else {
				run->ClearCache ();
			}
			
			if (pre->start + pre->length <= run->start + run->length)
				break;
		}
	}
	
	// now update the post region...
	for ( ; i < lines->len; i++, j = 0) {
		line = (TextLayoutLine *) lines->pdata[i];
		
		if (post->start >= line->start + line->length) {
			// pre-region not on this line...
			continue;
		}
		
		for ( ; j < line->runs->len; j++) {
			run = (TextLayoutRun *) line->runs->pdata[j];
			
			if (post->start >= run->start + run->length) {
				// post-region not in this run...
				continue;
			}
			
			if (post->start <= run->start) {
				if (post->start + post->length >= run->start + run->length) {
					// run is fully contained within the pre-region
					if (run->clusters->len == 1) {
						cluster = (TextLayoutGlyphCluster *) run->clusters->pdata[0];
						cluster->selected = post->select;
					} else {
						run->ClearCache ();
					}
				} else {
					run->ClearCache ();
				}
			} else {
				run->ClearCache ();
			}
			
			if (post->start + post->length <= run->start + run->length)
				break;
		}
	}
}

void
TextLayout::Select (int start, int length, bool byte_offsets)
{
	int new_selection_length;
	int new_selection_start;
	int new_selection_end;
	int selection_end;
	TextRegion pre, post;
	const char *inptr;
	const char *inend;
	
	if (!text) {
		selection_length = 0;
		selection_start = 0;
		return;
	}
	
	if (!byte_offsets) {
		inptr = g_utf8_offset_to_pointer (text, start);
		new_selection_start = inptr - text;
		
		inend = g_utf8_offset_to_pointer (inptr, length);
		new_selection_length = inend - inptr;
	} else {
		new_selection_length = length;
		new_selection_start = start;
	}
	
	if (selection_start == new_selection_start &&
	    selection_length == new_selection_length) {
		// no change in selection...
		return;
	}
	
#if true
	// compute the region between the 2 starts
	pre.length = abs (new_selection_start - selection_start);
	pre.start = MIN (selection_start, new_selection_start);
	pre.select = new_selection_start < selection_start;
	
	// compute the region between the 2 ends
	new_selection_end = new_selection_start + new_selection_length;
	selection_end = selection_start + selection_length;
	post.length = abs (new_selection_end - selection_end);
	post.start = MIN (selection_end, new_selection_end);
	post.select = new_selection_end > selection_end;
	
	UpdateSelection (lines, &pre, &post);
	
	selection_length = new_selection_length;
	selection_start = new_selection_start;
#else
	if (selection_length || new_selection_length)
		ClearCache ();
	
	selection_length = new_selection_length;
	selection_start = new_selection_start;
#endif
}

/**
 * TextLayout::GetActualExtents:
 * @width:
 * @height:
 *
 * Gets the actual width and height extents required for rendering the
 * full text.
 **/
void
TextLayout::GetActualExtents (double *width, double *height)
{
	*height = actual_height;
	*width = actual_width;
}

#if 0
static const char *unicode_break_types[] = {
	"G_UNICODE_BREAK_MANDATORY",
	"G_UNICODE_BREAK_CARRIAGE_RETURN",
	"G_UNICODE_BREAK_LINE_FEED",
	"G_UNICODE_BREAK_COMBINING_MARK",
	"G_UNICODE_BREAK_SURROGATE",
	"G_UNICODE_BREAK_ZERO_WIDTH_SPACE",
	"G_UNICODE_BREAK_INSEPARABLE",
	"G_UNICODE_BREAK_NON_BREAKING_GLUE",
	"G_UNICODE_BREAK_CONTINGENT",
	"G_UNICODE_BREAK_SPACE",
	"G_UNICODE_BREAK_AFTER",
	"G_UNICODE_BREAK_BEFORE",
	"G_UNICODE_BREAK_BEFORE_AND_AFTER",
	"G_UNICODE_BREAK_HYPHEN",
	"G_UNICODE_BREAK_NON_STARTER",
	"G_UNICODE_BREAK_OPEN_PUNCTUATION",
	"G_UNICODE_BREAK_CLOSE_PUNCTUATION",
	"G_UNICODE_BREAK_QUOTATION",
	"G_UNICODE_BREAK_EXCLAMATION",
	"G_UNICODE_BREAK_IDEOGRAPHIC",
	"G_UNICODE_BREAK_NUMERIC",
	"G_UNICODE_BREAK_INFIX_SEPARATOR",
	"G_UNICODE_BREAK_SYMBOL",
	"G_UNICODE_BREAK_ALPHABETIC",
	"G_UNICODE_BREAK_PREFIX",
	"G_UNICODE_BREAK_POSTFIX",
	"G_UNICODE_BREAK_COMPLEX_CONTEXT",
	"G_UNICODE_BREAK_AMBIGUOUS",
	"G_UNICODE_BREAK_UNKNOWN",
	"G_UNICODE_BREAK_NEXT_LINE",
	"G_UNICODE_BREAK_WORD_JOINER",
	"G_UNICODE_BREAK_HANGUL_L_JAMO",
	"G_UNICODE_BREAK_HANGUL_V_JAMO",
	"G_UNICODE_BREAK_HANGUL_T_JAMO",
	"G_UNICODE_BREAK_HANGUL_LV_SYLLABLE",
	"G_UNICODE_BREAK_HANGUL_LVT_SYLLABLE"
};
#endif


#define BreakSpace(btype) (btype == G_UNICODE_BREAK_SPACE || btype == G_UNICODE_BREAK_ZERO_WIDTH_SPACE)
#define BreakAfter(btype) (btype == G_UNICODE_BREAK_AFTER || btype == G_UNICODE_BREAK_NEXT_LINE)
#define BreakBefore(btype) (btype == G_UNICODE_BREAK_BEFORE || btype == G_UNICODE_BREAK_PREFIX)

struct WordBreakOpportunity {
	GUnicodeBreakType btype;
	const char *inptr;
	double advance;
	gunichar c;
	int count;
};

struct LayoutWord {
	// <internal use>
	GArray *break_ops;     // TextWrappingWrap only
	
	// <input>
	const char *last_word; // TextWrappingWrap only
	double line_advance;
	TextFont *font;
	
	// <input/output>
	guint32 prev;
	
	// <output>
	double advance;
	int length;
	int count;
};

static inline void
layout_word_init (LayoutWord *word, double line_advance, guint32 prev)
{
	word->line_advance = line_advance;
	word->prev = prev;
}

/**
 * layout_word_lwsp:
 * @word: #LayoutWord context
 * @in: input text
 * @inend: end of input text
 *
 * Measures a word containing nothing but LWSP.
 **/
static void
layout_word_lwsp (LayoutWord *word, const char *in, const char *inend)
{
	guint32 prev = word->prev;
	GUnicodeBreakType btype;
	const char *inptr = in;
	const char *start;
	GlyphInfo *glyph;
	double advance;
	gunichar c;
	
	word->advance = 0.0;
	word->count = 0;
	
	while (inptr < inend) {
		// check for line-breaks
		if (*inptr == '\r' || *inptr == '\n')
			break;
		
		// ignore invalid chars
		start = inptr;
		if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
			continue;
		
		btype = g_unichar_break_type (c);
		if (!BreakSpace (btype)) {
			inptr = start;
			break;
		}
		
		word->count++;
		
		// ignore zero-width glyphs for layout metrics
		if (g_unichar_iszerowidth (c))
			continue;
		
		// canonicalize whitespace
		if (g_unichar_isspace (c))
			c = ' ';
		
		// ignore glyphs the font doesn't contain...
		if (!(glyph = word->font->GetGlyphInfo (c)))
			continue;
		
		// calculate total glyph advance
		if ((advance = glyph->metrics.horiAdvance) <= 0.0)
			continue;
		
		if ((prev != 0) && APPLY_KERNING (c))
			advance += word->font->Kerning (prev, glyph->index);
		else if (glyph->metrics.horiBearingX < 0)
			advance -= glyph->metrics.horiBearingX;
		
		word->line_advance += advance;
		word->advance += advance;
		prev = glyph->index;
	}
	
	word->length = (inptr - in);
	word->prev = prev;
}

/**
 * layout_word_overflow:
 * @word: #LayoutWord context
 * @in: input text
 * @inend = end of input text
 * @max_width: max allowable width for a line
 *
 * Calculates the advance of the current word.
 *
 * Returns: %true if the caller should create a new line for this word
 * or %false otherwise.
 **/
static bool
layout_word_overflow (LayoutWord *word, const char *in, const char *inend, double max_width)
{
	bool line_start = word->line_advance == 0.0;
	guint32 prev = word->prev;
	GUnicodeBreakType btype;
	const char *inptr = in;
	const char *start;
	GlyphInfo *glyph;
	double advance;
	gunichar c;
	
	word->advance = 0.0;
	word->count = 0;
	
	while (inptr < inend) {
		if (*inptr == '\r' || *inptr == '\n')
			break;
		
		// ignore invalid chars
		start = inptr;
		if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
			continue;
		
		btype = g_unichar_break_type (c);
		if (BreakSpace (btype)) {
			inptr = start;
			break;
		}
		
		word->count++;
		
		// ignore glyphs the font doesn't contain...
		if (!(glyph = word->font->GetGlyphInfo (c)))
			continue;
		
		// calculate total glyph advance
		if ((advance = glyph->metrics.horiAdvance) <= 0.0)
			continue;
		
		if ((prev != 0) && APPLY_KERNING (c))
			advance += word->font->Kerning (prev, glyph->index);
		else if (glyph->metrics.horiBearingX < 0)
			advance -= glyph->metrics.horiBearingX;
		
		// WrapWithOverflow never breaks in the middle of a word...
		//
		// If this word starts the line, then we must allow it to
		// overflow. Otherwise, return %true to our caller so it
		// can create a new line to layout this word into.
		if (!line_start && max_width > 0.0 && (word->line_advance + advance) >= max_width) {
			word->advance = 0.0;
			word->length = 0;
			return true;
		}
		
		word->line_advance += advance;
		word->advance += advance;
		prev = glyph->index;
	}
	
	word->length = (inptr - in);
	word->prev = prev;
	
	return false;
}

void
TextLayout::LayoutWrapWithOverflow ()
{
	TextLayoutAttributes *attrs, *nattrs;
	const char *inptr, *inend;
	TextLayoutLine *line;
	TextLayoutRun *run;
	LayoutWord word;
	TextFont *font;
	bool linebreak;
	int offset = 0;
	guint32 prev;
	
	if (!(attrs = (TextLayoutAttributes *) attributes->First ()) || attrs->start != 0)
		return;
	
	line = new TextLayoutLine (this, 0, 0);
	if (OverrideLineHeight ())
		line->height = line_height;
	
	g_ptr_array_add (lines, line);
	inptr = text;
	
	while (*inptr != '\0') {
		nattrs = (TextLayoutAttributes *) attrs->next;
		inend = text + (nattrs ? nattrs->start : length);
		run = new TextLayoutRun (line, attrs, inptr - text);
		g_ptr_array_add (line->runs, run);
		
		word.font = font = attrs->Font ();
		
		if (!OverrideLineHeight ()) {
			line->descend = MIN (line->descend, font->Descender ());
			line->height = MAX (line->height, font->Height ());
		}
		
		// layout until attrs change
		while (inptr < inend) {
			linebreak = false;
			prev = 0;
			
			// layout until eoln or until we reach max_width
			while (inptr < inend) {
				// check for line-breaks
				if (*inptr == '\r') {
					linebreak = true;
					run->count++;
					offset++;
					inptr++;
					if (*inptr == '\n') {
						run->count++;
						offset++;
						inptr++;
					}
					break;
				} else if (*inptr == '\n') {
					linebreak = true;
					run->count++;
					offset++;
					inptr++;
					break;
				}
				
				layout_word_init (&word, line->advance, prev);
				
				if (layout_word_overflow (&word, inptr, inend, max_width)) {
					// force a line wrap...
					linebreak = true;
					break;
				}
				
				if (word.length > 0) {
					// append the word to the run/line
					line->advance += word.advance;
					line->width = line->advance;
					run->advance += word.advance;
					run->count += word.count;
					offset += word.count;
					
					inptr += word.length;
					prev = word.prev;
				}
				
				// now append any trailing lwsp
				layout_word_init (&word, line->advance, prev);
				
				layout_word_lwsp (&word, inptr, inend);
				
				if (word.length > 0) {
					line->advance += word.advance;
					run->advance += word.advance;
					run->count += word.count;
					offset += word.count;
					
					inptr += word.length;
					prev = word.prev;
					
					// LWSP only counts toward line width if it is underlined
					if (attrs->IsUnderlined ())
						line->width = line->advance;
				}
			}
			
			// the current run has ended
			run->length = inptr - (text + run->start);
			
			if (linebreak || *inptr == '\0') {
				// the current line has ended
				line->length = inptr - (text + line->start);
				
				// update actual width extents
				actual_width = MAX (actual_width, line->width);
				
				// update actual height extents
				actual_height += line->height;
				
				if (*inptr != '\0') {
					// more text to layout... which means we'll need a new line
					line = new TextLayoutLine (this, inptr - text, offset);
					if (OverrideLineHeight ())
						line->height = line_height;
					
					g_ptr_array_add (lines, line);
				}
				
				if (inptr < inend) {
					// more text to layout with the current attrs...
					if (!OverrideLineHeight ()) {
						line->descend = font->Descender ();
						line->height = font->Height ();
					}
					
					run = new TextLayoutRun (line, attrs, inptr - text);
					g_ptr_array_add (line->runs, run);
				}
			}
		}
		
		attrs = nattrs;
	}
	
	count = offset;
}

void
TextLayout::LayoutNoWrap ()
{
	TextLayoutAttributes *attrs, *nattrs;
	const char *inptr, *inend;
	TextLayoutLine *line;
	TextLayoutRun *run;
	GlyphInfo *glyph;
	TextFont *font;
	bool linebreak;
	double advance;
	int offset = 0;
	guint32 prev;
	gunichar c;
	
	if (!(attrs = (TextLayoutAttributes *) attributes->First ()) || attrs->start != 0)
		return;
	
	line = new TextLayoutLine (this, 0, 0);
	if (OverrideLineHeight ())
		line->height = line_height;
	
	g_ptr_array_add (lines, line);
	inptr = text;
	
	while (*inptr != '\0') {
		nattrs = (TextLayoutAttributes *) attrs->next;
		inend = text + (nattrs ? nattrs->start : length);
		run = new TextLayoutRun (line, attrs, inptr - text);
		g_ptr_array_add (line->runs, run);
		
		font = attrs->Font ();
		if (!OverrideLineHeight ()) {
			line->descend = MIN (line->descend, font->Descender ());
			line->height = MAX (line->height, font->Height ());
		}
		
		// layout until attrs change
		while (inptr < inend) {
			linebreak = false;
			prev = 0;
			
			// layout until eoln
			while (inptr < inend) {
				// check for line-breaks
				if (*inptr == '\r') {
					linebreak = true;
					run->count++;
					offset++;
					inptr++;
					if (*inptr == '\n') {
						run->count++;
						offset++;
						inptr++;
					}
					break;
				} else if (*inptr == '\n') {
					linebreak = true;
					run->count++;
					offset++;
					inptr++;
					break;
				}
				
				// ignore invalid chars
				if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
					continue;
				
				run->count++;
				offset++;
				
				// ignore zero-width glyphs for layout metrics
				if (g_unichar_iszerowidth (c))
					continue;
				
				// canonicalize whitespace
				if (g_unichar_isspace (c))
					c = ' ';
				
				// ignore glyphs the font doesn't contain...
				if (!(glyph = font->GetGlyphInfo (c)))
					continue;
				
				// calculate total glyph advance
				if ((advance = glyph->metrics.horiAdvance) <= 0.0)
					continue;
				
				if ((prev != 0) && APPLY_KERNING (c))
					advance += font->Kerning (prev, glyph->index);
				else if (glyph->metrics.horiBearingX < 0)
					advance -= glyph->metrics.horiBearingX;
				
				line->advance += advance;
				run->advance += advance;
				prev = glyph->index;
				
				// LWSP only counts toward line width if it is underlined
				if (c != ' ' || attrs->IsUnderlined ())
					line->width = line->advance;
			}
			
			// the current run has ended
			run->length = inptr - (text + run->start);
			
			if (linebreak || *inptr == '\0') {
				// the current line has ended
				line->length = inptr - (text + line->start);
				
				// update actual width extents
				actual_width = MAX (actual_width, line->width);
				
				// update actual height extents
				actual_height += line->height;
				
				if (linebreak) {
					// more text to layout... which means we'll need a new line
					line = new TextLayoutLine (this, inptr - text, offset);
					
					if (!OverrideLineHeight ()) {
						if (*inptr == '\0' || inptr < inend) {
							line->descend = font->Descender ();
							line->height = font->Height ();
						}
					} else {
						line->height = line_height;
					}
					
					g_ptr_array_add (lines, line);
					prev = 0;
				}
				
				if (inptr < inend) {
					// more text to layout with the current attrs...
					if (!OverrideLineHeight ()) {
						line->descend = font->Descender ();
						line->height = font->Height ();
					}
					
					run = new TextLayoutRun (line, attrs, inptr - text);
					g_ptr_array_add (line->runs, run);
				}
			}
		}
		
		attrs = nattrs;
	}
	
	count = offset;
}

/**
 * layout_word_wrap:
 * @word: word state
 * @in: start of word
 * @inend = end of word
 * @max_width: max allowable width for a line
 *
 * Calculates the advance of the current word, breaking if needed.
 *
 * Returns: %true if the caller should create a new line for the
 * remainder of the word or %false otherwise.
 **/
static bool
layout_word_wrap (LayoutWord *word, const char *in, const char *inend, double max_width)
{
	bool line_start = word->line_advance == 0.0;
	guint32 prev = word->prev;
	GUnicodeBreakType btype;
	WordBreakOpportunity op;
	const char *inptr = in;
	const char *start;
	bool wrap = false;
	GlyphInfo *glyph;
	double advance;
	bool log = false;
	int glyphs = 0;
	gunichar c;
	
	g_array_set_size (word->break_ops, 0);
	word->advance = 0.0;
	word->count = 0;
	
	if (!strncmp (inptr, "bbb?", 4))
		log = true;
	
	while (inptr < inend) {
		if (*inptr == '\r' || *inptr == '\n')
			break;
		
		// ignore invalid chars
		start = inptr;
		if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
			continue;
		
		btype = g_unichar_break_type (c);
		if (BreakSpace (btype)) {
			inptr = start;
			break;
		}
		
		word->count++;
		
		if (g_unichar_combining_class (c) != 0 && glyphs > 0) {
			// this char gets combined with the previous glyph
			g_array_remove_index (word->break_ops, word->break_ops->len - 1);
			op.count = word->count;
			g_array_append_val (word->break_ops, op);
			continue;
		} else {
			glyphs++;
		}
		
		// ignore glyphs the font doesn't contain...
		if (!(glyph = word->font->GetGlyphInfo (c)))
			continue;
		
		// calculate total glyph advance
		if ((advance = glyph->metrics.horiAdvance) > 0.0) {
			if ((prev != 0) && APPLY_KERNING (c))
				advance += word->font->Kerning (prev, glyph->index);
			else if (glyph->metrics.horiBearingX < 0)
				advance -= glyph->metrics.horiBearingX;
		} else {
			advance = 0.0;
		}
		
		word->line_advance += advance;
		word->advance += advance;
		prev = glyph->index;
		
		op.advance = word->advance;
		op.count = word->count;
		op.inptr = inptr;
		op.btype = btype;
		op.c = c;
		
		g_array_append_val (word->break_ops, op);
		
		if (max_width > 0.0 && word->line_advance >= max_width) {
			if (inptr < word->last_word) {
				// not the last word, safe to break apart
				wrap = true;
				break;
			} else if (!line_start) {
				// last word, but not located at the beginning of the line...
				word->advance = 0.0;
				word->length = 0;
				return true;
			} else {
				// last word, at the beginning of the line - DO NOT BREAK
			}
		}
	}
	
	if (!wrap) {
		word->length = (inptr - in);
		word->prev = prev;
		return false;
	}
	
	// glyphs should always be > 0, but just in case...
	if (glyphs > 0) {
		// keep going until we reach a new distinct glyph
		while (inptr < inend) {
			if (*inptr == '\r' || *inptr == '\n')
				break;
			
			// ignore invalid chars
			start = inptr;
			if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
				continue;
			
			btype = g_unichar_break_type (c);
			if (BreakSpace (btype) || g_unichar_combining_class (c) == 0) {
				inptr = start;
				break;
			}
			
			word->count++;
			
			g_array_remove_index (word->break_ops, word->break_ops->len - 1);
			op.count = word->count;
			g_array_append_val (word->break_ops, op);
		}
	}
	
	if (log)
		printf ("measure for %.*s is %f, time to work back...\n", inptr - in, in, word->advance);
	
	// at this point, we're going to break the word so we can reset kerning
	word->prev = 0;
	
	// we can't break any smaller than a single glyph
	if (line_start && glyphs == 1) {
		word->length = (inptr - in);
		word->prev = prev;
		return true;
	}
	
	// search backwards for the best break point
	for (guint i = word->break_ops->len; i > 0; i--) {
		op = g_array_index (word->break_ops, WordBreakOpportunity, i - 1);
		
		switch (op.btype) {
		case G_UNICODE_BREAK_NEXT_LINE:
		case G_UNICODE_BREAK_UNKNOWN:
			// break after this char
			if (log)
				printf ("break next-line\n");
			word->length = (op.inptr - in);
			word->advance = op.advance;
			word->count = op.count;
			return true;
		case G_UNICODE_BREAK_BEFORE_AND_AFTER:
		case G_UNICODE_BREAK_EXCLAMATION:
			//case G_UNICODE_BREAK_AFTER:
			// only break after this char if there are chars before it
			if (line_start && i > 1) {
				if (log)
					printf ("break before-and-after\n");
				word->length = (op.inptr - in);
				word->advance = op.advance;
				word->count = op.count;
				return true;
			}
			break;
		case G_UNICODE_BREAK_BEFORE:
			if (i > 1) {
				if (log)
					printf ("break before\n");
				
				// break after the previous glyph
				op = g_array_index (word->break_ops, WordBreakOpportunity, i - 2);
				word->length = (op.inptr - in);
				word->advance = op.advance;
				word->count = op.count;
				
				return true;
			}
			break;
		case G_UNICODE_BREAK_WORD_JOINER:
			// only break if there is nothing before it
			if (i == 1) {
				if (log)
					printf ("break word-joiner\n");
				word->length = (op.inptr - in);
				word->advance = op.advance;
				word->count = op.count;
				return true;
			}
			break;
		default:
			// only break if we have no choice...
			if (line_start) {
				if (log)
					printf ("break default\n");
				if (i > 1) {
					// break after the previous glyph
					op = g_array_index (word->break_ops, WordBreakOpportunity, i - 2);
				} else {
					// break after this char
				}
				
				word->length = (op.inptr - in);
				word->advance = op.advance;
				word->count = op.count;
				
				return true;
			}
			break;
		}
	}
	
	if (log)
		printf ("breaking before the word...\n");
	
	word->advance = 0.0;
	word->length = 0;
	word->count = 0;
	
	return true;
}

static const char *
FindLastWord (List *attributes, const char *text, int length)
{
	TextLayoutAttributes *attrs = (TextLayoutAttributes *) attributes->Last ();
	const char *inend = text + length;
	const char *inptr, *pchar;
	const char *last_word;
	gunichar c;
	
	// get a rough iea of where the last 'word' starts
	last_word = utf8_find_last_word (text, length);
	
	// we now need to scan backwards through the attribute runs... if
	// the word is split between attribute runs (aka TextBlock Runs),
	// then the we must treat the last run with a portion of the word
	// as the starting location of the last word.
	while (attrs && last_word < text + attrs->start) {
		pchar = inptr = text + attrs->start;
		
		// find the first valid unichar
		while (inptr < inend) {
			if ((c = utf8_getc (&inptr, inend - inptr)) != (gunichar) -1)
				break;
			pchar = inptr;
		}
		
		if (inptr < inend && !g_unichar_isspace (c)) {
			// woot, we found the beginning of the last word segment
			last_word = pchar;
			break;
		}
		
		attrs = (TextLayoutAttributes *) attrs->next;
	}
	
	return last_word;
}

void
TextLayout::LayoutWrap ()
{
	TextLayoutAttributes *attrs, *nattrs;
	const char *inptr, *inend;
	TextLayoutLine *line;
	TextLayoutRun *run;
	LayoutWord word;
	TextFont *font;
	bool linebreak;
	int offset = 0;
	guint32 prev;
	
	if (!(attrs = (TextLayoutAttributes *) attributes->First ()) || attrs->start != 0)
		return;
	
	if (last_word == NULL)
		last_word = FindLastWord (attributes, text, length);
	
	word.break_ops = g_array_new (false, false, sizeof (WordBreakOpportunity));
	word.last_word = last_word;
	
	line = new TextLayoutLine (this, 0, 0);
	if (OverrideLineHeight ())
		line->height = line_height;
	
	g_ptr_array_add (lines, line);
	inptr = text;
	
	while (*inptr != '\0') {
		nattrs = (TextLayoutAttributes *) attrs->next;
		inend = text + (nattrs ? nattrs->start : length);
		run = new TextLayoutRun (line, attrs, inptr - text);
		g_ptr_array_add (line->runs, run);
		
		word.font = font = attrs->Font ();
		
		if (!OverrideLineHeight ()) {
			line->descend = MIN (line->descend, font->Descender ());
			line->height = MAX (line->height, font->Height ());
		}
		
		// layout until attrs change
		while (inptr < inend) {
			linebreak = false;
			prev = 0;
			
			// layout until eoln or until we reach max_width
			while (inptr < inend) {
				// check for line-breaks
				if (*inptr == '\r') {
					linebreak = true;
					run->count++;
					offset++;
					inptr++;
					if (*inptr == '\n') {
						run->count++;
						offset++;
						inptr++;
					}
					break;
				} else if (*inptr == '\n') {
					linebreak = true;
					run->count++;
					offset++;
					inptr++;
					break;
				}
				
				layout_word_init (&word, line->advance, prev);
				
				linebreak = layout_word_wrap (&word, inptr, inend, max_width);
				
				if (word.length > 0) {
					// append the word to the run/line
					line->advance += word.advance;
					line->width = line->advance;
					run->advance += word.advance;
					run->count += word.count;
					offset += word.count;
					
					inptr += word.length;
					prev = word.prev;
				}
				
				// now append any trailing lwsp
				layout_word_init (&word, line->advance, prev);
				
				layout_word_lwsp (&word, inptr, inend);
				
				if (word.length > 0) {
					line->advance += word.advance;
					run->advance += word.advance;
					run->count += word.count;
					offset += word.count;
					
					inptr += word.length;
					prev = word.prev;
					
					// LWSP only counts toward line width if it is underlined
					if (attrs->IsUnderlined ())
						line->width = line->advance;
				}
				
				if (linebreak)
					break;
			}
			
			// the current run has ended
			run->length = inptr - (text + run->start);
			
			if (linebreak || *inptr == '\0') {
				// the current line has ended
				line->length = inptr - (text + line->start);
				
				// update actual width extents
				actual_width = MAX (actual_width, line->width);
				
				// update actual height extents
				actual_height += line->height;
				
				if (*inptr != '\0') {
					// more text to layout... which means we'll need a new line
					line = new TextLayoutLine (this, inptr - text, offset);
					if (OverrideLineHeight ())
						line->height = line_height;
					
					g_ptr_array_add (lines, line);
				}
				
				if (inptr < inend) {
					if (!OverrideLineHeight ()) {
						line->descend = font->Descender ();
						line->height = font->Height ();
					}
					
					// more text to layout with the current attrs...
					run = new TextLayoutRun (line, attrs, inptr - text);
					g_ptr_array_add (line->runs, run);
				}
			}
		}
		
		attrs = nattrs;
	}
	
	g_array_free (word.break_ops, true);
	
	count = offset;
}

static void
print_lines (GPtrArray *lines)
{
	TextLayoutLine *line;
	TextLayoutRun *run;
	const char *text;
	double y = 0.0;
	
	for (guint i = 0; i < lines->len; i++) {
		line = (TextLayoutLine *) lines->pdata[i];
		
		printf ("Line (top=%f, height=%f, advance=%f, offset=%d):\n", y, line->height, line->advance, line->offset);
		for (guint j = 0; j < line->runs->len; j++) {
			run = (TextLayoutRun *) line->runs->pdata[j];
			
			text = line->layout->GetText () + run->start;
			
			printf ("\tRun (advance=%f): \"", run->advance);
			for (const char *s = text; s < text + run->length; s++) {
				switch (*s) {
				case '\r':
					fputs ("\\r", stdout);
					break;
				case '\n':
					fputs ("\\n", stdout);
					break;
				case '\t':
					fputs ("\\t", stdout);
					break;
				case '"':
					fputs ("\\\"", stdout);
					break;
				default:
					fputc (*s, stdout);
					break;
				}
			}
			printf ("\"\n");
		}
		
		y += line->height;
	}
}

void
TextLayout::Layout ()
{
	if (actual_width != -1.0)
		return;
	
	actual_height = 0.0;
	actual_width = 0.0;
	ClearLines ();
	
	if (text == NULL)
		return;
	
	switch (wrapping) {
	case TextWrappingWrapWithOverflow:
#if DEBUG
		if (debug_flags & RUNTIME_DEBUG_LAYOUT) {
			if (max_width > 0.0)
				printf ("TextLayout::LayoutWrapWithOverflow(%f)\n", max_width);
			else
				printf ("TextLayout::LayoutWrapWithOverflow()\n");
		}
#endif
		LayoutWrapWithOverflow ();
		break;
	case TextWrappingNoWrap:
#if DEBUG
		if (debug_flags & RUNTIME_DEBUG_LAYOUT) {
			if (max_width > 0.0)
				printf ("TextLayout::LayoutWrapNoWrap(%f)\n", max_width);
			else
				printf ("TextLayout::LayoutNoWrap()\n");
		}
#endif
		LayoutNoWrap ();
		break;
	case TextWrappingWrap:
	// Silverlight default is to wrap for invalid values
	default:
#if DEBUG
		if (debug_flags & RUNTIME_DEBUG_LAYOUT) {
			if (max_width > 0.0)
				printf ("TextLayout::LayoutWrap(%f)\n", max_width);
			else
				printf ("TextLayout::LayoutWrap()\n");
		}
#endif
		LayoutWrap ();
		break;
	}
	
#if DEBUG
	if (debug_flags & RUNTIME_DEBUG_LAYOUT) {
		print_lines (lines);
		printf ("actualWidth = %f, actualHeight = %f\n", actual_width, actual_height);
	}
#endif
}

static inline TextLayoutGlyphCluster *
GenerateGlyphCluster (TextFont *font, guint32 *kern, const char *text, int start, int length)
{
	TextLayoutGlyphCluster *cluster = new TextLayoutGlyphCluster (start, length);
	const char *inend = text + start + length;
	const char *inptr = text + start;
	guint32 prev = *kern;
	GlyphInfo *glyph;
	double x0, y0;
	int size = 0;
	gunichar c;
	
	// set y0 to the baseline
	y0 = font->Ascender ();
	x0 = 0.0;
	
	// count how many path data items we'll need to allocate
	while (inptr < inend) {
		if (*inptr == '\r' || *inptr == '\n')
			break;
		
		if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
			continue;
		
		// canonicalize lwsp
		if (g_unichar_isspace (c) && !g_unichar_iszerowidth (c))
			c = ' ';
		
		if (!(glyph = font->GetGlyphInfo (c)))
			continue;
		
		size += glyph->path->cairo.num_data + 1;
	}
	
	if (size > 0) {
		// generate the cached path for the cluster
		cluster->path = moon_path_new (size);
		inptr = text + start;
		
		while (inptr < inend) {
			if (*inptr == '\r' || *inptr == '\n')
				break;
			
			if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
				continue;
			
			// canonicalize lwsp
			if (g_unichar_isspace (c) && !g_unichar_iszerowidth (c))
				c = ' ';
			
			if (!(glyph = font->GetGlyphInfo (c)))
				continue;
			
			if ((prev != 0) && APPLY_KERNING (c))
				x0 += font->Kerning (prev, glyph->index);
			else if (glyph->metrics.horiBearingX < 0)
				x0 += glyph->metrics.horiBearingX;
			
			font->AppendPath (cluster->path, glyph, x0, y0);
			x0 += glyph->metrics.horiAdvance;
			prev = glyph->index;
		}
		
		moon_close_path (cluster->path);
	}
	
	cluster->advance = x0;
	
	*kern = prev;
	
	return cluster;
}

void
TextLayoutRun::GenerateCache ()
{
	int selection_length = line->layout->GetSelectionLength ();
	int selection_start = line->layout->GetSelectionStart ();
	const char *text = line->layout->GetText ();
	const char *inend = text + start + length;
	const char *inptr = text + start;
	TextFont *font = attrs->Font ();
	TextLayoutGlyphCluster *cluster;
	const char *selection_end;
	guint32 prev = 0;
	int len;
	
	// cache the glyph cluster leading up to the selection
	if (selection_length == 0 || start < selection_start) {
		if (selection_length > 0)
			len = MIN (selection_start - start, length);
		else
			len = length;
		
		cluster = GenerateGlyphCluster (font, &prev, text, start, len);
		g_ptr_array_add (clusters, cluster);
		inptr += len;
	}
	
	// cache the selected glyph cluster
	selection_end = text + selection_start + selection_length;
	if (inptr < inend && inptr < selection_end) {
		len = MIN (inend, selection_end) - inptr;
		cluster = GenerateGlyphCluster (font, &prev, text, inptr - text, len);
		g_ptr_array_add (clusters, cluster);
		cluster->selected = true;
		inptr += len;
	}
	
	// cache the glyph cluster following the selection
	if (inptr < inend) {
		cluster = GenerateGlyphCluster (font, &prev, text, inptr - text, inend - inptr);
		g_ptr_array_add (clusters, cluster);
		inptr = inend;
	}
}

void
TextLayoutGlyphCluster::Render (cairo_t *cr, const Point &origin, TextLayoutAttributes *attrs, const char *text, double x, double y)
{
	TextFont *font = attrs->Font ();
	GlyphInfo *glyph;
	Brush *brush;
	double y0;
	Rect area;
	int crlf;
	
	// y is the baseline, set the origin to the top-left
	cairo_translate (cr, x, y - font->Ascender ());
	
	// set y0 to the baseline relative to the translation matrix
	y0 = font->Ascender ();
	
	if (selected) {
		area = Rect (origin.x, origin.y, advance, font->Height ());
		
		// extend the selection bg by the width of a SPACE if it includes CRLF
		crlf = start + length - 1;
		if (text[crlf] == '\r' || text[crlf] == '\n') {
			if ((glyph = font->GetGlyphInfo (' ')))
				area.width += glyph->metrics.horiAdvance;
		}
		
		// render the selection background
		brush = attrs->Background (true);
		brush->SetupBrush (cr, area);
		cairo_new_path (cr);
		cairo_rectangle (cr, area.x, area.y, area.width, area.height);
		brush->Fill (cr);
	}
	
	// setup the foreground brush
	area = Rect (origin.x, origin.y, advance, font->Height ());
	brush = attrs->Foreground (selected);
	brush->SetupBrush (cr, area);
	cairo_new_path (cr);
	
	if (path && path->cairo.data)
		cairo_append_path (cr, &path->cairo);
	
	brush->Fill (cr);
	
	if (attrs->IsUnderlined ()) {
		double thickness = font->UnderlineThickness ();
		double pos = y0 + font->UnderlinePosition ();
		
		cairo_set_line_width (cr, thickness);
		
		cairo_new_path (cr);
		Rect underline = Rect (0.0, pos - thickness * 0.5, advance, thickness);
		underline.Draw (cr);
		
		brush->Fill (cr);
	}
}

void
TextLayoutRun::Render (cairo_t *cr, const Point &origin, double x, double y)
{
	const char *text = line->layout->GetText ();
	TextLayoutGlyphCluster *cluster;
	double x0 = x;
	
	if (clusters->len == 0)
		GenerateCache ();
	
	for (guint i = 0; i < clusters->len; i++) {
		cluster = (TextLayoutGlyphCluster *) clusters->pdata[i];
		
		cairo_save (cr);
		cluster->Render (cr, origin, attrs, text, x0, y);
		cairo_restore (cr);
		
		x0 += cluster->advance;
	}
}

void
TextLayoutLine::Render (cairo_t *cr, const Point &origin, double left, double top)
{
	TextLayoutRun *run;
	double x0, y0;
	
	// set y0 to the line's baseline (descend is a negative value)
	y0 = top + height + descend;
	x0 = left;
	
	for (guint i = 0; i < runs->len; i++) {
		run = (TextLayoutRun *) runs->pdata[i];
		run->Render (cr, origin, x0, y0);
		x0 += run->advance;
	}
}

double
TextLayout::HorizontalAlignment (double line_width)
{
	double deltax;
	
	switch (alignment) {
	case TextAlignmentCenter:
		if (line_width < max_width)
			deltax = (max_width - line_width) / 2.0;
		else
			deltax = 0.0;
		break;
	case TextAlignmentRight:
		if (line_width < max_width)
			deltax = max_width - line_width;
		else
			deltax = 0.0;
		break;
	default:
		deltax = 0.0;
		break;
	}
	
	return deltax;
}

void
TextLayout::Render (cairo_t *cr, const Point &origin, const Point &offset)
{
	TextLayoutLine *line;
	double x, y;
	
	y = offset.y;
	
	Layout ();
	
	for (guint i = 0; i < lines->len; i++) {
		line = (TextLayoutLine *) lines->pdata[i];
		
		x = offset.x + HorizontalAlignment (line->advance);
		line->Render (cr, origin, x, y);
		y += (double) line->height;
	}
}

TextLayoutLine *
TextLayout::GetLineFromY (const Point &offset, double y)
{
	TextLayoutLine *line = NULL;
	double y0, y1;
	
	//printf ("TextLayout::GetLineFromY (%.2g)\n", y);
	
	y0 = offset.y;
	
	for (guint i = 0; i < lines->len; i++) {
		line = (TextLayoutLine *) lines->pdata[i];
		
		// set y1 the top of the next line
		y1 = y0 + line->height;
		
		if (y < y1) {
			// we found the line that the point is located on
			return line;
		}
		
		y0 = y1;
	}
	
	return NULL;
}

int
TextLayout::GetCursorFromXY (const Point &offset, double x, double y)
{
	const char *inend, *ch, *inptr;
	TextLayoutRun *run = NULL;
	TextLayoutLine *line;
	GlyphInfo *glyph;
	guint32 prev = 0;
	TextFont *font;
	int cursor;
	gunichar c;
	double x0;
	double m;
	guint i;
	
	//printf ("TextLayout::GetCursorFromXY (%.2g, %.2g)\n", x, y);
	
	if (y < offset.y)
		return 0;
	
	if (!(line = GetLineFromY (offset, y)))
		return count;
	
	// adjust x0 for horizontal alignment
	x0 = offset.x + HorizontalAlignment (line->advance);
	
	inptr = text + line->start;
	cursor = line->offset;
	
	for (i = 0; i < line->runs->len; i++) {
		run = (TextLayoutRun *) line->runs->pdata[i];
		
		if (x < x0 + run->advance) {
			// x is in somewhere inside this run
			break;
		}
		
		// x is beyond this run
		cursor += run->count;
		inptr += run->length;
		x0 += run->advance;
		run = NULL;
	}
	
	if (run != NULL) {
		inptr = text + run->start;
		inend = inptr + run->length;
		font = run->attrs->Font ();
		
		while (inptr < inend) {
			if (*inptr == '\r' || *inptr == '\n')
				break;
			
			ch = inptr;
			if ((c = utf8_getc (&inptr, inend - inptr)) == (gunichar) -1)
				continue;
			
			cursor++;
			
			if (g_unichar_iszerowidth (c))
				continue;
			
			if (g_unichar_isspace (c))
				c = ' ';
			
			if (!(glyph = font->GetGlyphInfo (c)))
				continue;
			
			if ((prev != 0) && APPLY_KERNING (c))
				x0 += font->Kerning (prev, glyph->index);
			else if (glyph->metrics.horiBearingX < 0)
				x0 += glyph->metrics.horiBearingX;
			
			// calculate midpoint of the character
			m = glyph->metrics.horiAdvance / 2.0;
			
			// if x is <= the midpoint, then the cursor is
			// considered to be at the start of this character.
			if (x <= x0 + m) {
				inptr = ch;
				cursor--;
				break;
			}
			
			x0 += glyph->metrics.horiAdvance;
			prev = glyph->index;
		}
	} else if (i > 0) {
		// x is beyond the end of the last run
		run = (TextLayoutRun *) line->runs->pdata[i - 1];
		inend = text + run->start + run->length;
		inptr = text + run->start;
		
		if (inend > inptr && inend[-1] == '\n') {
			cursor--;
			inend--;
		}
		
		if (inend > inptr && inend[-1] == '\r') {
			cursor--;
			inend--;
		}
	}
	
	return cursor;
}

Rect
TextLayout::GetCursor (const Point &offset, int index)
{
	const char *cursor = g_utf8_offset_to_pointer (text, index);
	double height, x0, y0, y1;
	const char *inptr, *inend;
	TextLayoutLine *line;
	TextLayoutRun *run;
	GlyphInfo *glyph;
	TextFont *font;
	guint32 prev;
	gunichar c;
	
	//printf ("TextLayout::GetCursor (%d)\n", index);
	
	x0 = offset.x;
	y0 = offset.y;
	height = 0.0;
	y1 = 0.0;
	
	for (guint i = 0; i < lines->len; i++) {
		line = (TextLayoutLine *) lines->pdata[i];
		inend = text + line->start + line->length;
		
		// adjust x0 for horizontal alignment
		x0 = offset.x + HorizontalAlignment (line->advance);
		
		// set y1 to the baseline (descend is a negative value)
		y1 = y0 + line->height + line->descend;
		height = line->height;
		
		//printf ("\tline: left=%.2g, top=%.2g, baseline=%.2g, start index=%d\n", x0, y0, y1, cur);
		
		if (cursor >= inend) {
			// maybe the cursor is on the next line...
			if ((i + 1) == lines->len) {
				// we are on the last line...
				if ((inend > text + line->start) &&
				    (inend[-1] == '\r' || inend[-1] == '\n')) {
					// cursor is on the next line by itself
					x0 = offset.x + HorizontalAlignment (0.0);
					y0 += line->height;
				} else {
					// cursor at the end of the last line
					x0 += line->advance;
				}
				
				break;
			}
			
			y0 += line->height;
			continue;
		}
		
		// cursor is on this line...
		for (guint j = 0; j < line->runs->len; j++) {
			run = (TextLayoutRun *) line->runs->pdata[j];
			inend = text + run->start + run->length;
			
			if (cursor >= inend) {
				// maybe the cursor is in the next run...
				x0 += run->advance;
				continue;
			}
			
			// cursor is in this run...
			font = run->attrs->Font ();
			inptr = text + run->start;
			prev = 0;
			
			while (inptr < cursor) {
				if ((c = utf8_getc (&inptr, cursor - inptr)) == (gunichar) -1)
					continue;
				
				if (g_unichar_iszerowidth (c))
					continue;
				
				if (g_unichar_isspace (c))
					c = ' ';
				
				if (!(glyph = font->GetGlyphInfo (c)))
					continue;
				
				if ((prev != 0) && APPLY_KERNING (c))
					x0 += font->Kerning (prev, glyph->index);
				else if (glyph->metrics.horiBearingX < 0)
					x0 += glyph->metrics.horiBearingX;
				
				x0 += glyph->metrics.horiAdvance;
				prev = glyph->index;
			}
			
			break;
		}
		
		break;
	}
	
	return Rect (x0, y0, 1.0, height);
}
