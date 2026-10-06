"""Rebuild the tiny XLSX reader fixtures with Python's stdlib, independently of OpenXLSX."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
REL = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
PKG = "http://schemas.openxmlformats.org/package/2006/relationships"
ROOT = Path(__file__).parent


def sheet(rows):
    return f'<worksheet xmlns="{NS}"><sheetData>{rows}</sheetData></worksheet>'


def text(ref, value):
    return f'<c r="{ref}" t="inlineStr"><is><t xml:space="preserve">{value}</t></is></c>'


def row(number, cells):
    return f'<row r="{number}">{cells}</row>'


headers = ("name", "number", "boolean", "date", "formula", "note", "error", "large")
data = sheet(
    row(1, "".join(text(f"{chr(65 + i)}1", name) for i, name in enumerate(headers)))
    + row(
        2,
        '<c r="A2" t="s"><v>0</v></c><c r="B2"><v>1.25</v></c>'
        '<c r="C2" t="b"><v>1</v></c><c r="D2" s="1"><v>45292.5</v></c>'
        '<c r="E2"><f>B2*2</f><v>2.5</v></c>'
        '<c r="F2" t="inlineStr"><is><r><t xml:space="preserve"> Olá </t></r>'
        '<r><t>🌍\nnext</t></r></is></c><c r="G2" t="e"><v>#DIV/0!</v></c>'
        '<c r="H2"><v>9007199254740993</v></c>',
    )
    + row(3, text("A3", ""))
    + row(
        4,
        '<c r="A4" t="s"><v>1</v></c><c r="B4"><v>-1.25E+03</v></c>'
        '<c r="C4" t="b"><v>0</v></c><c r="D4" s="1"><v>60</v></c>'
        '<c r="E4" t="str"><f>IF(1, &quot;&quot;)</f><v/></c>'
        + text("F4", "0012"),
    )
    + row(1_048_576, '<c r="XFD1048576" s="1"><v/></c>')
)
second = sheet(row(1, text("A1", "key") + text("B1", "value")) + row(2, text("A2", "other") + text("B2", "42")))
preamble = sheet(
    row(1, text("A1", "Title"))
    + row(2, text("A2", ""))
    + row(3, text("A3", "name") + text("C3", "value"))
    + row(4, text("A4", "first") + '<c r="C4"><v>10</v></c>')
    + row(6, text("A6", "last"))
)
missing = sheet(
    row(1, text("A1", "value"))
    + row(2, '<c r="A2"><f>1+0</f><v>1</v></c>')
    + row(3, '<c r="A3"><f>1+1</f><v/></c>')
)
absent = sheet(row(1, text("A1", "value")) + row(2, '<c r="A2" t="str"><f>1+1</f></c>'))
sheets = [
    ("Data", "worksheets/sheet1.xml", data),
    ("Chart", "chartsheets/sheet1.xml", f'<chartsheet xmlns="{NS}"/>'),
    ("Second", "worksheets/sheet2.xml", second),
    ("Preamble", "worksheets/sheet3.xml", preamble),
    ("Empty", "worksheets/sheet4.xml", sheet("")),
    ("HeaderOnly", "worksheets/sheet5.xml", sheet(row(1, text("A1", "name")))),
    ("MissingCache", "worksheets/sheet6.xml", missing),
    ("AbsentCache", "worksheets/sheet7.xml", absent),
]
entries = {
    "_rels/.rels": f'<Relationships xmlns="{PKG}"><Relationship Id="rId1" Type="{REL}/officeDocument" Target="xl/workbook.xml"/></Relationships>',
    "xl/workbook.xml": f'<workbook xmlns="{NS}" xmlns:r="{REL}"><sheets>'
    + "".join(f'<sheet name="{name}" sheetId="{i}" r:id="rId{i}"/>' for i, (name, _, _) in enumerate(sheets, 1))
    + "</sheets></workbook>",
    "xl/_rels/workbook.xml.rels": f'<Relationships xmlns="{PKG}">'
    + "".join(
        f'<Relationship Id="rId{i}" Type="{REL}/{"chartsheet" if path.startswith("chartsheets") else "worksheet"}" Target="{path}"/>'
        for i, (_, path, _) in enumerate(sheets, 1)
    )
    + f'<Relationship Id="rId9" Type="{REL}/sharedStrings" Target="sharedStrings.xml"/>'
    + f'<Relationship Id="rId10" Type="{REL}/styles" Target="styles.xml"/></Relationships>',
    "xl/sharedStrings.xml": f'<sst xmlns="{NS}"><si><r><t>São </t></r><r><t>Paulo</t></r></si><si><t>Beta</t></si></sst>',
    "xl/styles.xml": f'<styleSheet xmlns="{NS}"><fonts count="1"><font/></fonts><fills count="1"><fill/></fills>'
    '<borders count="1"><border/></borders><cellStyleXfs count="1"><xf numFmtId="0"/></cellStyleXfs>'
    '<cellXfs count="2"><xf numFmtId="0"/><xf numFmtId="14" applyNumberFormat="1"/></cellXfs></styleSheet>',
}
entries.update({f"xl/{path}": content for _, path, content in sheets})
entries["[Content_Types].xml"] = (
    '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
    '<Default Extension="xml" ContentType="application/xml"/>'
    '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
    '<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>'
    '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
    + "".join(
        f'<Override PartName="/xl/{path}" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.{"chartsheet" if path.startswith("chartsheets") else "worksheet"}+xml"/>'
        for _, path, _ in sheets
    )
    + "</Types>"
)


def write_book(name, parts):
    with ZipFile(ROOT / name, "w") as archive:
        for path, content in sorted(parts.items()):
            entry = ZipInfo(path, (2026, 1, 1, 0, 0, 0))
            entry.compress_type = ZIP_DEFLATED
            archive.writestr(entry, content.encode("utf-8"))


if __name__ == "__main__":
    write_book("xlsx_read.xlsx", entries)
    write_book("xlsx_bad_sheet.xlsx", entries | {"xl/worksheets/sheet1.xml": data[:-12]})
    write_book("xlsx_bad_workbook.xlsx", entries | {"xl/workbook.xml": entries["xl/workbook.xml"][:-11]})
