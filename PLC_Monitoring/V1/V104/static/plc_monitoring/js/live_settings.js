let eventSource = null;
let keyNames = {};

function loadKeyNames() {
    fetch('/api/plc-keys/')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                data.data.forEach(item => {
                    keyNames[item.key] = item.fa_name || item.name || item.key;
                });
            }
        })
        .catch(err => console.error('Error loading key names:', err));
}

function getKeyName(key) {
    return keyNames[key] || key;
}

function formatTimestamp(timestamp) {
    if (!timestamp) return '-';
    const date = new Date(timestamp * 1000);
    const jDate = jalaali.toJalaali(date.getFullYear(), date.getMonth() + 1, date.getDate());
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    return `${jDate.jy}/${jDate.jm}/${jDate.jd} - ${hours}:${minutes}:${seconds}`;
}

function updateSettings(data) {
    const container = document.getElementById('settings_container');
    const timeEl = document.getElementById('last_update_time');
    
    if (timeEl) {
        timeEl.textContent = formatTimestamp(data.LastUpdate);
    }
    
    if (!data.setting || Object.keys(data.setting).length === 0) {
        return;
    }
    
    let grid = container.querySelector('.settings-grid-large');
    
    if (!grid) {
        container.innerHTML = '<div class="settings-grid-large"></div>';
        grid = container.querySelector('.settings-grid-large');
    }
    
    for (const [key, value] of Object.entries(data.setting)) {
        let card = grid.querySelector(`.setting-card[data-key="${key}"]`);
        
        if (card) {
            const valueEl = card.querySelector('.setting-card-value');
            if (valueEl.textContent !== String(value)) {
                valueEl.textContent = value;
                card.classList.add('value-changed');
                setTimeout(() => card.classList.remove('value-changed'), 500);
            }
        } else {
            const newCard = document.createElement('div');
            newCard.className = 'setting-card new-card';
            newCard.dataset.key = key;
            newCard.innerHTML = `
                <div class="setting-card-key">${getKeyName(key)}</div>
                <div class="setting-card-value">${value}</div>
            `;
            grid.appendChild(newCard);
            setTimeout(() => newCard.classList.remove('new-card'), 500);
        }
    }
}

function connectSSE() {
    if (eventSource) {
        eventSource.close();
    }
    
    eventSource = new EventSource(`/api/settings/stream/?plc=${PLC_ID}`);
    
    eventSource.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            updateSettings(data);
        } catch (e) {
            console.error('Error parsing SSE data:', e);
        }
    };
    
    eventSource.onerror = function(err) {
        console.error('SSE Error:', err);
        eventSource.close();
        setTimeout(connectSSE, 3000);
    };
}

document.addEventListener('DOMContentLoaded', function() {
    loadKeyNames();
    if (typeof PLC_ID !== 'undefined') {
        connectSSE();
    }
});

window.addEventListener('beforeunload', function() {
    if (eventSource) {
        eventSource.close();
    }
});
