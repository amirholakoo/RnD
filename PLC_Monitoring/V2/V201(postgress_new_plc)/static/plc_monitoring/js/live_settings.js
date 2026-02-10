let eventSource = null;
let keyNames = {};
let keyOrders = {};
let alertConfigs = {};
let alertStates = {};

function loadKeyNames() {
    fetch('/api/plc-keys/')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                keyNames = {};
                keyOrders = {};
                data.data.forEach(item => {
                    keyNames[item.key] = item.fa_name || item.name || item.key;
                    if (typeof item.order_index === 'number') {
                        keyOrders[item.key] = item.order_index;
                    }
                });
                applySettingsOrder();
                initSettingCardsDrag();
            }
        })
        .catch(err => console.error('Error loading key names:', err));
}

function getKeyName(key) {
    return keyNames[key] || key;
}

// Format display value for 'st' key (m -> MAN, a -> AUTO)
function formatDisplayValue(key, value) {
    if (key === 'st') {
        if (value === 'm') return 'MAN';
        if (value === 'a') return 'AUTO';
    }
    return value;
}

// Update card style based on key and value
function updateCardStyle(card, key, value) {
    const numValue = parseFloat(value);
    const isNumeric = !isNaN(numValue) && isFinite(value);
    
    if (key === 'st') {
        if (value === 'm') {
            card.style.background = 'linear-gradient(135deg, #f8f9fa 0%, #ffe9a6 100%)';
            card.style.border = '1px solid #FFC107';
        } else if (value === 'a') {
            card.style.background = 'rgba(76, 175, 80, 0.2)';
            card.style.borderColor = '#4caf50';
        } else {
            card.style.background = '';
            card.style.border = '';
        }
    } else if (key === 'ru' && value === '1') {
        card.style.background = 'rgba(76, 175, 80, 0.2)';
        card.style.borderColor = '#4caf50';
    } else if (isNumeric && alertConfigs[key]) {
        const config = alertConfigs[key];
        let alertTriggered = false;
        let alertType = null;
        
        if (config.max_value !== null && numValue > config.max_value) {
            const color = config.color_max || '#ff4444';
            card.style.background = `linear-gradient(135deg, transparent 0%, ${color} 100%)`;
            card.style.border = `1px solid ${color}`;
            alertTriggered = true;
            alertType = 'max';
        } else if (config.min_value !== null && numValue < config.min_value) {
            const color = config.color_min || '#ff8800';
            card.style.background = `linear-gradient(135deg, transparent 0%, ${color} 100%)`;
            card.style.border = `1px solid ${color}`;
            alertTriggered = true;
            alertType = 'min';
        } else {
            card.style.background = '';
            card.style.border = '';
        }
        
        if (alertTriggered && config.alert_types) {
            const prevState = alertStates[key];
            const newState = { type: alertType, value: numValue };
            
            if (!prevState || prevState.type !== alertType || Math.abs(prevState.value - numValue) > 0.01) {
                triggerAlert(key, alertType, numValue, config);
                alertStates[key] = newState;
            }
        } else {
            alertStates[key] = null;
        }
    }
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
        const displayValue = formatDisplayValue(key, value);
        
        if (card) {
            const valueEl = card.querySelector('.setting-card-value');
            const valueChanged = valueEl.textContent !== String(displayValue);
            if (valueChanged) {
                valueEl.textContent = displayValue;
                card.classList.add('value-changed');
                setTimeout(() => card.classList.remove('value-changed'), 500);
            }
            card.dataset.rawValue = value;
            updateCardStyle(card, key, value);
        } else {
            const newCard = document.createElement('div');
            newCard.className = 'setting-card new-card';
            newCard.dataset.key = key;
            newCard.setAttribute('draggable', 'true');
            newCard.setAttribute('title', 'کلیک برای تنظیمات هشدار');
            newCard.innerHTML = `
                <div class="setting-card-key">${getKeyName(key)}</div>
                <div class="setting-card-value">${displayValue}</div>
            `;
            newCard.dataset.rawValue = value;
            updateCardStyle(newCard, key, value);
            grid.appendChild(newCard);
            setTimeout(() => newCard.classList.remove('new-card'), 500);
        }
    }

    initSettingCardsDrag();
    applySettingsOrder();
}

