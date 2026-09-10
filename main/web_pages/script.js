//global
//navigation
const routes = {
    home: '/',
    post_credentials: '/post/credentials',
    post_mdns: '/post/mdns',
    get_clock: '/get/clock',
    post_clock_preview: '/post/clock',
    post_clock_save: '/post/clock_save',
    get_version: '/get/version',
    post_flash: '/post/flash'
};

function navigate(target) {
    window.location.href = routes[target] ?? '/';
}

//settings
function toggle_password_visibility(e, id) {
    if (e.innerHTML == 'Show') {
        e.innerHTML = 'Hide'
        document.getElementById(id).type = "text";
    } else {
        e.innerHTML = 'Show'
        document.getElementById(id).type = "password";
    }
}

function feedback(id, ok, msg) {
    const el = document.getElementById('fb-' + id);
    el.className = 'feedback ' + (ok ? 'ok' : 'err');
    el.textContent = msg;
    clearTimeout(el._t);
    el._t = setTimeout(() => el.className = 'feedback', 4000);
}

async function save(target) {
    /* ---- wifi ---- */
    if (target === 'wifi') {
        const ssid = document.getElementById('wifi-ssid').value.trim();
        const pass = document.getElementById('wifi-pass').value;
        if (!ssid) { feedback('wifi', false, 'ssid is required'); return; }
        try {
            const r = await fetch(routes.post_credentials, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password: pass })
            });
            r.ok ? feedback('wifi', true, 'saved — reconnecting…')
                : feedback('wifi', false, 'error ' + r.status);
        } catch { feedback('wifi', false, 'could not reach device'); }
    }

    /* ---- mdns ---- */
    if (target === 'mdns') {
        const host = document.getElementById('mdns-host').value.trim();
        if (!host) { feedback('mdns', false, 'hostname is required'); return; }
        if (!/^[a-z0-9-]+$/i.test(host)) { feedback('mdns', false, 'letters, numbers and hyphens only'); return; }
        try {
            const r = await fetch(routes.post_mdns, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ mdns: host })
            });
            if (r.ok) {
                feedback('mdns', true, 'saved — restarting mDNS…');
                setTimeout(() => {
                    window.location.href = `http://${host}.local`;
                }, 2000);
            } else {
                feedback('mdns', false, 'error ' + r.status);
            }
        } catch { feedback('mdns', false, 'could not reach device'); }
    }
}

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
    console.log(`remove version: ${data.project_version}`);
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

function check4update() {
    fetch_remote_version().then(remote_version => {
        if (is_new_ver_recent(current_version, remote_version)) {
            feedback('flash', true, 'downloading firmware');
            return fetch_firmware().then(file => {
                upload_firmware(file);
            });
        } else {
            feedback('flash', true, 'up to date');
        }
    })
        .catch(err => console.error('Error checking for update:', err));
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
        feedback('flash', true, await res.text());
    } catch {
        feedback('flash', false, 'could not reach device');
    }
}


function settings_init() {
    document.getElementById('mdns-host')
        .addEventListener('input', function () {
            document.getElementById('mdns-preview').textContent = this.value.trim() || 'relogio';
        });
}

let colour_pickers = {
    hour: { label: 'Hora', h: 60, s: 100, v: 100 },
    minute: { label: 'Minuto', h: 60, s: 100, v: 100 },
    second: { label: 'Segundo', h: 0, s: 0, v: 100 },
    background: { label: 'Fundo', h: 0, s: 0, v: 0 },
};
const preview_colours_button = document.getElementById('preview-colours-btn');
const has_seconds_checkbox = document.getElementById('has-seconds');
const colour_pickers_container = document.getElementById('colour-cards');

const brightness_day_slider = document.getElementById('day-brightness');
const brightness_night_slider = document.getElementById('night-brightness');
const brightness_day_value = document.getElementById('day-val');
const brightness_night_value = document.getElementById('night-val');

let previewed = false;

function colour_picker_mode(colour_picker) {
    if (colour_picker.s === 0 && colour_picker.v === 0) return 'black';
    if (colour_picker.s === 0) return 'white';
    return 'colour';
}

function hsv2hex(h, s, v) {
    if (v !== 0) v = 100;
    const sN = s / 100, vN = v / 100;
    const c = vN * sN;
    const x = c * (1 - Math.abs((h / 60) % 2 - 1));
    const m = vN - c;
    let r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    const toHex = n => Math.round((n + m) * 255).toString(16).padStart(2, '0');
    return '#' + toHex(r) + toHex(g) + toHex(b);
}

function render_colour_picker(key) {
    const colour_picker = colour_pickers[key];

    const mode = colour_picker_mode(colour_picker);
    const hex = hsv2hex(colour_picker.h, colour_picker.s, colour_picker.v);
    //console.log(`render ${key}: h=${colour_picker.h} s=${colour_picker.s} v=${colour_picker.v} -> mode=${mode} hex=${hex}`);

    document.getElementById('swatch-' + key).style.background = hex;

    document.querySelectorAll(`.mode-toggle[data-slot="${key}"] button`).forEach(btn => {
        btn.classList.toggle('active', btn.dataset.mode === mode);
    });

    const hue_slider = document.getElementById('hue-' + key);
    hue_slider.value = colour_picker.h;
    hue_slider.classList.toggle('disabled-slider', mode !== 'colour');
}

function remove_preview_mode() {
    if (previewed) {
        previewed = false;
        preview_colours_button.textContent = 'Preview';
        preview_colours_button.classList.remove('save-state');
    }
}

