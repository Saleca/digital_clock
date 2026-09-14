let colour_pickers = {
    hour: { label: 'Horas', h: 60, s: 100, v: 100 },
    minute: { label: 'Minutos', h: 60, s: 100, v: 100 },
    second: { label: 'Segundos', h: 0, s: 0, v: 100 },
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

    document.querySelectorAll(`.nav-bar[data-slot="${key}"] button`).forEach(btn => {
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

Object.entries(colour_pickers).forEach(([key, colour_picker]) => {
    const card = document.createElement('div');
    card.className = 'colour-card';
    card.id = 'card-' + key;
    card.innerHTML = `
    <div class="nav-bar" data-slot="${key}">
        <div class="colour-card-head">
            <div class="swatch" id="swatch-${key}"></div>
            <h2> ${colour_picker.label}</h2>
        </div>
        <div class="colour-card-bar">
            <button data-mode="colour" class="btn-nav bar-stretch btn-border-left">Cor</button>
            <button data-mode="white" class="btn-nav bar-stretch btn-border-middle">Branco</button>
            <button data-mode="black" class="btn-nav bar-stretch btn-border-right">Preto</button>
        </div>
    </div>
    <input type="range" class="hue-slider" id="hue-${key}" min="0" max="359" value="${colour_picker.h}">
    `;
    colour_pickers_container.appendChild(card);
});

Object.keys(colour_pickers).forEach(key => {
    document.querySelector(`.nav-bar[data-slot="${key}"]`).addEventListener('click', (e) => {
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

fetch(routes.get_clock).then(res => {
    if (!res.ok) {
        throw new Error('error ' + res.status);
    }
    return res.json();
})
    .then(data => {
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
    })
    .catch(err => {
        feedback('colour', false, 'could not load current settings');
    });

//keep a copy of this default to allow reset since clock will only store current face (even if preview)