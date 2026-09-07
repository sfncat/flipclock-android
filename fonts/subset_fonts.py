# -*- coding: utf-8 -*-
"""
字体子集化压缩脚本（pyftsubset 封装）

用途：把源字体目录下的大体积中文字体按"实际会用到的字符集"
裁剪子集，大幅减小 APK 体积。脚本可重复使用（以后数据变化后重跑即可）。

两种模式：
  1. --charset-file sub.txt   —— 直接用指定字符集文件裁剪（推荐，精确控制）
  2. 不传 --charset-file       —— 自动构建 GB2312+Big5+假名+标点+数据字符全集

用法示例：
  # 用 sub.txt 精确裁剪，源字体在 fonts/，输出到 assets/fonts/
  python fonts/subset_fonts.py --charset-file fonts/sub.txt --fonts-dir fonts --out-dir app/src/main/assets/fonts

  # 自动构建大字符集（GB2312+Big5 等）
  python fonts/subset_fonts.py --fonts-dir fonts

  # 直接替换 assets/fonts 原文件
  python fonts/subset_fonts.py --charset-file fonts/sub.txt --fonts-dir fonts --replace
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
FONTS_DIR = PROJECT_ROOT / 'app' / 'src' / 'main' / 'assets' / 'fonts'
DEFAULT_DATA_FILE = PROJECT_ROOT / 'test' / 'output_1.jsonl'
DEFAULT_OUT_DIR = Path(__file__).resolve().parent / 'subset_out'
DEFAULT_CHARSET_FILE = Path(__file__).resolve().parent / 'sub.txt'

# 常用中文标点/符号区段（CJK标点 + 全角形式 + 常用排版符号）
PUNCT_RANGES = [
    (0x3000, 0x303F),   # CJK 符号和标点（、。〈〉《》等）
    (0xFF00, 0xFFEF),   # 全角/半角形式（！？（），：；等）
    (0xFE30, 0xFE4F),   # CJK 兼容形式
]
EXTRA_PUNCT = '""''—…·～℃№①②③④⑤⑥⑦⑧⑨⑩✓★'
KANA_RANGE = (0x3040, 0x30FF)  # 平假名 + 片假名


def gb2312_chars():
    """枚举 GB2312 收录的全部汉字/符号"""
    chars = set()
    for hi in range(0xA1, 0xF8):
        for lo in range(0xA1, 0xFF):
            try:
                chars.add(bytes([hi, lo]).decode('gb2312'))
            except UnicodeDecodeError:
                continue
    return chars


def big5_common_chars():
    """枚举 Big5 常用字区（0xA440~0xC67E，5401 个繁体常用字）"""
    chars = set()
    for hi in range(0xA4, 0xC7):
        end_lo = 0x7F if hi == 0xC6 else 0xFF  # 0xC6 行到 0xC67E 截止
        for lo in list(range(0x40, 0x7F)) + (list(range(0xA1, end_lo)) if hi != 0xC6 else list(range(0xA1, 0x7F))):
            try:
                ch = bytes([hi, lo]).decode('big5')
                if '\u4e00' <= ch <= '\u9fff':
                    chars.add(ch)
            except UnicodeDecodeError:
                continue
    return chars


def extract_data_chars(data_file: Path):
    """从数据文件（JSONL/任意文本）中提取出现过的所有非 ASCII 字符"""
    chars = set()
    if not data_file.exists():
        print(f"[WARN] 数据文件不存在，跳过: {data_file}")
        return chars
    with open(data_file, 'r', encoding='utf-8') as f:
        for line in f:
            for ch in line:
                if ord(ch) > 0x7F:
                    chars.add(ch)
    return chars


def build_charset(data_file: Path) -> set:
    charset = set(chr(c) for c in range(0x20, 0x7F))  # ASCII 可打印
    for start, end in PUNCT_RANGES:
        charset.update(chr(c) for c in range(start, end + 1))
    charset.update(EXTRA_PUNCT)
    gb = gb2312_chars()
    charset.update(gb)
    charset.update(chr(c) for c in range(KANA_RANGE[0], KANA_RANGE[1] + 1))
    big5 = big5_common_chars()
    charset.update(big5)
    data_chars = extract_data_chars(data_file)
    charset.update(data_chars)
    charset.discard('\n')
    charset.discard(' ')
    print(f"[CHARSET] ASCII+标点+GB2312({len(gb)}字)+假名+Big5繁体({len(big5)}字)+数据字符，合计 {len(charset)} 个码位")
    return charset


def subset_font(font: Path, out_path: Path, charset_file: Path) -> bool:
    """调用 pyftsubset 裁剪单个字体；.ttc 字体集合取第 0 号字体裁剪为单字体，
    输出文件名后缀按内容格式修正为 .ttf/.otf（Android FreeType 可正常加载）"""
    cmd = [
        sys.executable, '-m', 'fontTools.subset', str(font),
        f'--text-file={charset_file}',
        f'--output-file={out_path}',
        '--no-hinting',          # 去掉 hinting 指令，屏幕显示无影响
        '--desubroutinize',      # CFF 字体（OTF）必须，否则裁剪效果差
        '--notdef-glyph',        # 保留 .notdef（缺字时有兜底字形）
        '--name-IDs=*',          # 保留字体名称信息
        '--layout-features=*',   # 保留全部 OpenType 特性
    ]
    if font.suffix.lower() == '.ttc':
        cmd.append('--font-number=0')
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[ERROR] {font.name}: {result.stderr[:300]}")
        return False
    return True


def main():
    parser = argparse.ArgumentParser(description='字体子集化压缩')
    parser.add_argument('--charset-file', type=Path, default=DEFAULT_CHARSET_FILE,
                        help='字符集文件，每行/每字即为保留字符（默认 fonts/sub.txt）。'
                             '提供时跳过 GB2312+Big5 全集构建，仅保留文件中出现的字符')
    parser.add_argument('--data-file', type=Path, default=DEFAULT_DATA_FILE,
                        help='提取字符的数据文件（仅在不使用 --charset-file 时生效）')
    parser.add_argument('--fonts-dir', type=Path, default=FONTS_DIR,
                        help='源字体目录（默认 assets/fonts，建议指向完整原字体目录）')
    parser.add_argument('--out-dir', type=Path, default=DEFAULT_OUT_DIR,
                        help='子集字体输出目录')
    parser.add_argument('--replace', action='store_true',
                        help='直接替换 assets/fonts 原文件（完整原字体请自行保留在其他目录）')
    args = parser.parse_args()

    fonts_dir = args.fonts_dir.resolve()
    if not fonts_dir.is_dir():
        print(f"[ERROR] 字体目录不存在: {fonts_dir}")
        sys.exit(1)
    if fonts_dir == args.out_dir.resolve():
        print("[ERROR] 源字体目录与输出目录相同，会覆盖原字体，请更换 --out-dir")
        sys.exit(1)

    fonts = sorted(p for p in fonts_dir.iterdir()
                   if p.suffix.lower() in ('.ttf', '.otf', '.ttc') and not p.name.endswith('.bak'))
    if not fonts:
        print(f"[ERROR] 字体目录无字体文件: {fonts_dir}")
        sys.exit(1)
    print(f"[INFO] 源目录 {fonts_dir}，发现 {len(fonts)} 款字体")

    args.out_dir.mkdir(parents=True, exist_ok=True)

    # ---- 字符集构建 ----
    charset_file = args.charset_file.resolve()
    if charset_file.exists():
        # 模式1：直接使用字符集文件（精确控制，仅保留文件中出现的字符）
        with open(charset_file, 'r', encoding='utf-8') as f:
            charset = set(f.read())
        charset.discard('\n')
        charset.discard('\r')
        print(f"[CHARSET] 从 {charset_file} 读取，共 {len(charset)} 个唯一字符")
        # 将清理后的字符集写到输出目录，供 pyftsubset --text-file 使用
        tmp_charset = args.out_dir / '_subset_chars.txt'
        tmp_charset.write_text(''.join(sorted(charset)), encoding='utf-8')
        charset_file = tmp_charset
    else:
        # 模式2：自动构建 GB2312+Big5+假名+标点+数据字符全集
        charset = build_charset(args.data_file)
        tmp_charset = args.out_dir / '_subset_chars.txt'
        tmp_charset.write_text(''.join(sorted(charset)), encoding='utf-8')
        charset_file = tmp_charset

    total_before = total_after = 0
    for font in fonts:
        out_path = args.out_dir / font.name
        ok = subset_font(font, out_path, charset_file)
        if not ok:
            continue
        if font.suffix.lower() == '.ttc':
            # pyftsubset 输出的是单字体，后缀按内容格式修正（OTTO→.otf，否则 .ttf）
            suffix = '.otf' if out_path.read_bytes()[:4] == b'OTTO' else '.ttf'
            fixed = out_path.with_suffix(suffix)
            out_path.rename(fixed)
            out_path = fixed
        before = font.stat().st_size
        after = out_path.stat().st_size
        total_before += before
        total_after += after
        if args.replace:
            target = FONTS_DIR / out_path.name
            shutil.copy2(out_path, target)
        print(f"  {font.name:<38s} {before/1048576:7.2f} MB -> {after/1048576:7.2f} MB "
              f"(-{(1-after/before)*100:.0f}%)")

    print(f"\n[TOTAL] {total_before/1048576:.1f} MB -> {total_after/1048576:.1f} MB "
          f"(节省 {(1-total_after/total_before)*100:.0f}%)")
    if args.replace:
        print("[INFO] 已替换 assets/fonts 中的字体文件")
    else:
        print(f"[INFO] 子集字体在 {args.out_dir}，确认无误后用 --replace 一键替换")


if __name__ == '__main__':
    main()
