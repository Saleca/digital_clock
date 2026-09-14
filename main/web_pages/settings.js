function toggle_password_visibility(e, id) {
    if (e.innerHTML == 'Show') {
        e.innerHTML = 'Hide'
        document.getElementById(id).type = "text";
    } else {
        e.innerHTML = 'Show'
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
            r.ok ? feedback('wifi', true, 'saved — reconnecting…')
                : feedback('wifi', false, 'error ' + r.status);
        } catch {
            feedback('wifi', false, 'could not reach device');
        }

    } else if (target === 'mdns') {
        const host = document.getElementById('mdns-host').value.trim();
        if (!host) {
            feedback('mdns', false, 'hostname is required');
            return;
        }
        if (!/^[a-z0-9-]+$/i.test(host)) {
            feedback('mdns', false, 'letters, numbers and hyphens only');
            return;
        }
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
        } catch {
            feedback('mdns', false, 'could not reach device');
        }
    }
}

document.getElementById('mdns-host')
    .addEventListener('input', function () {
        document.getElementById('mdns-preview').textContent = this.value.trim() || 'relogio';
    });