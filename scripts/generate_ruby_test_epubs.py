#!/usr/bin/env python3
"""Generate horizontal/vertical ruby fixtures using only the standard library."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


OUTPUT = Path(__file__).resolve().parent.parent / "test" / "epubs"


def generate(vertical: bool) -> Path:
    direction = "vertical" if vertical else "horizontal"
    title = f"ルビ表示テスト ({direction})"
    mode = "vertical-rl" if vertical else "horizontal-tb"
    body = """
<h2>対応単位と中付き</h2>
<p>モノ：<ruby>漢<rt>かん</rt>字<rt>じ</rt></ruby>。
グループ：<ruby>今日<rt>きょう</rt></ruby>は晴れ。
別々のruby：<ruby>山<rt>やま</rt></ruby><ruby>川<rt>かわ</rt></ruby>。</p>
<p>明示rb：<ruby><rb>漢</rb><rt>かん</rt><rb>字</rb><rt>じ</rt></ruby>。
一括rb：<ruby><rb>今日</rb><rt>きょう</rt></ruby>。</p>
<p>rbc/rtc：<ruby><rbc><rb>漢</rb><rb>字</rb></rbc>
<rtc><rt>かん</rt><rt>じ</rt></rtc><rtc><rt rbspan="2">secondary-not-displayed</rt></rtc></ruby>。
rbspan：<ruby><rb>今</rb><rb>日</rb><rt rbspan="2">きょう</rt></ruby>。</p>
<p>rpは非表示：<ruby>漢<rp>（</rp><rt>かん</rt><rp>）</rp></ruby>。
空rt：<ruby>字<rt> </rt></ruby>。
縦中横：<ruby>12<rt>じゅうに</rt></ruby>。
横倒し：<ruby>Ruby<rt>ルビー</rt></ruby>。</p>
<h2>行頭・行末・短い行</h2>
<p><ruby>長<rt>ながいながいふりがな</rt></ruby>から始まり、
最後も<ruby>長<rt>とてもながいふりがな</rt></ruby></p>
<p><ruby>字<rt>ひとつのもじにながいよみ</rt></ruby></p>
<p><ruby>甲<rt>ながいよみがな</rt></ruby><ruby>乙<rt>ながいよみがな</rt></ruby><ruby>丙<rt>ながいよみがな</rt></ruby></p>
<p>小書き仮名：<ruby>客<rt>きゃく</rt></ruby>、<ruby>町<rt>ちょう</rt></ruby>、<ruby>音<rt>ミュージック</rt></ruby>。</p>
<h2>折り返しと禁則</h2>
"""
    for prefix in range(1, 30):
        body += f'<p>{"本文" * prefix}「<ruby>今日<rt>きょう</rt></ruby>」。続き。</p>\n'
    body += "<h2>中間flush境界</h2>\n<p>"
    body += "字" * 399 + '<ruby>漢<rt>かん</rt>字<rt>じ</rt></ruby>' + "続" * 40 + "</p>\n"
    body += "<p>" + '<ruby>今日<rt>きょう</rt></ruby>の本文。' * 100 + "</p>\n"
    body += "<h2>一行より長い親文字（安全策）</h2>\n"
    body += '<p><ruby>' + "親文字" * 30 + '<rt>グループ全体の読みは先頭の断片に一度だけ表示</rt></ruby>続き。</p>'
    body += "<h2>ルビなし</h2><p>ルビのない本文。" + "回帰確認。" * 100 + "</p>"

    xhtml = f'''<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja">
<head><title>{title}</title><link rel="stylesheet" href="style.css" type="text/css"/></head>
<body>{body}</body></html>'''
    opf = f'''<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="id">ruby-test-{direction}</dc:identifier><dc:title>{title}</dc:title><dc:language>ja</dc:language>
</metadata><manifest>
<item id="body" href="body.xhtml" media-type="application/xhtml+xml"/>
<item id="css" href="style.css" media-type="text/css"/>
<item id="nav" href="nav.xhtml" properties="nav" media-type="application/xhtml+xml"/>
</manifest><spine page-progression-direction="{'rtl' if vertical else 'ltr'}"><itemref idref="body"/></spine></package>'''
    nav = f'''<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>{title}</title></head><body><nav epub:type="toc"><ol><li><a href="body.xhtml">{title}</a></li></ol></nav></body></html>'''
    container = '''<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'''
    path = OUTPUT / f"ja_ruby_{direction}.epub"
    with ZipFile(path, "w", compression=ZIP_DEFLATED) as epub:
        epub.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        epub.writestr("META-INF/container.xml", container)
        epub.writestr("OEBPS/content.opf", opf)
        epub.writestr("OEBPS/body.xhtml", xhtml)
        epub.writestr("OEBPS/nav.xhtml", nav)
        epub.writestr("OEBPS/style.css", f"body {{ writing-mode: {mode}; }} p {{ text-indent: 0; }}")
    return path


if __name__ == "__main__":
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for is_vertical in (False, True):
        print(generate(is_vertical))
