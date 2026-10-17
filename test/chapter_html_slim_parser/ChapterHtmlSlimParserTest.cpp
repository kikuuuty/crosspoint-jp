#include <Arduino.h>
#include <GfxRenderer.h>

#include <cstdio>
#include <cstdlib>

#include "Epub.h"
#include "Epub/Page.h"
#include "Epub/parsers/ChapterHtmlSlimParser.h"

namespace {
void check(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

struct Result {
  std::string text;
  std::vector<std::unique_ptr<Page>> pages;
  int extracts = 0;
  std::vector<std::pair<std::string, uint16_t>> anchors;
};

Result parse(const std::string& body, bool cssEnabled = true, bool vertical = false) {
  Storage = {};
  testDelays.clear();
  const std::string path = "/chapter.xhtml";
  Storage.put(path, "<html xmlns:epub=\"http://www.idpf.org/2007/ops\"><body>" + body + "</body></html>");
  auto epub = std::make_shared<Epub>();
  GfxRenderer renderer;
  CssParser css("/cache/");
  Storage.put("/style.css", ".show { display: block; } .hide { display: none; }");
  FsFile cssFile;
  check(Storage.openFileForRead("TEST", "/style.css", cssFile) && css.loadFromStream(cssFile), "load real CSS");
  Result result;
  ChapterHtmlSlimParser parser(
      epub, path, renderer, 1, 1.0f, false, 1, 480, 800, false, false,
      [&](std::unique_ptr<Page> page) {
        if (page) result.pages.push_back(std::move(page));
      },
      true, "", "/images/", 0, nullptr, cssEnabled ? &css : nullptr, nullptr, 0, vertical);
  check(parser.parseAndBuildPages(), "parse valid XHTML with production Expat callbacks");
  check(parser.depth == 0 && parser.skipUntilDepth == INT_MAX && parser.listDepth == 0 && parser.tableDepth == 0,
        "all depth/skip/list/table state must be balanced after parsing");
  for (const auto& page : result.pages) {
    for (const auto& element : page->elements) {
      if (element->getTag() == TAG_PageLine) {
        const auto& block = static_cast<const PageLine&>(*element).getBlock();
        for (const auto& word : block->getWords()) result.text += word;
      } else if (element->getTag() == TAG_PageTableRow) {
        const auto& block = static_cast<const PageTableRow&>(*element).block;
        for (const auto& cell : block->cellLines) {
          for (const auto& line : cell) result.text += line;
        }
      }
    }
  }
  // Ignore layout's paragraph indentation when comparing visible text.
  for (size_t pos; (pos = result.text.find("\xe2\x80\x83")) != std::string::npos;) result.text.erase(pos, 3);
  result.extracts = epub->extracts;
  result.anchors = parser.getAnchors();
  return result;
}

void hiddenElements() {
  for (bool vertical : {false, true}) {
    for (bool cssEnabled : {false, true}) {
      for (const char* tag : {"p", "h1", "span", "div", "pre", "ruby", "table", "ol"}) {
        for (const char* value : {"", "hidden", "false", "until-found"}) {
          const std::string hidden = "<" + std::string(tag) + " hidden=\"" + value +
                                     "\" class=\"show\" "
                                     "style=\"display:block\"><b>HIDDEN</b></" +
                                     tag + ">";
          const auto result = parse("<p>Before</p>" + hidden + "<p>After</p>", cssEnabled, vertical);
          if (result.text != "BeforeAfter") {
            std::fprintf(stderr, "tag=%s value=%s css=%d vertical=%d text=[%s]\n", tag, value, cssEnabled, vertical,
                         result.text.c_str());
          }
          check(result.text == "BeforeAfter", "hidden presence must override CSS and suppress descendants");
        }
      }
      const auto inlineResult = parse("<p>前<span hidden=\"\"><b>隠す</b></span>後</p>", cssEnabled, vertical);
      check(inlineResult.text == "前後", "visible Japanese siblings must survive inline hidden content");
      const auto images =
          parse("<p>Before<img hidden=\"hidden\" src=\"a.png\" alt=\"SECRET\"/>After</p>", cssEnabled, vertical);
      check(images.text == "BeforeAfter" && images.extracts == 0,
            "hidden images must not extract or emit placeholders");
    }
  }
  check(parse("<p class=\"hide\">HIDDEN</p><p>Visible</p>").text == "Visible", "existing CSS hiding must still work");
  check(parse("<p class=\"show\">Visible</p>").text == "Visible", "ordinary visible elements remain visible");
  check(parse("<p data-hidden=\"hidden\">Visible</p>").text == "Visible", "data-hidden is not the hidden attribute");
  const auto anchors = parse("<div id=\"visible\"><span hidden=\"\" id=\"secret\">HIDDEN</span>Visible</div>").anchors;
  check(anchors.size() == 1 && anchors[0].first == "visible", "hidden anchors must not replace visible metadata");
  check(parse("<table><tr><td>Before<span hidden=\"\">SECRET</span>After</td></tr>"
              "<tr hidden=\"hidden\"><td>HIDDEN_ROW</td></tr><tr><td hidden=\"hidden\">HIDDEN_CELL</td>"
              "<td>Visible</td></tr></table><p>Tail</p>")
                .text == "BeforeAfterVisibleTail",
        "hidden rows/cells and inline descendants must not enter table buffering");
  check(parse("<table><tr><td>A<table hidden=\"\"><tr><td>SECRET</td></tr></table>B</td></tr></table>"
              "<p>Tail</p>")
                .text == "ABTail",
        "a hidden nested table must not close the visible outer table");
  const auto lists = parse("<ol><li>A</li><li hidden=\"\">SECRET<ol><li>HIDDEN</li></ol></li><li>B</li></ol>");
  check(lists.text == parse("<ol><li>A</li><li>B</li></ol>").text,
        "hidden list items must not consume visible numbering or corrupt list nesting");
  for (bool vertical : {false, true}) {
    const auto ruby = parse("<p><ruby>日<span hidden=\"\">隠</span><rt>にち</rt></ruby>後</p>", true, vertical);
    check(ruby.text == "日後", "hidden ruby descendants must not enter the base or normal text");
    bool foundRuby = false;
    for (const auto& page : ruby.pages) {
      for (const auto& element : page->elements) {
        if (element->getTag() != TAG_PageLine) continue;
        const auto& spans = static_cast<const PageLine&>(*element).getBlock()->getRubySpans();
        for (const auto& span : spans) {
          if (span.text == "にち" && span.count == 1) foundRuby = true;
        }
      }
    }
    check(foundRuby, "visible Japanese ruby association must survive");
  }
}
}  // namespace

int main() {
  hiddenElements();
  std::puts("Chapter parser tests passed (hidden presence/CSS, nested content, Japanese, images, balanced state).");
}
