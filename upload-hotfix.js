const fs = require('fs');
const path = require('path');

const token = process.env.GITHUB_TOKEN || "PLACEHOLDER_TOKEN";
const filePath = path.resolve('./app/dist-desktop/OpenPlotter 3.1.6.exe');
const fileName = "OpenPlotter-3.1.6-Release.exe";

async function publish() {
  console.log('Creating release v3.1.6...');
  
  // Create Release
  const createReq = await fetch('https://api.github.com/repos/OpenWonderWorks/OpenPlotter/releases', {
    method: 'POST',
    headers: {
      'Authorization': `token ${token}`,
      'Accept': 'application/vnd.github.v3+json',
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      tag_name: 'v3.1.6',
      target_commitish: 'main',
      name: 'OpenPlotter v3.1.6',
      body: '## Fixes\\n- **Auto-Refresh**: App now smoothly auto-reloads after downloading missing toolchains.\\n- **Flashing Fixed**: Built-in firmware is now compiled dynamically from source on the fly when clicking Flash, ensuring it works perfectly out of the box!',
      draft: false,
      prerelease: false,
      generate_release_notes: false
    })
  });
  
  let rel;
  if (!createReq.ok) {
    const err = await createReq.json();
    if (err.errors && err.errors[0] && err.errors[0].code === 'already_exists') {
      console.log('Release already exists. Fetching it...');
      const getRel = await fetch('https://api.github.com/repos/OpenWonderWorks/OpenPlotter/releases/tags/v3.1.6', {
        headers: { 'Authorization': `token ${token}` }
      });
      rel = await getRel.json();
    } else {
      console.error('Failed to create release:', err);
      return;
    }
  } else {
    rel = await createReq.json();
  }
  
  const stat = fs.statSync(filePath);
  console.log(`Size: ${Math.round(stat.size/1024/1024)} MB. Release ID: ${rel.id}`);

  // Upload new asset
  const uploadUrl = rel.upload_url.replace('{?name,label}', `?name=${encodeURIComponent(fileName)}`);
  
  console.log('Starting POST to', uploadUrl);
  const stream = fs.createReadStream(filePath);
  
  const result = await fetch(uploadUrl, {
    method: 'POST',
    headers: {
      'Authorization': `token ${token}`,
      'Content-Type': 'application/octet-stream',
      'Content-Length': stat.size
    },
    body: stream,
    duplex: 'half'
  });
  
  if (!result.ok) {
    console.error('Upload failed:', await result.text());
  } else {
    console.log('Upload SUCCESS!');
  }
}

publish().catch(console.error);
