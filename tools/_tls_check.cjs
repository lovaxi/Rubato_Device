// one-shot diagnostic: what does the EMQX server actually present, and does it verify against the pinned CA?
const tls = require('tls');
const fs = require('fs');
const path = require('path');

const host = 'ubaa35f0.ala.cn-shenzhen.emqxsl.cn';
const ca = fs.readFileSync(path.join(__dirname, '..', 'certs', 'emqxsl-ca.crt'));

const s = tls.connect({ host, port: 8883, ca: [ca], servername: host, rejectUnauthorized: true }, () => {
  console.log('PC-SIDE VERIFY:', s.authorized ? 'PASSED' : 'FAILED: ' + s.authorizationError);
  const chain = s.getPeerCertificate(true);
  let c = chain, i = 0;
  while (c && c.subject && i < 5) {
    console.log(`  cert[${i}] CN="${c.subject.CN}" issuer="${c.issuer && c.issuer.CN}" valid ${c.valid_from} -> ${c.valid_to}`);
    if (!c.issuerCertificate || c.issuerCertificate.fingerprint === c.fingerprint) break;
    c = c.issuerCertificate; i++;
  }
  s.end();
  setTimeout(() => process.exit(0), 300);
});
s.on('error', e => { console.log('ERR:', e.message); process.exit(1); });
