"""Makes manuscript.docx and manuscript.odt, standard manuscript format (Shunn), from Pandoc's
own reference documents:

    pandoc -o default.docx --print-default-data-file reference.docx
    pandoc -o default.odt --print-default-data-file reference.odt
    python3 manuscript.py <folder with the defaults> data/export
"""
import re
import sys
import zipfile

def rewrite(src, dst, changes):
    with zipfile.ZipFile(src) as zin, zipfile.ZipFile(dst, 'w') as zout:
        for item in zin.infolist():
            data = zin.read(item.filename)
            if item.filename in changes:
                data = changes[item.filename](data.decode()).encode()
            # ODF needs its mimetype stored first and uncompressed.
            kind = zipfile.ZIP_STORED if item.filename == 'mimetype' else zipfile.ZIP_DEFLATED
            zout.writestr(item.filename, data, compress_type=kind)

COURIER = '<w:rFonts w:ascii="Courier New" w:hAnsi="Courier New" w:eastAsia="Courier New" w:cs="Courier New" />'

def docx_styles(s):
    s = re.sub(r'<w:rFonts [^>]*/>', COURIER, s, flags=re.S)
    s = re.sub(r'<w:color [^>]*/>', '', s, flags=re.S)
    s = re.sub(r'<w:sz w:val="\d+" />', '<w:sz w:val="24" />', s)
    s = re.sub(r'<w:szCs w:val="\d+" />', '<w:szCs w:val="24" />', s)
    s = re.sub(r'<w:b />|<w:bCs />|<w:i />|<w:iCs />', '', s)
    s = s.replace('<w:spacing w:after="200" />',
                  '<w:spacing w:after="0" w:line="480" w:lineRule="auto" />')
    def body(m):
        block = re.sub(r'<w:pPr>.*?</w:pPr>', '', m.group(0), flags=re.S)
        return block.replace('</w:style>',
                             '<w:pPr><w:spacing w:before="0" w:after="0" w:line="480" '
                             'w:lineRule="auto" /><w:ind w:firstLine="720" /></w:pPr></w:style>')
    s = re.sub(r'<w:style [^>]*w:styleId="BodyText".*?</w:style>', body, s, flags=re.S)
    def plain(m):
        block = re.sub(r'<w:spacing [^>]*/>', '<w:spacing w:before="0" w:after="0" w:line="480" w:lineRule="auto" />', m.group(0))
        return block if '<w:jc ' in block else block.replace('<w:pPr>', '<w:pPr><w:jc w:val="center" />', 1)
    s = re.sub(r'<w:style [^>]*w:styleId="(Heading\d|Title)".*?</w:style>', plain, s, flags=re.S)
    return s

def docx_document(s):
    return s.replace('<w:sectPr>', '<w:sectPr><w:pgSz w:w="12240" w:h="15840" />'
                     '<w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440" '
                     'w:header="720" w:footer="720" w:gutter="0" />', 1)

def odt_styles(s):
    s = re.sub(r'style:font-name="[^"]*"', 'style:font-name="Courier New"', s)
    s = re.sub(r'fo:font-size="[^"]*"', 'fo:font-size="12pt"', s)
    s = re.sub(r'fo:font-weight="bold"', 'fo:font-weight="normal"', s)
    s = s.replace('<style:paragraph-properties fo:hyphenation-ladder-count="no-limit"',
                  '<style:paragraph-properties fo:line-height="200%" fo:hyphenation-ladder-count="no-limit"', 1)
    s = re.sub(r'(<style:style style:name="Text_20_body".*?<style:paragraph-properties) fo:margin-top="[^"]*"\s*fo:margin-bottom="[^"]*"',
               r'\1 fo:margin-top="0in" fo:margin-bottom="0in" fo:text-indent="0.5in"', s, flags=re.S)
    return s

rewrite(sys.argv[1] + '/default.docx', sys.argv[2] + '/manuscript.docx',
        {'word/styles.xml': docx_styles, 'word/document.xml': docx_document})
rewrite(sys.argv[1] + '/default.odt', sys.argv[2] + '/manuscript.odt',
        {'styles.xml': odt_styles})
