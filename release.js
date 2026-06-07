const fs = require('fs');
const https = require('https');
const path = require('path');

const GITHUB_TOKEN = process.env.GITHUB_TOKEN || "github_pat_11A2YOS7Q02gKOf1wK8n2Q_0zR6fF0r2T5RjS2yv5dFhLwL5O7C2z1E7s5Z7S1g5U6D7O4W5R7T9Z7X3N"; 
const REPO_OWNER = 'OpenWonderWorks';
const REPO_NAME = 'OpenPlotter';
const TAG_NAME = 'v3.1.8';
const RELEASE_NAME = 'OpenPlotter v3.1.8: Flash UI Hotfix';
const BODY = 'This release fixes a critical ReferenceError in the UI that crashed the application when attempting to flash firmware, ensuring that progress logs stream cleanly into the System Logs tab.';
const ASSET_PATH = path.join(__dirname, 'app', 'dist-desktop', 'OpenPlotter 3.1.8.exe');
const ASSET_NAME = 'OpenPlotter-3.1.8-Setup.exe';

function apiRequest(method, endpoint, body, isUpload = false) {
  return new Promise((resolve, reject) => {
    const host = isUpload ? 'uploads.github.com' : 'api.github.com';
    const options = {
      hostname: host,
      path: endpoint,
      method: method,
      headers: {
        'User-Agent': 'Node.js',
        'Authorization': `token ${GITHUB_TOKEN}`,
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
