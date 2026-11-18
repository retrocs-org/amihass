'use strict';
const announcement = document.querySelector('#announcement');
document.querySelectorAll('[data-device]').forEach(button => {
  button.addEventListener('click', () => {
    const on = button.getAttribute('aria-pressed') !== 'true';
    button.setAttribute('aria-pressed', String(on));
    button.classList.toggle('off', !on);
    button.querySelector('.tile-value').textContent = on ? 'On' : 'Off';
    announcement.textContent = `${button.dataset.device} switched ${on ? 'on' : 'off'} in this web preview.`;
  });
});
