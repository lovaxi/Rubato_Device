// Social preview card: 1280x640 (GitHub's recommended size) - black bg, product photo right,
// wordmark + tagline + breathing-orb dot left. Output feeds the manual upload:
// Repo Settings -> General -> Social preview. Run: node _gen_social_preview.cjs
const path = require('path');
const sharp = require('sharp');

(async () => {
  const photo = await sharp(path.join(__dirname, '..', '..', 'assets', 'product.jpg'))
    .resize(660, 640, { fit: 'cover', position: 'attention' })
    .png()
    .toBuffer();
  // left-to-right black fade over the photo edge so the card reads as one piece
  const fade = Buffer.from(`<svg width="1280" height="640">
    <defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#000000"/><stop offset="0.42" stop-color="#000000"/>
      <stop offset="0.56" stop-color="#000000" stop-opacity="0"/>
    </linearGradient></defs>
    <rect width="1280" height="640" fill="url(#g)"/>
    <circle cx="86" cy="150" r="17" fill="#FFD8A8"/>
    <circle cx="86" cy="150" r="30" fill="#FFD8A8" fill-opacity="0.25"/>
    <text x="66" y="300" fill="#F5F2EC" font-family="sans-serif" font-weight="bold" font-size="118">Rubato</text>
    <text x="68" y="368" fill="#D9D9D9" font-family="sans-serif" font-size="38">When AI thinks, you move.</text>
    <text x="68" y="430" fill="#ADADAD" font-family="sans-serif" font-size="30">Health breaks for programmers &amp; heavy AI users.</text>
  </svg>`);
  await sharp({ create: { width: 1280, height: 640, channels: 3, background: { r: 0, g: 0, b: 0 } } })
    .composite([{ input: photo, left: 620, top: 0 }, { input: fade, left: 0, top: 0 }])
    .png()
    .toFile(path.join(__dirname, '..', '..', 'assets', 'social-preview.png'));
  console.log('social preview: ' + path.join(__dirname, '..', '..', 'assets', 'social-preview.png'));
})();
