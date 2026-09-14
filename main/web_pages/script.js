//navigation
const routes = {
    home: '/',
    settings: 'settings.html',
    update: 'update.html',
    post_credentials: '/post/credentials',
    post_mdns: '/post/mdns',
    get_clock: '/get/clock',
    post_clock_preview: '/post/clock',
    post_clock_save: '/post/clock_save',
    get_version: '/get/version',
    post_flash: '/post/flash'
};

function navigate(target) {
    window.location.href = routes[target] ?? routes['home'];
}

function feedback(id, ok, msg) {
    const el = document.getElementById('fb-' + id);
    el.className = 'feedback ' + (ok ? 'ok' : 'err');
    el.textContent = msg;
    clearTimeout(el._t);
    el._t = setTimeout(() => el.className = 'feedback', 4000);
}

document.querySelectorAll('.btn-nav').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.btn-nav').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
    });
});