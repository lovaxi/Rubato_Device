// 健康提醒图标候选：从 Iconify API 拉取线性图标 → 白墨化 → 拼候选总览图
// 用法: node _fetch_health_icons.cjs   (在本目录运行，复用 node_modules/sharp)
// 输出: ../icons/*.svg + ../icons/candidates.png + candidates.json
// 许可: lucide=ISC(MIT同) / tabler=MIT / material-symbols=Apache-2.0，均可免费商用
const fs = require('fs');
const path = require('path');
const https = require('https');
const sharp = require('sharp');

const OUT = path.join(__dirname, '..', 'icons');
fs.mkdirSync(OUT, { recursive: true });

const CELL = 120, ICON = 96, COLS = 6;

const CANDS = [
  ['water',  'lucide:glass-water'],
  ['water',  'lucide:cup-soda'],
  ['water',  'tabler:glass'],
  ['water',  'material-symbols:local-drink'],
  ['water',  'material-symbols:water-drop'],
  ['toilet', 'material-symbols:wc'],
  ['toilet', 'material-symbols:restroom'],
  ['toilet', 'tabler:toilet-paper'],
  ['toilet', 'lucide:bath'],
  ['eyes',   'lucide:eye'],
  ['eyes',   'tabler:eye'],
  ['eyes',   'material-symbols:visibility'],
  ['neck',   'lucide:accessibility'],
  ['neck',   'lucide:person-standing'],
  ['neck',   'tabler:stretching'],
  ['neck',   'material-symbols:self-improvement'],
  ['neck',   'material-symbols:accessibility-new'],
];

function fetchUrl(url) {
  return new Promise((resolve, reject) => {
    https.get(url, { headers: { 'User-Agent': 'Mozilla/5.0' } }, res => {
      if (res.statusCode !== 200) { res.resume(); return reject(new Error('HTTP ' + res.statusCode)); }
      const chunks = [];
      res.on('data', c => chunks.push(c));
      res.on('end', () => resolve(Buffer.concat(chunks)));
    }).on('error', reject);
  });
}

(async () => {
  const cells = [];
  const mapping = [];
  let idx = 0;
  for (const [act, id] of CANDS) {
    const [prefix, name] = id.split(':');
    const url = `https://api.iconify.design/${prefix}/${name}.svg?height=${ICON}`;
    try {
      const svg = await fetchUrl(url);
      const svgFile = path.join(OUT, `${String(idx).padStart(2, '0')}--${act}--${prefix}-${name}.svg`);
      fs.writeFileSync(svgFile, svg);
      const { data } = await sharp(svg)
        .resize(ICON, ICON, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
        .ensureAlpha().raw().toBuffer({ resolveWithObject: true });
      // alpha-as-ink：与 logo 管线同法，取 alpha 通道作白墨（源是 currentColor 黑也无妨）
      const out = Buffer.alloc(ICON * ICON * 4);
      for (let i = 0; i < ICON * ICON; i++) {
        out[i * 4] = 255; out[i * 4 + 1] = 255; out[i * 4 + 2] = 255; out[i * 4 + 3] = data[i * 4 + 3];
      }
      const png = await sharp(out, { raw: { width: ICON, height: ICON, channels: 4 } }).png().toBuffer();
      const row = Math.floor(idx / COLS), col = idx % COLS;
      cells.push({ input: png, left: col * CELL + (CELL - ICON) / 2, top: row * CELL + (CELL - ICON) / 2 });
      mapping.push({ idx, act, id, svg: path.basename(svgFile) });
      console.log(`OK   #${idx} [${act}] ${id}`);
      idx++;
    } catch (e) {
      console.log(`MISS #${String(idx).padStart(2, '0')} [${act}] ${id} -> ${e.message}`);
    }
  }
  const rows = Math.ceil(idx / COLS);
  await sharp({ create: { width: COLS * CELL, height: rows * CELL, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 1 } } })
    .composite(cells).png().toFile(path.join(OUT, 'candidates.png'));
  fs.writeFileSync(path.join(OUT, 'candidates.json'), JSON.stringify(mapping, null, 2));
  console.log(`\nsheet: ${path.join(OUT, 'candidates.png')}  (${idx} icons, ${COLS} per row, idx 按行左->右)`);
})();