function initSettingCardsDrag() {
    const grid = document.querySelector('#settings_container .settings-grid-large');
    if (!grid) {
        return;
    }
    const cards = grid.querySelectorAll('.setting-card');
    cards.forEach(card => {
        card.setAttribute('draggable', 'true');
    });
}

function applySettingsOrder() {
    const container = document.getElementById('settings_container');
    if (!container || !Object.keys(keyOrders).length) {
        return;
    }
    const grid = container.querySelector('.settings-grid-large');
    if (!grid) {
        return;
    }
    const cards = Array.from(grid.querySelectorAll('.setting-card'));
    if (!cards.length) {
        return;
    }

    cards.sort((a, b) => {
        const keyA = a.dataset.key || '';
        const keyB = b.dataset.key || '';
        const orderA = keyOrders.hasOwnProperty(keyA) ? keyOrders[keyA] : 9999;
        const orderB = keyOrders.hasOwnProperty(keyB) ? keyOrders[keyB] : 9999;
        if (orderA === orderB) {
            return keyA.localeCompare(keyB);
        }
        return orderA - orderB;
    });

    cards.forEach(card => grid.appendChild(card));
}

let draggedSettingCard = null;
let wasDragged = false;
let mouseDownTime = 0;
let mouseDownPos = { x: 0, y: 0 };

document.addEventListener('mousedown', function(e) {
    const card = e.target.closest('.setting-card');
    if (card && card.closest('#settings_container')) {
        mouseDownTime = Date.now();
        mouseDownPos = { x: e.clientX, y: e.clientY };
        wasDragged = false;
    }
});

document.addEventListener('dragstart', function(e) {
    const card = e.target.closest('.setting-card');
    if (!card || !card.closest('#settings_container')) {
        return;
    }
    
    wasDragged = true;
    draggedSettingCard = card;
    card.classList.add('dragging');
    if (e.dataTransfer) {
        e.dataTransfer.effectAllowed = 'move';
        e.dataTransfer.setData('text/plain', card.dataset.key || '');
    }
});

document.addEventListener('dragend', function(e) {
    const card = e.target.closest('.setting-card');
    if (card && card.closest('#settings_container')) {
        card.classList.remove('dragging');
    }
    document.querySelectorAll('#settings_container .setting-card').forEach(el => {
        el.classList.remove('drag-over');
    });
    draggedSettingCard = null;
    setTimeout(() => {
        wasDragged = false;
    }, 100);
});

document.addEventListener('dragover', function(e) {
    if (!draggedSettingCard) {
        return;
    }
    const card = e.target.closest('.setting-card');
    if (!card || !card.closest('#settings_container')) {
        return;
    }
    e.preventDefault();
    if (e.dataTransfer) {
        e.dataTransfer.dropEffect = 'move';
    }

    document.querySelectorAll('#settings_container .setting-card').forEach(el => {
        el.classList.remove('drag-over');
    });
    card.classList.add('drag-over');
});

document.addEventListener('drop', function(e) {
    if (!draggedSettingCard) {
        return;
    }
    const targetCard = e.target.closest('.setting-card');
    if (!targetCard || !targetCard.closest('#settings_container')) {
        return;
    }
    e.preventDefault();
    targetCard.classList.remove('drag-over');

    const grid = targetCard.closest('.settings-grid-large');
    if (!grid) {
        return;
    }
    const cards = Array.from(grid.querySelectorAll('.setting-card'));
    const draggedIndex = cards.indexOf(draggedSettingCard);
    const targetIndex = cards.indexOf(targetCard);
    if (draggedIndex === -1 || targetIndex === -1 || draggedSettingCard === targetCard) {
        return;
    }

    if (draggedIndex < targetIndex) {
        targetCard.after(draggedSettingCard);
    } else {
        targetCard.before(draggedSettingCard);
    }

    const newCards = Array.from(grid.querySelectorAll('.setting-card'));
    const orders = {};
    newCards.forEach((card, index) => {
        const key = card.dataset.key;
        if (key) {
            orders[key] = index;
            keyOrders[key] = index;
        }
    });

    const container = document.getElementById('settings_container');
    if (!container) {
        return;
    }
    container.classList.add('saving-order');

    const formData = new FormData();
    formData.append('orders', JSON.stringify(orders));

    fetch('/api/plc-keys/order/bulk/', {
        method: 'POST',
        body: formData
    })
        .then(response => response.json())
        .then(result => {
            if (result.status !== 'ok') {
                console.error('Error saving settings order:', result.message);
            }
        })
        .catch(error => {
            console.error('Error saving settings order:', error);
        })
        .finally(() => {
            container.classList.remove('saving-order');
        });
});

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

