#include "document.h"
#include "html.h"
#include "latex.h"

#include "common.h"
#include "utils.h"
#include <time.h>

/* FEATURES INFO / DEFAULTS */

enum renderer_type {
	RENDERER_HTML,
	RENDERER_LATEX,
	RENDERER_HTML_TOC
};

struct extension_category_info {
	unsigned int flags;
	const char *option_name;
	const char *label;
};

struct extension_info {
	unsigned int flag;
	const char *option_name;
	const char *description;
};

struct html_flag_info {
	unsigned int flag;
	const char *option_name;
	const char *description;
};

static struct extension_category_info categories_info[] = {
	{HOEDOWN_EXT_BLOCK, "block", "Block extensions"},
	{HOEDOWN_EXT_SPAN, "span", "Span extensions"},
	{HOEDOWN_EXT_FLAGS, "flags", "Other flags"},
	{HOEDOWN_EXT_NEGATIVE, "negative", "Negative flags"},
};

static struct extension_info extensions_info[] = {
	{HOEDOWN_EXT_TABLES, "tables", "Parse PHP-Markdown style tables."},
	{HOEDOWN_EXT_FENCED_CODE, "fenced-code", "Parse fenced code blocks."},
	{HOEDOWN_EXT_FOOTNOTES, "footnotes", "Parse footnotes."},

	{HOEDOWN_EXT_AUTOLINK, "autolink", "Automatically turn safe URLs into links."},
	{HOEDOWN_EXT_STRIKETHROUGH, "strikethrough", "Parse ~~stikethrough~~ spans."},
	{HOEDOWN_EXT_UNDERLINE, "underline", "Parse _underline_ instead of emphasis."},
	{HOEDOWN_EXT_HIGHLIGHT, "highlight", "Parse ==highlight== spans."},
	{HOEDOWN_EXT_QUOTE, "quote", "Render \"quotes\" as <q>quotes</q>."},
	{HOEDOWN_EXT_SUPERSCRIPT, "superscript", "Parse super^script."},
	{HOEDOWN_EXT_MATH, "math", "Parse TeX $$math$$ syntax, Kramdown style."},

	{HOEDOWN_EXT_NO_INTRA_EMPHASIS, "disable-intra-emphasis", "Disable emphasis_between_words."},
	{HOEDOWN_EXT_SPACE_HEADERS, "space-headers", "Require a space after '#' in headers."},
	{HOEDOWN_EXT_MATH_EXPLICIT, "math-explicit", "Instead of guessing by context, parse $inline math$ and $$always block math$$ (requires --math)."},
	{HOEDOWN_EXT_SCI, "scidown", "SciDown Extension"},
	{HOEDOWN_EXT_DISABLE_INDENTED_CODE, "disable-indented-code", "Don't parse indented code blocks."},
};

static struct html_flag_info html_flags_info[] = {
	{SCIDOWN_RENDER_SKIP_HTML, "skip-html", "Strip all HTML tags."},
	{SCIDOWN_RENDER_ESCAPE, "escape", "Escape all HTML."},
	{SCIDOWN_RENDER_HARD_WRAP, "hard-wrap", "Render each linebreak as <br>."},
	{SCIDOWN_RENDER_USE_XHTML, "xhtml", "Render XHTML."},
	{SCIDOWN_RENDER_MERMAID, "mermaid", "Render mermaid diagrams."},
	{SCIDOWN_RENDER_GNUPLOT, "gnuplot", "Render gnuplot plot."},
	{SCIDOWN_RENDER_CSS, "style", "Set specified style-sheet."}
};

static const char *category_prefix = "all-";
static const char *negative_prefix = "no-";

#define DEF_IUNIT 1024
#define DEF_OUNIT 64
#define DEF_MAX_NESTING 16

/* Get local info */
localization get_local()
{
  localization local;
  local.figure = "Figure";
  local.listing = "Listing";
  local.table = "Table";
  return local;
}


/* PRINT HELP */

void
print_help(const char *basename)
{
	size_t i;
	size_t e;

	/* usage */
	printf("Usage: %s [OPTION]... [FILE]\n\n", basename);

	/* description */
	printf("Process the Markdown in FILE (or standard input) and render it to standard output, using the Hoedown library. "
	       "Parsing and rendering can be customized through the options below. The default is to parse pure markdown and output HTML.\n\n");

	/* main options */
	printf("Main options:\n");
	print_option('n', "max-nesting=N", "Maximum level of block nesting parsed. Default is " str(DEF_MAX_NESTING) ".");
	print_option('t', "toc-level=N", "Maximum level for headers included in the TOC. Zero disables TOC (the default).");
	print_option(  0, "html", "Render (X)HTML. The default.");
	print_option(  0, "latex", "Render as LATEX.");
	print_option(  0, "html-toc", "Render the Table of Contents in (X)HTML.");


	print_option('T', "time", "Show time spent in rendering.");
	print_option('i', "input-unit=N", "Reading block size. Default is " str(DEF_IUNIT) ".");
	print_option('o', "output-unit=N", "Writing block size. Default is " str(DEF_OUNIT) ".");
	print_option('h', "help", "Print this help text.");
	print_option('v', "version", "Print Hoedown version.");
	printf("\n");

	/* extensions */
	for (i = 0; i < count_of(categories_info); i++) {
		struct extension_category_info *category = categories_info+i;
		printf("%s (--%s%s):\n", category->label, category_prefix, category->option_name);
		for (e = 0; e < count_of(extensions_info); e++) {
			struct extension_info *extension = extensions_info+e;
			if (extension->flag & category->flags) {
				print_option(  0, extension->option_name, extension->description);
			}
		}
		printf("\n");
	}

	/* html-specific */
	printf("HTML-specific options:\n");
	for (i = 0; i < count_of(html_flags_info); i++) {
		struct html_flag_info *html_flag = html_flags_info+i;
		print_option(  0, html_flag->option_name, html_flag->description);
	}
	printf("\n");

	/* ending */
	printf("Flags and extensions can be negated by prepending 'no' to them, as in '--no-tables', '--no-span' or '--no-escape'. "
	       "Options are processed in order, so in case of contradictory options the last specified stands.\n\n");

	printf("When FILE is '-', read standard input. If no FILE was given, read standard input. Use '--' to signal end of option parsing. "
	       "Exit status is 0 if no errors occurred, 1 with option parsing errors, 4 with memory allocation errors or 5 with I/O errors.\n\n");
}


