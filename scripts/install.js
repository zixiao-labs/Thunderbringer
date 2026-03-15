'use strict';

const path = require('path');
const fs = require('fs');
const { execSync } = require('child_process');

const rootDir = path.join(__dirname, '..');

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

// Compile with cmake-js
try {
  const cmakeJs = path.join(rootDir, 'node_modules', '.bin', 'cmake-js');
  execSync(`"${cmakeJs}" compile`, {
    cwd: rootDir,
    stdio: 'inherit',
  });
  console.log('thunderbringer: compiled successfully.');
} catch (err) {
  console.error('thunderbringer: compilation failed.');
  console.error('Make sure you have CMake >= 3.15 and a C++ compiler installed.');
  process.exit(1);
}