function loadAlertConfigs() {
    fetch('/api/alert-configs/all/')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                alertConfigs = data.data || {};
                checkAllCardsForAlerts();
            }
        })
        .catch(err => console.error('Error loading alert configs:', err));
}

function checkAllCardsForAlerts() {
    const container = document.getElementById('settings_container');
    if (!container) return;
    
    const grid = container.querySelector('.settings-grid-large');
    if (!grid) return;
    
    const cards = grid.querySelectorAll('.setting-card');
    cards.forEach(card => {
        const key = card.dataset.key;
        if (key) {
            const rawValue = card.dataset.rawValue;
            if (rawValue !== undefined) {
                updateCardStyle(card, key, rawValue);
            } else {
                const valueEl = card.querySelector('.setting-card-value');
                if (valueEl) {
                    const displayValue = valueEl.textContent;
                    let rawValue = displayValue;
                    if (key === 'st') {
                        if (displayValue === 'MAN') rawValue = 'm';
                        else if (displayValue === 'AUTO') rawValue = 'a';
                    }
                    card.dataset.rawValue = rawValue;
                    updateCardStyle(card, key, rawValue);
                }
            }
        }
    });
}

function triggerAlert(key, type, value, config) {
    if (!config.alert_types) return;
    
    const keyName = getKeyName(key);
    const threshold = type === 'max' ? config.max_value : config.min_value;
    const message = `${keyName}: مقدار ${value} از ${type === 'max' ? 'حداکثر' : 'حداقل'} (${threshold}) عبور کرد`;
    
    if (config.alert_types.local) {
        showLocalAlert(message);
    }
    
    if (config.alert_types.sms) {
        console.log('SMS alert would be sent:', message);
    }
    
    if (config.alert_types.email) {
        console.log('Email alert would be sent:', message);
    }
}

function showLocalAlert(message) {
    const notification = document.getElementById('local_alert_notification');
    const messageEl = document.getElementById('local_alert_message');
    if (notification && messageEl) {
        messageEl.textContent = message;
        notification.style.display = 'block';
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        setTimeout(() => {
            closeLocalAlert();
        }, 5000);
    }
}

function closeLocalAlert() {
    const notification = document.getElementById('local_alert_notification');
    if (notification) {
        notification.classList.remove('show');
        setTimeout(() => {
            notification.style.display = 'none';
        }, 300);
    }
}

function updatePreview() {
    const colorMax = document.getElementById('alert_config_color_max').value;
    const colorMin = document.getElementById('alert_config_color_min').value;
    const previewMax = document.getElementById('preview_max');
    const previewMin = document.getElementById('preview_min');
    
    if (previewMax) {
        previewMax.style.background = `linear-gradient(135deg, transparent 0%, ${colorMax} 100%)`;
        previewMax.style.border = `1px solid ${colorMax}`;
    }
    
    if (previewMin) {
        previewMin.style.background = `linear-gradient(135deg, transparent 0%, ${colorMin} 100%)`;
        previewMin.style.border = `1px solid ${colorMin}`;
    }
}