/* OPTION PARSING */

struct option_data {
	/* time reporting */
	int show_time;

	/* I/O */
	size_t iunit;
	size_t ounit;

	/* renderer */
	enum renderer_type renderer;
	int toc_level;
	scidown_render_flags render_flags;

	/* parsing */
	hoedown_extensions extensions;
	size_t max_nesting;
};

int md2html(const uint8_t* input_data, size_t input_size, uint8_t** output_data, size_t* output_size)
{
	struct option_data data;
	clock_t t1, t2;
	FILE *file = stdin;
	hoedown_buffer *ib, *ob;
	hoedown_renderer *renderer = NULL;
	void (*renderer_free)(hoedown_renderer *) = NULL;
	hoedown_document *document;

	/* Parse options */
	data.show_time = 1;
	data.iunit = DEF_IUNIT;
	data.ounit = DEF_OUNIT;
	data.renderer = RENDERER_HTML;
	data.toc_level = 0;
	data.render_flags = SCIDOWN_RENDER_MERMAID | SCIDOWN_RENDER_CHARTER | SCIDOWN_RENDER_GNUPLOT | SCIDOWN_RENDER_CSS;
	data.extensions = HOEDOWN_EXT_BLOCK | HOEDOWN_EXT_SPAN | HOEDOWN_EXT_FLAGS;
	data.max_nesting = DEF_MAX_NESTING;
	/* Read everything */
	ib = hoedown_buffer_new(data.iunit);
    hoedown_buffer_set(ib, input_data, input_size);

	/* Create the renderer */
	if (data.renderer == RENDERER_HTML)
		renderer = hoedown_html_renderer_new(data.render_flags, data.toc_level, get_local());
	else if (data.renderer == RENDERER_HTML_TOC)
		renderer = hoedown_html_toc_renderer_new(data.toc_level, get_local());
	else if (data.renderer == RENDERER_LATEX)
		renderer = scidown_latex_renderer_new(data.render_flags, data.toc_level, get_local());
	renderer_free = hoedown_html_renderer_free;

	/* Perform Markdown rendering */
	ob = hoedown_buffer_new(data.ounit);

	ext_definition ext = {NULL, NULL};
	if (data.renderer == RENDERER_HTML) {
		ext.extra_header =
                "<link rel=\"stylesheet\" href=\"qrc:/web_res/ajax/libs/KaTeX/0.11.1/katex.min.css\" crossorigin=\"anonymous\">"
                "<link rel=\"stylesheet\" href=\"qrc:/web_res/ajax/libs/highlight.js/9.18.1/styles/xcode.min.css\">"
                "<script src=\"qrc:/web_res/ajax/libs/KaTeX/0.11.1/katex.min.js\" crossorigin=\"anonymous\"></script>\n"
                "<script src=\"qrc:/web_res/ajax/libs/KaTeX/0.11.1/contrib/auto-render.min.js\" crossorigin=\"anonymous\"></script>\n"
                "<script src=\"qrc:/web_res/ajax/libs/highlight.js/9.18.1/highlight.min.js\"></script>"
                "<script src=\"qrc:/web_res/npm/mermaid@8.4.0/dist/mermaid.min.js\"></script>"
							;
		ext.extra_closing = "<style>@font-face {\n"
                            "    font-family: 'HiraginoSans';\n"
                            "    src: url('qrc:/web_res/HiraginoSansGBW6.otf');\n"
                            "    font-weight: 300;\n"
                            "    font-style: normal;\n"
                            "  }"
                            "body {"
                            "   font-family: 'HiraginoSans';"
                            "}"
                            " </style>"
		        "<script>renderMathInElement(document.body); hljs.initHighlightingOnLoad(); mermaid.initialize({startOnLoad:true});</script>\n";
	}
	document = hoedown_document_new(renderer, data.extensions, &ext, NULL, data.max_nesting);

	t1 = clock();
	hoedown_document_render(document, ob, ib->data, ib->size, -1);
	t2 = clock();

	/* Cleanup */
	hoedown_buffer_free(ib);
	hoedown_document_free(document);
	renderer_free(renderer);

    void *output = malloc(ob->size);
	memcpy(output, ob->data, ob->size);
	*output_data = output;
	*output_size = ob->size;
	hoedown_buffer_free(ob);

	/* Show rendering time */
	if (data.show_time) {
		double elapsed;

		if (t1 == ((clock_t) -1) || t2 == ((clock_t) -1)) {
			fprintf(stderr, "Failed to get the time.\n");
			return 1;
		}

		elapsed = (double)(t2 - t1) / CLOCKS_PER_SEC;
		if (elapsed < 1)
			fprintf(stderr, "Time spent on rendering: %7.2f ms.\n", elapsed*1e3);
		else
			fprintf(stderr, "Time spent on rendering: %6.3f s.\n", elapsed);
	}

	return 0;
}
