function toggle_password_visibility(e, id) {
    if (e.innerHTML == 'Ver') {
        e.innerHTML = 'X'
        document.getElementById(id).type = "text";
    } else {
        e.innerHTML = 'Ver'
        document.getElementById(id).type = "password";
    }
}

async function save(target) {
    if (target === 'wifi') {
        const ssid = document.getElementById('wifi-ssid').value.trim();
        const pass = document.getElementById('wifi-pass').value;
        if (!ssid) {
            feedback('wifi', false, 'ssid is required');
            return;
        }
        try {
            const r = await fetch(routes.post_credentials, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password: pass })
            });
            r.ok ? feedback('wifi', true, 'guardado — a reconectar...')
                : feedback('wifi', false, 'erro: ' + r.status);
        } catch {
            feedback('wifi', false, 'dispositivo fora de alcance');
        }

    } else if (target === 'mdns') {
        const host = document.getElementById('mdns-host').value.trim();
        if (!host) {
            feedback('mdns', false, 'hostname is required');
            return;
        }
        if (!/^[a-z0-9-]+$/i.test(host)) {
            feedback('mdns', false, 'apenas letras, numeros e hifens');
            return;
        }
        try {
            const r = await fetch(routes.post_mdns, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ mdns: host })
            });
            if (r.ok) {
                feedback('mdns', true, 'guardado — pagina vai atualizar em 2s');
                setTimeout(() => {
                    window.location.href = `http://${host}.local`;
                }, 2000);
            } else {
                feedback('mdns', false, 'erro ' + r.status);
            }
        } catch {
            feedback('mdns', false, 'dispositivo fora de alcance');
        }
    }
}

document.getElementById('mdns-host')
    .addEventListener('input', function () {
        document.getElementById('mdns-preview').textContent = this.value.trim() || 'relogio';
    });