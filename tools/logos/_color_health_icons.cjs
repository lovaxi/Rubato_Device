// 彩色版健康图标预览：读取已缓存的候选 SVG → 注入活动主题色 → 色彩白墨化（srcRGB×alpha 贴黑底）
// 用法: node _color_health_icons.cjs   (在本目录运行；依赖 ../icons/*.svg 已由 _fetch_health_icons.cjs 缓存)
// 输出: ../icons/color/*.svg + ../icons/candidates_color.png
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');

const ICONS_DIR = path.join(__dirname, '..', 'icons');
const OUT_DIR = path.join(ICONS_DIR, 'color');
fs.mkdirSync(OUT_DIR, { recursive: true });

const CELL = 120, ICON = 96, COLS = 6;

// 活动主题色（低饱和、亮度相近，纯黑底友好；均为 RGB565 可精确近似的值）
const ACCENT = {
  water:  '#96C3F0', // 冰川蓝——水（与 Generating 蓝同族，语义自然）
  toilet: '#A8B0C0', // 冷灰——厕所（中性事务，不抢戏）
  eyes:   '#FFD8A8', // 奶油——眼睛（暖，柔和）
  neck:   '#C9B6E4', // 浅藕紫——肩颈（放松感）
};

(async () => {
  const files = fs.readdirSync(ICONS_DIR).filter(f => f.endsWith('.svg')).sort();
  const cells = [];
  const mapping = [];
  let idx = 0;
  for (const f of files) {
    const act = f.split('--')[1];
    const color = ACCENT[act] || '#FFFFFF';
    let svg = fs.readFileSync(path.join(ICONS_DIR, f), 'utf8');
    if (!svg.includes('currentColor')) { console.log(`SKIP ${f} (no currentColor)`); continue; }
    svg = svg.split('currentColor').join(color);
    const outFile = path.join(OUT_DIR, f);
    fs.writeFileSync(outFile, svg);
    const { data } = await sharp(Buffer.from(svg))
      .resize(ICON, ICON, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .ensureAlpha().raw().toBuffer({ resolveWithObject: true });
    // 色彩墨水：黑底合成 = srcRGB × alpha（设备端等价于 JPEG 直接画在黑底上）
    const out = Buffer.alloc(ICON * ICON * 4);
    for (let i = 0; i < ICON * ICON; i++) {
      const a = data[i * 4 + 3];
      out[i * 4]     = (data[i * 4]     * a) / 255;
      out[i * 4 + 1] = (data[i * 4 + 1] * a) / 255;
      out[i * 4 + 2] = (data[i * 4 + 2] * a) / 255;
      out[i * 4 + 3] = 255;
    }
    const png = await sharp(out, { raw: { width: ICON, height: ICON, channels: 4 } }).png().toBuffer();
    const row = Math.floor(idx / COLS), col = idx % COLS;
    cells.push({ input: png, left: col * CELL + (CELL - ICON) / 2, top: row * CELL + (CELL - ICON) / 2 });
    mapping.push({ idx, act, color, svg: f });
    console.log(`OK   #${String(idx).padStart(2, '0')} [${act}] ${color}  ${f}`);
    idx++;
  }
  const rows = Math.ceil(idx / COLS);
  await sharp({ create: { width: COLS * CELL, height: rows * CELL, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 1 } } })
    .composite(cells).png().toFile(path.join(ICONS_DIR, 'candidates_color.png'));
  fs.writeFileSync(path.join(OUT_DIR, 'mapping.json'), JSON.stringify(mapping, null, 2));
  console.log(`\nsheet: ${path.join(ICONS_DIR, 'candidates_color.png')}  (${idx} icons, ${COLS} per row)`);
})();
