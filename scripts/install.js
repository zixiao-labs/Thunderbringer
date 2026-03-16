'use strict';

const path = require('path');
const fs = require('fs');
const { execSync } = require('child_process');

const rootDir = path.join(__dirname, '..');

// Detect electron-rebuild context (npm_config_runtime is set by @electron/rebuild)
const runtime = process.env.npm_config_runtime;
const isElectron = runtime === 'electron';

// When rebuilding for Electron, skip prebuild/existing-binary checks
// because those were built for Node, not Electron.
if (!isElectron) {
  // 1. Check if prebuilt binary already works
  try {
    const binding = require('node-gyp-build')(rootDir);
    if (binding) {
      console.log('thunderbringer: using prebuilt binary.');
      process.exit(0);
    }
  } catch {}

  // 2. Check if cmake-js build output exists and loads
  const releasePath = path.join(rootDir, 'build', 'Release', 'thunderbringer.node');
  if (fs.existsSync(releasePath)) {
    try {
      require(releasePath);
      console.log('thunderbringer: using existing build.');
      process.exit(0);
    } catch {
      // Exists but can't load (wrong arch?), rebuild
    }
  }
}

// 3. Need to compile from source
console.log('thunderbringer: compiling from source...');

// Fetch libgit2 if needed
try {
  execSync('node scripts/fetch-libgit2.js', {
    cwd: rootDir,
    stdio: 'inherit',
  });
} catch (err) {
  console.error('thunderbringer: failed to fetch libgit2');
  process.exit(1);
}

// Build cmake-js args, forwarding electron-rebuild env vars
const cmakeJsArgs = ['compile'];
if (isElectron) {
  cmakeJsArgs.push('--runtime=electron');
  if (process.env.npm_config_target) {
    cmakeJsArgs.push(`--target=${process.env.npm_config_target}`);
  }
  if (process.env.npm_config_arch) {
    cmakeJsArgs.push(`--arch=${process.env.npm_config_arch}`);
  }
}

// Compile with cmake-js
try {
  const cmakeJsPkgDir = path.dirname(require.resolve('cmake-js/package.json'));
  const cmakeJs = path.join(cmakeJsPkgDir, 'bin', 'cmake-js');
  execSync(`"${process.execPath}" "${cmakeJs}" ${cmakeJsArgs.join(' ')}`, {
    cwd: rootDir,
    stdio: 'inherit',
  });
  console.log('thunderbringer: compiled successfully.');
} catch (err) {
  console.error('thunderbringer: compilation failed.');
  console.error('Make sure you have CMake >= 3.15 and a C++ compiler installed.');
  process.exit(1);
}