async function preview_colours() {
    preview_colours_button.disabled = true;
    try {
        if (!previewed) {
            const r = await fetch(routes.post_clock_preview, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    hour_hs: { h: colour_pickers.hour.h, s: colour_pickers.hour.s, v: colour_pickers.hour.v },
                    minute_hs: { h: colour_pickers.minute.h, s: colour_pickers.minute.s, v: colour_pickers.minute.v },
                    second_hs: { h: colour_pickers.second.h, s: colour_pickers.second.s, v: colour_pickers.second.v },
                    background_hs: { h: colour_pickers.background.h, s: colour_pickers.background.s, v: colour_pickers.background.v },
                    has_seconds: has_seconds_checkbox.checked,
                    is_background_black: colour_picker_mode(colour_pickers.background) === 'black',
                    day_brightness: parseInt(brightness_day_slider.value, 10),
                    night_brightness: parseInt(brightness_night_slider.value, 10),
                }),
            });
            if (!r.ok) throw new Error('error ' + r.status);

            previewed = true;
            preview_colours_button.textContent = 'Save';
            preview_colours_button.classList.add('save-state');
            feedback('colour', true, 'previewing on device…');
        } else {
            const r = await fetch(routes.post_clock_save, { method: 'POST' });
            if (!r.ok) throw new Error('error ' + r.status);

            feedback('colour', true, 'saved');
            preview_colours_button.textContent = 'Preview';
            preview_colours_button.classList.remove('save-state');
            previewed = false;
        }
    } catch (e) {
        feedback('colour', false, e.message || 'could not reach device');
    } finally {
        preview_colours_button.disabled = false;
    }
}

async function load_clock_defaults() {
    try {
        const r = await fetch(routes.get_clock);
        if (!r.ok) {
            throw new Error('error ' + r.status);
        }
        const data = await r.json();

        Object.keys(colour_pickers).forEach(key => {
            const hsv = data[key + '_hs'];
            colour_pickers[key].h = hsv.h;
            colour_pickers[key].s = hsv.s;
            colour_pickers[key].v = hsv.v;
            render_colour_picker(key);
        });

        has_seconds_checkbox.checked = data.has_seconds;
        document.getElementById('card-second').classList.toggle('hidden-slot', !data.has_seconds);

        brightness_day_slider.value = data.day_brightness;
        brightness_day_value.textContent = data.day_brightness;

        brightness_night_slider.value = data.night_brightness;
        brightness_night_value.textContent = data.night_brightness;
        feedback('colour', true, 'loaded colours');

    } catch (e) {
        feedback('colour', false, 'could not load current settings');
    }
}

async function colour_pickers_init() {
    Object.entries(colour_pickers).forEach(([key, colour_picker]) => {
        const card = document.createElement('div');
        card.className = 'colour-card';
        card.id = 'card-' + key;
        card.innerHTML = `
      <div class="colour-card-top">
        <div class="swatch" id="swatch-${key}"></div>
        <label>${colour_picker.label}</label>
      </div>
      <div class="mode-toggle" data-slot="${key}">
        <button data-mode="colour">Cor</button>
        <button data-mode="white">Branco</button>
        <button data-mode="black">Preto</button>
      </div>
      <input type="range" class="hue-slider" id="hue-${key}" min="0" max="359" value="${colour_picker.h}">
    `;
        colour_pickers_container.appendChild(card);
    });

    Object.keys(colour_pickers).forEach(key => {
        document.querySelector(`.mode-toggle[data-slot="${key}"]`).addEventListener('click', (e) => {
            const btn = e.target.closest('button');
            if (!btn) {
                return;
            }

            const colour_picker = colour_pickers[key];
            if (btn.dataset.mode === 'colour') {
                colour_picker.s = 100;
                colour_picker.v = 100;
            }
            else if (btn.dataset.mode === 'white') {
                colour_picker.s = 0;
                colour_picker.v = 100;
            }
            else if (btn.dataset.mode === 'black') {
                colour_picker.s = 0;
                colour_picker.v = 0;
            }

            render_colour_picker(key);
            remove_preview_mode();
        });

        document.getElementById('hue-' + key).addEventListener('input', (e) => {
            colour_pickers[key].h = parseInt(e.target.value, 10);
            render_colour_picker(key);
            remove_preview_mode();
        });

        render_colour_picker(key);
    });

    brightness_day_slider.addEventListener('input', () => {
        const day = parseInt(brightness_day_slider.value, 10);
        const night = parseInt(brightness_night_slider.value, 10);
        if (day < night) {
            brightness_night_slider.value = day;
            brightness_night_value.textContent = day;
        }
        brightness_day_value.textContent = day;
        remove_preview_mode();
    });

    brightness_night_slider.addEventListener('input', () => {
        const day = parseInt(brightness_day_slider.value, 10);
        const night = parseInt(brightness_night_slider.value, 10);
        if (night > day) {
            brightness_day_slider.value = night;
            brightness_day_value.textContent = night;
        }
        brightness_night_value.textContent = night;
        remove_preview_mode();
    });

    has_seconds_checkbox.addEventListener('change', () => {
        document.getElementById('card-second').classList.toggle('hidden-slot', !has_seconds_checkbox.checked);
        remove_preview_mode();
    });

    await fetch_current_version();
    console.log(`version: ${current_version}`);

    load_clock_defaults();
}
