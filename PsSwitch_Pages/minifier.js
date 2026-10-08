const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { minify } = require('html-minifier-terser');

const files = [
  'provisioning_page.html',
  'web_interface.html',
  'web_interface_wol.html'
];

// Pasta recebida pelo Python
const destDir = process.argv[2]
  ? path.resolve(process.argv[2])
  : path.resolve(__dirname, 'temp');

async function processFiles() {
  console.log(`📍 Script: ${__dirname}`);
  console.log(`📍 Destino: ${destDir}\n`);

  if (!fs.existsSync(destDir)) {
    fs.mkdirSync(destDir, { recursive: true });
    console.log(`📁 Pasta criada: ${destDir}\n`);
  }

  for (const fileName of files) {
    const filePath = path.join(__dirname, fileName);

    if (!fs.existsSync(filePath)) {
      console.error(`❌ Arquivo NÃO encontrado: ${filePath}`);
      continue;
    }

    try {
      const content = fs.readFileSync(filePath, 'utf8');

      // 1. Minificação
      const minified = await minify(content, {
        collapseWhitespace: true,
        removeComments: true,
        minifyJS: true,
        minifyCSS: true,
        removeRedundantAttributes: true
      });

      // 2. Gzip
      const gzippedBuffer = zlib.gzipSync(minified, {
        level: 9
      });

      // 3. Nome do arquivo
      const outputFileName = `${fileName}.gz`;
      const outputPath = path.join(destDir, outputFileName);

      // 4. Salva
      fs.writeFileSync(outputPath, gzippedBuffer);

      const originalSize = Buffer.byteLength(content, 'utf8');
      const minifiedSize = Buffer.byteLength(minified, 'utf8');
      const gzSize = gzippedBuffer.length;

      const reduction = (
        ((originalSize - gzSize) / originalSize) * 100
      ).toFixed(1);

      console.log(`✅ ${fileName}`);
      console.log(`   Original:   ${originalSize} bytes`);
      console.log(`   Minificado: ${minifiedSize} bytes`);
      console.log(`   Gzip:       ${gzSize} bytes`);
      console.log(`   Redução:    ${reduction}%`);
      console.log(`   📁 ${outputPath}\n`);

    } catch (err) {
      console.error(`⚠️ Erro ao processar ${fileName}:`);
      console.error(err.message);
      process.exitCode = 1;
    }
  }
}

processFiles();