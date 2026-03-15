'use strict';

const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const https = require('https');

const LIBGIT2_VERSION = '1.9.0';
const LIBGIT2_TARBALL = `https://github.com/libgit2/libgit2/archive/refs/tags/v${LIBGIT2_VERSION}.tar.gz`;

const rootDir = path.join(__dirname, '..');
const libgit2Dir = path.join(rootDir, 'deps', 'libgit2');

// Already present (git submodule or previous download)
if (fs.existsSync(path.join(libgit2Dir, 'CMakeLists.txt'))) {
  console.log(`libgit2 v${LIBGIT2_VERSION} already present.`);
  process.exit(0);
}

// Try git submodule first (works in cloned repos)
if (fs.existsSync(path.join(rootDir, '.git'))) {
  console.log('Initializing libgit2 submodule...');
  try {
    execSync('git submodule update --init --recursive', {
      cwd: rootDir,
      stdio: 'inherit',
    });
    if (fs.existsSync(path.join(libgit2Dir, 'CMakeLists.txt'))) {
      console.log('libgit2 submodule initialized.');
      process.exit(0);
    }
  } catch {
    console.log('git submodule failed, falling back to tarball download...');
  }
}

// Download tarball (works in npm-installed packages)
console.log(`Downloading libgit2 v${LIBGIT2_VERSION}...`);

const depsDir = path.join(rootDir, 'deps');
if (!fs.existsSync(depsDir)) {
  fs.mkdirSync(depsDir, { recursive: true });
}

function download(url) {
  return new Promise((resolve, reject) => {
    https.get(url, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        return download(res.headers.location).then(resolve, reject);
      }
      if (res.statusCode !== 200) {
        return reject(new Error(`HTTP ${res.statusCode} for ${url}`));
      }
      resolve(res);
    }).on('error', reject);
  });
}

async function extractTarGz(stream, destDir) {
  const tmpFile = path.join(destDir, '_libgit2.tar.gz');

  // Save stream to file
  await new Promise((resolve, reject) => {
    const file = fs.createWriteStream(tmpFile);
    stream.pipe(file);
    file.on('finish', () => { file.close(); resolve(); });
    file.on('error', reject);
  });

  // Extract
  execSync('tar xzf _libgit2.tar.gz', { cwd: destDir, stdio: 'inherit' });

  // Rename extracted dir: libgit2-1.9.0 → libgit2
  const extracted = path.join(destDir, `libgit2-${LIBGIT2_VERSION}`);
  if (fs.existsSync(extracted)) {
    if (fs.existsSync(libgit2Dir)) {
      fs.rmSync(libgit2Dir, { recursive: true });
    }
    fs.renameSync(extracted, libgit2Dir);
  }

  fs.unlinkSync(tmpFile);
}

(async () => {
  try {
    const stream = await download(LIBGIT2_TARBALL);
    await extractTarGz(stream, depsDir);
    console.log(`libgit2 v${LIBGIT2_VERSION} downloaded and extracted.`);
  } catch (err) {
    console.error('Failed to download libgit2:', err.message);
    console.error(`Please manually download from ${LIBGIT2_TARBALL}`);
    console.error(`and extract to ${libgit2Dir}`);
    process.exit(1);
  }
})();
