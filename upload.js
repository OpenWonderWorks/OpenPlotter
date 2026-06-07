const fs = require('fs');
const https = require('https');
const path = require('path');

const token = process.env.GITHUB_TOKEN || "PLACEHOLDER_TOKEN";
const filePath = path.resolve('app/dist-desktop/OpenPlotter 3.0.2.exe');
const fileName = "OpenPlotter-3.1.0-Setup.exe"; // Try replacing the original one just in case they don't see it
const uploadUrl = `https://uploads.github.com/repos/OpenWonderWorks/OpenPlotter/releases/335580753/assets?name=${fileName}`;

const stat = fs.statSync(filePath);

const options = {
  method: 'POST',
  headers: {
    'Authorization': `token ${token}`,
    'Accept': 'application/vnd.github.v3+json',
    'Content-Type': 'application/octet-stream',
    'Content-Length': stat.size,
    'User-Agent': 'NodeJS'
  }
};

const req = https.request(uploadUrl, options, (res) => {
  let body = '';
  res.on('data', chunk => body += chunk);
  res.on('end', () => {
    console.log(`Status: ${res.statusCode}`);
    console.log(`Response: ${body}`);
  });
});

req.on('error', (e) => {
  console.error(`Problem with request: ${e.message}`);
});

const fileStream = fs.createReadStream(filePath);
fileStream.pipe(req);

fileStream.on('end', () => {
  console.log("Stream ended.");
});
