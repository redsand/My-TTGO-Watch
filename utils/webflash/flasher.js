
let port;
let firmwareFiles = [];
let esploader;

const logEl = document.getElementById('log');

function log(msg, type = '') {
  console.log(msg);
  const span = document.createElement('span');
  span.textContent = msg + '\n';
  if (type) span.classList.add(type);
  logEl.appendChild(span);
  logEl.scrollTop = logEl.scrollHeight;
}

function updateProgress(percent) {
  document.getElementById('progress-bar').style.width = percent + '%';
}

document.getElementById('mode').addEventListener('change', (e) => {
  const mode = e.target.value;
  if (mode === 'upload') {
    document.getElementById('upload-section').style.display = 'block';
    document.getElementById('url-section').style.display = 'none';
  } else {
    document.getElementById('upload-section').style.display = 'none';
    document.getElementById('url-section').style.display = 'block';
  }
});

document.getElementById('firmware').addEventListener('change', (e) => {
  firmwareFiles = [...e.target.files];
  log(`Loaded ${firmwareFiles.length} firmware file(s)`);
});

document.getElementById('download-firmware').addEventListener('click', async () => {
  const url = document.getElementById('firmware-url').value.trim();
  if (!url) {
    log('Please enter a firmware URL.', 'error');
    return;
  }

  try {
    log(`Downloading firmware from ${url}...`);
    const response = await fetch(url);
    if (!response.ok) throw new Error('Network response was not ok');

    const blob = await response.blob();
    const fileName = url.split('/').pop().toLowerCase();
    const arrayBuffer = await blob.arrayBuffer();

    firmwareFiles = [{
      name: fileName,
      arrayBuffer
    }];

    log(`Firmware ${fileName} downloaded successfully. Ready to flash.`, 'success');
  } catch (err) {
    log('Download error: ' + err.message, 'error');
  }
});

document.getElementById('connect').addEventListener('click', async () => {
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    log('Connected to T-Watch', 'success');

    esploader = new ESPTool(port, 115200);

    await esploader.initialize();
    log('Device in bootloader mode', 'success');

    document.getElementById('flash').disabled = false;
  } catch (err) {
    log('Connection error: ' + err.message, 'error');
  }
});

document.getElementById('flash').addEventListener('click', async () => {
  if (firmwareFiles.length === 0) {
    log('Please load firmware first.', 'error');
    return;
  }

  try {
    const eraseAll = document.getElementById('eraseFlash').checked;
    log(eraseAll ? 'Erasing flash...' : 'Skipping erase...');
    if (eraseAll) await esploader.eraseFlash();

    const fileArray = [];

    for (const file of firmwareFiles) {
      const arrayBuffer = file.arrayBuffer || await file.arrayBuffer();
      const fileName = file.name.toLowerCase();

      let address = 0x10000;
      if (fileName.includes('bootloader')) address = 0x1000;
      else if (fileName.includes('partition')) address = 0x8000;

      fileArray.push({
        data: new Uint8Array(arrayBuffer),
        address
      });

      log(`Prepared ${file.name} for address 0x${address.toString(16)}`);
    }

    log('Starting flashing...');
    await esploader.flash({
      fileArray,
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (p) => {
        updateProgress(p);
        log(`Progress: ${p.toFixed(2)}%`);
      }
    });

    log('Flash complete! Rebooting...', 'success');
    await esploader.hardReset();
    await port.close();
    log('Disconnected.', 'success');

  } catch (err) {
    log('Flashing error: ' + err.message, 'error');
  }
});

const dropArea = document.getElementById('drop-area');

dropArea.addEventListener('dragover', (e) => {
  e.preventDefault();
  dropArea.classList.add('dragover');
});

dropArea.addEventListener('dragleave', () => {
  dropArea.classList.remove('dragover');
});

dropArea.addEventListener('drop', async (e) => {
  e.preventDefault();
  dropArea.classList.remove('dragover');

  firmwareFiles = [...e.dataTransfer.files];
  log(`Loaded ${firmwareFiles.length} firmware file(s) via drag & drop`);
});
