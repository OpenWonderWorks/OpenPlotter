const fs = require('fs');
const https = require('https');
const path = require('path');

const GITHUB_TOKEN = process.env.GITHUB_TOKEN; 
const REPO_OWNER = 'OpenWonderWorks';

const REPO_NAME = 'OpenPlotter';
const TAG_NAME = 'v3.2.0';
const RELEASE_NAME = 'OpenPlotter v3.2.0: Flash Orchestrator';
const BODY = 'This release completely redesigns the firmware flashing experience. The simple flash button has been replaced with a fully-featured Flash Orchestrator modal featuring granular control toggles (Clean Build, Verify Flash, Auto-Install Libraries), a live build tracker, and a dedicated syntax-highlighted console output for arduino-cli.';
const ASSET_PATH = path.join(__dirname, 'app', 'dist-desktop', 'OpenPlotter 3.2.0.exe');
const ASSET_NAME = 'OpenPlotter-3.2.0-Setup.exe';

function apiRequest(method, endpoint, body, isUpload = false) {
  return new Promise((resolve, reject) => {
    const host = isUpload ? 'uploads.github.com' : 'api.github.com';
    const options = {
      hostname: host,
      path: endpoint,
      method: method,
      headers: {
        'User-Agent': 'Node.js',
        'Authorization': `Bearer ${GITHUB_TOKEN}`,
        'Accept': 'application/vnd.github.v3+json'
      }
    };

    if (body && !isUpload) {
      body = JSON.stringify(body);
      options.headers['Content-Type'] = 'application/json';
      options.headers['Content-Length'] = Buffer.byteLength(body);
    }

    if (isUpload) {
      const stats = fs.statSync(ASSET_PATH);
      options.headers['Content-Type'] = 'application/octet-stream';
      options.headers['Content-Length'] = stats.size;
    }

    const req = https.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(data ? JSON.parse(data) : null);
        } else {
          reject(`API Error ${res.statusCode}: ${data}`);
        }
      });
    });

    req.on('error', e => reject(e));

    if (isUpload) {
      console.log(`Uploading asset to https://${host}${endpoint}`);
      const fileStream = fs.createReadStream(ASSET_PATH);
      fileStream.pipe(req);
      
      let uploaded = 0;
      fileStream.on('data', (chunk) => {
        uploaded += chunk.length;
      });
      
      const stats = fs.statSync(ASSET_PATH);
      const logInterval = setInterval(() => {
        console.log(`Uploaded ${(uploaded / 1024 / 1024).toFixed(2)} MB of ${(stats.size / 1024 / 1024).toFixed(2)} MB`);
      }, 5000);
      
      fileStream.on('end', () => {
        clearInterval(logInterval);
        console.log('Stream ended.');
      });
      
    } else {
      if (body) req.write(body);
      req.end();
    }
  });
}

async function run() {
  try {
    const releaseData = {
      tag_name: TAG_NAME,
      name: RELEASE_NAME,
      body: BODY,
      draft: false,
      prerelease: false
    };

    const release = await apiRequest('POST', `/repos/${REPO_OWNER}/${REPO_NAME}/releases`, releaseData);
    console.log(`Release created successfully.`);
    const uploadUrl = release.upload_url.replace('{?name,label}', `?name=${ASSET_NAME}`);
    const uploadEndpoint = new URL(uploadUrl).pathname + new URL(uploadUrl).search;

    await apiRequest('POST', uploadEndpoint, null, true);
    console.log('Upload Status: 201');
    console.log('Upload successful!');
  } catch (error) {
    console.error('Failed to create release or upload asset:', error);
  }
}

run();
