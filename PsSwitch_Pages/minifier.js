const fs = require('fs');
const path = require('path');
const zlib = require('zlib'); // Módulo nativo para compressão Gzip
const { minify } = require('html-minifier-terser');

// Arquivos que devem estar na MESMA pasta deste script JS
const files = [
  'provisioning_page.html',
  'web_interface.html'
];

// Caminho de destino (components/webserver)
const destDir = path.resolve(__dirname, '../PsSwitch3_IDF/components/webserver');

async function processFiles() {
  console.log(`📍 Onde o script está rodando: ${__dirname}`);
  console.log(`📍 Destino configurado: ${destDir}\n`);

  // Garante que a pasta de destino existe
  if (!fs.existsSync(destDir)) {
    fs.mkdirSync(destDir, { recursive: true });
    console.log(`📁 Pasta criada: ${destDir}\n`);
  }

  for (const fileName of files) {
    const filePath = path.join(__dirname, fileName);

    if (!fs.existsSync(filePath)) {
      console.error(`❌ Arquivo NÃO encontrado: ${filePath}`);
      console.error(`👉 Verifique se o arquivo "${fileName}" está exatamente na pasta indicada acima.\n`);
      continue;
    }

    try {
      const content = fs.readFileSync(filePath, 'utf8');

      // 1. Minificação de HTML, JS e CSS inline
      const minified = await minify(content, {
        collapseWhitespace: true,
        removeComments: true,
        minifyJS: true,
        minifyCSS: true,
        removeRedundantAttributes: true
      });

      // 2. Compactação com Gzip
      const gzippedBuffer = zlib.gzipSync(minified);

      // 3. Define o nome final com a extensão .gz (ex: provisioning_page.html.gz)
      const outputFileName = `${fileName}.gz`;
      const outputPath = path.join(destDir, outputFileName);

      // 4. Salva o buffer compactado
      fs.writeFileSync(outputPath, gzippedBuffer);

      // Estatísticas do tamanho
      const originalSize = Buffer.byteLength(content, 'utf8');
      const minifiedSize = Buffer.byteLength(minified, 'utf8');
      const gzSize = gzippedBuffer.length;
      const reduction = (((originalSize - gzSize) / originalSize) * 100).toFixed(1);

      console.log(`✅ Processado com sucesso: ${fileName}`);
      console.log(`   - Original:  ${originalSize} bytes`);
      console.log(`   - Minificado: ${minifiedSize} bytes`);
      console.log(`   - Gzip (.gz): ${gzSize} bytes (${reduction}% menor)`);
      console.log(`   📁 Salvo em: ${outputPath}\n`);

    } catch (err) {
      console.error(`⚠️ Erro ao processar ${fileName}:`, err.message);
    }
  }
}

processFiles();