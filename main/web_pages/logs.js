const logs_el = document.getElementById('logs');
async function delete_logs() {
    try {
        const r = await fetch(routes.delete_logs, { method: 'DELETE' });
        if (!r.ok) throw new Error('error ' + r.status);
        feedback('logs', true, 'logs eliminados com sucesso');
        logs_el.textContent = 'Apagado.';
    } catch (e) {
        feedback('logs', false, e.message || 'dispositivo fora de alcance');
    }
}

fetch(routes.get_logs).then(r => {
    if (!r.ok) {
        throw new Error('error ' + r.status);
    }

    r.text().then(text => {
        logs_el.textContent = text;
        feedback('logs', true, 'logs loaded');
    });
}).catch(e => {
    logs_el.textContent = 'nao foi possivel obter os logs';
    feedback('logs', false, e.message || 'dispositivo fora de alcance');
});