function openAlertConfigModal(key) {
    const modal = document.getElementById('alert_config_modal');
    const keyInput = document.getElementById('alert_config_key');
    const minInput = document.getElementById('alert_config_min');
    const maxInput = document.getElementById('alert_config_max');
    const colorMaxInput = document.getElementById('alert_config_color_max');
    const colorMinInput = document.getElementById('alert_config_color_min');
    const localCheckbox = document.getElementById('alert_type_local');
    const smsCheckbox = document.getElementById('alert_type_sms');
    const emailCheckbox = document.getElementById('alert_type_email');
    
    if (!modal) return;
    
    keyInput.value = key;
    
    fetch(`/api/alert-config/?key=${encodeURIComponent(key)}`)
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                const config = data.data;
                minInput.value = config.min_value !== null ? config.min_value : '';
                maxInput.value = config.max_value !== null ? config.max_value : '';
                colorMaxInput.value = config.color_max || '#ff4444';
                colorMinInput.value = config.color_min || '#ff8800';
                
                localCheckbox.checked = config.alert_types.local || false;
                smsCheckbox.checked = config.alert_types.sms || false;
                emailCheckbox.checked = config.alert_types.email || false;
            }
            updatePreview();
        })
        .catch(err => {
            console.error('Error loading alert config:', err);
            minInput.value = '';
            maxInput.value = '';
            colorMaxInput.value = '#ff4444';
            colorMinInput.value = '#ff8800';
            localCheckbox.checked = false;
            smsCheckbox.checked = false;
            emailCheckbox.checked = false;
            updatePreview();
        });
    
    modal.style.display = 'flex';
    setTimeout(() => modal.classList.add('active'), 10);
}

function closeAlertConfigModal() {
    const modal = document.getElementById('alert_config_modal');
    if (modal) {
        modal.classList.remove('active');
        setTimeout(() => {
            modal.style.display = 'none';
        }, 300);
    }
}

function saveAlertConfig() {
    const key = document.getElementById('alert_config_key').value;
    const minValue = document.getElementById('alert_config_min').value;
    const maxValue = document.getElementById('alert_config_max').value;
    const colorMax = document.getElementById('alert_config_color_max').value;
    const colorMin = document.getElementById('alert_config_color_min').value;
    const localChecked = document.getElementById('alert_type_local').checked;
    const smsChecked = document.getElementById('alert_type_sms').checked;
    const emailChecked = document.getElementById('alert_type_email').checked;
    
    const alertTypes = {
        local: localChecked,
        sms: smsChecked,
        email: emailChecked
    };
    
    const formData = new FormData();
    formData.append('key', key);
    formData.append('min_value', minValue || '');
    formData.append('max_value', maxValue || '');
    formData.append('color_max', colorMax);
    formData.append('color_min', colorMin);
    formData.append('alert_types', JSON.stringify(alertTypes));
    
    fetch('/api/alert-config/save/', {
        method: 'POST',
        body: formData
    })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                alertConfigs[key] = {
                    min_value: data.data.min_value,
                    max_value: data.data.max_value,
                    color_max: data.data.color_max,
                    color_min: data.data.color_min,
                    alert_types: data.data.alert_types
                };
                
                checkAllCardsForAlerts();
                
                closeAlertConfigModal();
                showLocalAlert('تنظیمات با موفقیت ذخیره شد');
            } else {
                showLocalAlert('خطا: ' + (data.message || 'خطا در ذخیره تنظیمات'));
            }
        })
        .catch(err => {
            console.error('Error saving alert config:', err);
            showLocalAlert('خطا در ارتباط با سرور');
        });
}

document.addEventListener('DOMContentLoaded', function() {
    loadKeyNames();
    loadAlertConfigs();
    if (typeof PLC_ID !== 'undefined') {
        connectSSE();
    }
    initSettingCardsDrag();
    
    const modal = document.getElementById('alert_config_modal');
    if (modal) {
        modal.addEventListener('click', function(e) {
            if (e.target === modal) {
                closeAlertConfigModal();
            }
        });
    }
    
    document.addEventListener('click', function(e) {
        const card = e.target.closest('.setting-card');
        if (card && card.closest('#settings_container')) {
            const timeDiff = Date.now() - mouseDownTime;
            const distance = Math.sqrt(
                Math.pow(e.clientX - mouseDownPos.x, 2) + 
                Math.pow(e.clientY - mouseDownPos.y, 2)
            );
            
            if (!wasDragged && timeDiff < 300 && distance < 5) {
                const key = card.dataset.key;
                if (key) {
                    e.preventDefault();
                    e.stopPropagation();
                    openAlertConfigModal(key);
                }
            }
        }
    });
});

window.addEventListener('beforeunload', function() {
    if (eventSource) {
        eventSource.close();
    }
});
