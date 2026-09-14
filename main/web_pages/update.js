const repo_url = 'https://raw.githubusercontent.com/Saleca/digital_clock/main/build';
const firmware_url = repo_url + '/digital_clock.bin';
const version_url = repo_url + '/version.json';
let current_version = "0.0.0";

function is_new_ver_recent(old_ver, new_ver) {
    const old_split = old_ver.split('.').map(Number);
    const new_split = new_ver.split('.').map(Number);
    const len = Math.max(old_split.length, new_split.length);

    for (let i = 0; i < len; i++) {
        const old_value = old_split[i] || 0;
        const new_value = new_split[i] || 0;
        if (old_value < new_value) {
            return true;
        }
        else if (old_value > new_value) {
            return false;
        }
    }
    return false;
}

async function fetch_remote_version() {
    const response = await fetch(version_url + '?t=' + Date.now(), { cache: 'no-store' });

    if (!response.ok) {
        throw new Error(`Failed to fetch version: ${response.status} ${response.statusText}`);
    }

    const data = await response.json();
    console.log(`remote version: ${data.project_version}`);
    return data.project_version;
}
async function fetch_current_version() {
    const response = await fetch(routes.get_version + '?t=' + Date.now(), { cache: 'no-store' }); // adjust to your actual route

    if (!response.ok) {
        throw new Error(`Failed to fetch device version: ${response.status} ${response.statusText}`);
    }

    const text = await response.text();
    current_version = text.trim();
}

async function fetch_firmware() {
    const response = await fetch(firmware_url);

    if (!response.ok) {
        throw new Error(`Failed to fetch firmware: ${response.status} ${response.statusText}`);
    }

    const blob = await response.blob();
    const file = new File([blob], 'digital_clock.bin', { type: 'application/octet-stream' });
    return file;
}

function check_remote_version() {
    fetch_remote_version()
        .then(remote_version => {
            if (is_new_ver_recent(current_version, remote_version)) {
                document.getElementById('update-status').textContent = `update disponivel: ${remote_version}`;
                document.getElementById('btn-update').disabled = false;
            } else {
                document.getElementById('update-status').textContent = 'já está atualizado';
            }
        })
        .catch(err => console.error('Error checking for update:', err));
}

function update_firmware() {
    feedback('update', true, 'downloading firmware');
    fetch_firmware().then(file => {
        upload_firmware(file);
    });
}

async function upload_firmware(file) {
    try {
        const res = await fetch(routes.post_flash, {
            method: 'POST',
            headers: { 'Content-Type': 'application/octet-stream' },
            body: file,
        });
        if (!res.ok) {
            throw new Error(`update failed: ${res.status}`);
        }
        feedback('update', true, await res.text());
    } catch {
        feedback('update', false, 'could not reach device');
    }
}

const file_picker_input = document.getElementById('file_picker');
const upload_btn = document.getElementById('btn-upload'); 

file_picker_input.addEventListener('change', () => {
    upload_btn.disabled = file_picker_input.files.length === 0;
});

fetch_current_version()
    .then(() => {
        console.log(`version: ${current_version}`);
        check_remote_version();
    })
    .catch(err => {
        console.error("failled to load version:", err);
    });
