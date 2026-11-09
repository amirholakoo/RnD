//Progress();
let eventSource = null;
let keyNames = {};
let keyOrders = {};
let keyMeta = {};
let alertConfigs = {};
let alertStates = {};

function loadKeyNames() {
    fetch('/api/plc-keys/')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                keyNames = {};
                keyOrders = {};
                keyMeta = {};
                data.data.forEach(item => {
                    keyNames[item.key] = item.fa_name || item.name || item.key;
                    if (typeof item.order_index === 'number') {
                        keyOrders[item.key] = item.order_index;
                    }
                    keyMeta[item.key] = {
                        live_background: item.live_background || false,
                        value_max: item.value_max != null ? item.value_max : 100
                    };
                });
                applySettingsOrder();
                initSettingCardsDrag();
                checkAllCardsForAlerts();
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

function hexToRgba(hex, alpha) {
    const m = String(hex).match(/^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i);
    return m ? `rgba(${parseInt(m[1],16)},${parseInt(m[2],16)},${parseInt(m[3],16)},${alpha})` : hex;
}


// // update status
// function UpdateStatus(card,key,value) {
//     let name = card.querySelector('.setting-card-key').innerHTML;
//     let target_name = name.split('_')[1]
//     for (const x of document.querySelector(".settings-grid-large").children) {
//         if (x.dataset.key.toLowerCase() == target_name.toLowerCase()) {
//             console.log("================",x,"================");
//             let status_card = x.querySelector(".card-status-bar");
//             if (!status_card) {
//                 let status_html_code = `
//                 <div class="card-status-bar"></div>
//                 `
//                 x.insertAdjacentHTML("afterbegin",status_html_code);
//             }

//             card.remove();
//         }
//     }
// }


// Update card style based on key and value
function updateCardStyle(card, key, value) {
    Progress(false);
    const numValue = parseInt(value);
    const isNumeric = !isNaN(numValue) && isFinite(value);
    let alertColor = null;
    if (document.querySelector(".svg_map")) {
        update_svg_map(key, value);
    }
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
        
        if (config.max_value != null && numValue > config.max_value) {
            alertColor = config.color_max || '#ff8800';
            alertType = config.max_value != null ? 'max_2' : 'max';
            alertTriggered = true;
        }
        if (config.max_value_2 != null && numValue > config.max_value_2) {
            alertColor = config.color_max_2 || '#ff4444';
            alertType = 'max';
            alertTriggered = true;
        }
        if (config.min_value != null && numValue < config.min_value) {
            alertColor = config.color_min || '#ff8800';
            alertType = config.min_value != null ? 'min_2' : 'min';
            alertTriggered = true;
        }
        if (config.min_value_2 != null && numValue < config.min_value_2) {
            alertColor = config.color_min_2 || '#ff4444';
            alertType = 'min';
            alertTriggered = true;
        }
        
        const hasLiveBg = keyMeta[key] && keyMeta[key].live_background;
        if (!hasLiveBg) {
            if (alertColor) {
                card.style.background = `linear-gradient(135deg, transparent 0%, ${alertColor} 100%)`;
                card.style.border = `1px solid ${alertColor}`;
                
            } else {
                card.style.background = '';
                card.style.border = '';
            }
        } else {
            card.style.background = '';
            card.style.border = alertColor ? `1px solid ${alertColor}` : '';
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
    
    const valueEl = card.querySelector('.setting-card-value');
    if (valueEl && keyMeta[key]) {
        const meta = keyMeta[key];
        if (meta.live_background && isNumeric) {
            const maxVal = meta.value_max || 100;
            const percent = Math.min(100, Math.max(0, (numValue / maxVal) * 100));
            const fillColor = alertColor ? hexToRgba(alertColor, 0.3) : 'rgba(76,175,80,0.3)';
            valueEl.parentElement.style.background = `linear-gradient(to top, ${fillColor} 0%, ${fillColor} ${percent}%, transparent ${percent}%)`;
            valueEl.parentElement.classList.add('live-background');
        } else {
            valueEl.parentElement.classList.remove('live-background');
            if (!alertColor) valueEl.parentElement.style.background = '';
        }
    }

    if (key.toLowerCase().includes("status")) {
        //UpdateStatus(card,key,value);
        let status_card = card.querySelector(".card-status-bar");
        if (!status_card) {
            let status_html_code = `
            <div class="card-status-bar"></div>
            `
            card.insertAdjacentHTML("afterbegin",status_html_code);
        }
        status_card = card.querySelector(".card-status-bar");
        if(String(value)=="3") {
            status_card.classList.add("error");
            card.querySelector('.setting-card-value').innerHTML = "RRROR";
        }
        else if(String(value)=="4") {
            status_card.classList.add("run");
            card.querySelector('.setting-card-value').innerHTML = "RUN";
        }
        else if(String(value)=="5") {
            status_card.classList.add("remote");
            card.querySelector('.setting-card-value').innerHTML = "REMOTE";
        }
        else if(String(value)=="6") {
            status_card.classList.add("local");
            card.querySelector('.setting-card-value').innerHTML = "LOCAL";
        }
        else if(String(value)=="7") {
            status_card.classList.add("interlock");
            card.querySelector('.setting-card-value').innerHTML = "INTER LOCK";
        }
        else {
            card.querySelector('.setting-card-value').innerHTML = "No Connection";
            status_card.classList.remove("error");
            status_card.classList.remove("run");
            status_card.classList.remove("remote");
            status_card.classList.remove("local");
            status_card.classList.remove("interlock");
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


let tcp_server_status = document.querySelector(".tcp_server_status");
let tcp_client_status = document.querySelector(".tcp_client_status");

function updateSettings(data) {
    const container = document.getElementById('settings_container');
    const timeEl = document.getElementById('last_update_time');

    if (data.plc_status == "ERROR") {
        tcp_client_status.classList.remove("live-indicator");
        tcp_client_status.children[0].classList.add("ofline-dot");
        tcp_client_status.children[1].innerHTML = "آفلاین";
        if (document.querySelector(".svg_map")) {
            let status_bar = document.querySelector(".status_bar");
            status_bar.children[1].classList.remove("online")
        }
    } else {
        tcp_client_status.classList.add("live-indicator");
        tcp_client_status.children[0].classList.remove("ofline-dot");
        tcp_client_status.children[1].innerHTML = "زنده";
        if (document.querySelector(".svg_map")) {
            let status_bar = document.querySelector(".status_bar");
            status_bar.children[1].classList.add("online")
        }
    }

    if (data.server_status == "ERROR") {
        tcp_server_status.classList.remove("online");
        if (document.querySelector(".svg_map")) {
            let status_bar = document.querySelector(".status_bar");
            status_bar.children[0].classList.remove("online")
        }
    } else {
        tcp_server_status.classList.add("online");
        if (document.querySelector(".svg_map")) {
            let status_bar = document.querySelector(".status_bar");
            status_bar.children[0].classList.add("online")
        }
    }
    
    if (!data.setting || Object.keys(data.setting).length === 0) {
        return;
    }
    if (timeEl) {
        timeEl.textContent = formatTimestamp(data.LastUpdate);
    }
    let grid = container.querySelector('.settings-grid-large');
    
    if (!grid) {
        container.innerHTML = '<div class="settings-grid-large"></div>';
        grid = container.querySelector('.settings-grid-large');
    }
    
    for (const [key, value] of Object.entries(data.setting)) {
        if (!key.includes("warning")) {
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
                newCard.dataset.name = "name";
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
            AM_alert({text:"فرمت اطلاعات ارسالی اشتباه است",status:"error"})
        }
    };
    
    eventSource.onerror = function(err) {
        console.error('SSE Error:', err);
        AM_alert({text: "لطفاً اتصال شبکه و وضعیت سرور را بررسی کنید. تلاش برای اتصال مجدد...",status:"error"})
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
    let threshold, label;
    if (type === 'max') {
        threshold = config.max_value_2;
        label = 'حداکثر ۲';
    } else if (type === 'max_2') {
        threshold = config.max_value;
        label = 'حداکثر';
    } else if (type === 'min') {
        threshold = config.min_value_2;
        label = 'حداقل ۲';
    } else {
        threshold = config.min_value;
        label = 'حداقل';
    }
    const message = `${keyName}: مقدار ${value} از ${label} (${threshold}) عبور کرد`;
    
    if (config.alert_types.local) {
        AM_alert({text:message,status:"error"})
    }
    
    if (config.alert_types.sms) {
        console.log('SMS alert would be sent:', message);
        AM_alert({text:"در حال بروزرسانی",status:"info"})
    }
    
    if (config.alert_types.email) {
        console.log('Email alert would be sent:', message);
        AM_alert({text:"در حال بروزرسانی",status:"info"})
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
    const colorMax2 = document.getElementById('alert_config_color_max_2').value;
    const colorMin = document.getElementById('alert_config_color_min').value;
    const colorMin2 = document.getElementById('alert_config_color_min_2').value;
    const previewMax = document.getElementById('preview_max');
    const previewMax2 = document.getElementById('preview_max_2');
    const previewMin = document.getElementById('preview_min');
    const previewMin2 = document.getElementById('preview_min_2');
    
    if (previewMax) {
        previewMax.style.background = `linear-gradient(135deg, transparent 0%, ${colorMax} 100%)`;
        previewMax.style.border = `1px solid ${colorMax}`;
    }
    if (previewMax2) {
        previewMax2.style.background = `linear-gradient(135deg, transparent 0%, ${colorMax2} 100%)`;
        previewMax2.style.border = `1px solid ${colorMax2}`;
    }
    if (previewMin2) {
        previewMin2.style.background = `linear-gradient(135deg, transparent 0%, ${colorMin2} 100%)`;
        previewMin2.style.border = `1px solid ${colorMin2}`;
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
    const min2Input = document.getElementById('alert_config_min_2');
    const maxInput = document.getElementById('alert_config_max');
    const max2Input = document.getElementById('alert_config_max_2');
    const colorMaxInput = document.getElementById('alert_config_color_max');
    const colorMax2Input = document.getElementById('alert_config_color_max_2');
    const colorMinInput = document.getElementById('alert_config_color_min');
    const colorMin2Input = document.getElementById('alert_config_color_min_2');
    const localCheckbox = document.getElementById('alert_type_local');
    const smsCheckbox = document.getElementById('alert_type_sms');
    const emailCheckbox = document.getElementById('alert_type_email');
    const liveBgCheckbox = document.getElementById('alert_config_live_background');
    const valueMaxInput = document.getElementById('alert_config_value_max');
    
    if (!modal) return;
    
    keyInput.value = key;
    
    if (keyMeta[key]) {
        liveBgCheckbox.checked = keyMeta[key].live_background || false;
        valueMaxInput.value = keyMeta[key].value_max != null ? keyMeta[key].value_max : 100;
    } else {
        liveBgCheckbox.checked = false;
        valueMaxInput.value = 100;
    }
    
    fetch(`/api/alert-config/?key=${encodeURIComponent(key)}`)
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                const config = data.data;
                minInput.value = config.min_value != null ? config.min_value : '';
                min2Input.value = config.min_value_2 != null ? config.min_value_2 : '';
                maxInput.value = config.max_value != null ? config.max_value : '';
                max2Input.value = config.max_value_2 != null ? config.max_value_2 : '';
                colorMaxInput.value = config.color_max || '#ff4444';
                colorMax2Input.value = config.color_max_2 || '#ff8800';
                colorMinInput.value = config.color_min || '#ff4444';
                colorMin2Input.value = config.color_min_2 || '#ffaa00';
                
                localCheckbox.checked = config.alert_types.local || false;
                smsCheckbox.checked = config.alert_types.sms || false;
                emailCheckbox.checked = config.alert_types.email || false;
            }
            updatePreview();
        })
        .catch(err => {
            console.error('Error loading alert config:', err);
            minInput.value = '';
            min2Input.value = '';
            maxInput.value = '';
            max2Input.value = '';
            colorMaxInput.value = '#ff4444';
            colorMax2Input.value = '#ff8800';
            colorMinInput.value = '#ff4444';
            colorMin2Input.value = '#ffaa00';
            localCheckbox.checked = false;
            smsCheckbox.checked = false;
            emailCheckbox.checked = false;
            liveBgCheckbox.checked = false;
            valueMaxInput.value = 100;
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
    const minValue2 = document.getElementById('alert_config_min_2').value;
    const maxValue = document.getElementById('alert_config_max').value;
    const maxValue2 = document.getElementById('alert_config_max_2').value;
    const colorMax = document.getElementById('alert_config_color_max').value;
    const colorMax2 = document.getElementById('alert_config_color_max_2').value;
    const colorMin = document.getElementById('alert_config_color_min').value;
    const colorMin2 = document.getElementById('alert_config_color_min_2').value;
    const liveBackgroundChecked = document.getElementById('alert_config_live_background').checked;
    const valueMax = document.getElementById('alert_config_value_max').value;
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
    formData.append('min_value_2', minValue2 || '');
    formData.append('max_value', maxValue || '');
    formData.append('max_value_2', maxValue2 || '');
    formData.append('color_max', colorMax);
    formData.append('color_max_2', colorMax2);
    formData.append('color_min', colorMin);
    formData.append('color_min_2', colorMin2);
    formData.append('alert_types', JSON.stringify(alertTypes));
    
    const keySettingsForm = new FormData();
    keySettingsForm.append('key', key);
    keySettingsForm.append('live_background', liveBackgroundChecked);
    keySettingsForm.append('value_max', valueMax || '100');
    
    Promise.all([
        fetch('/api/alert-config/save/', { method: 'POST', body: formData }).then(r => r.json()),
        fetch('/api/plc-keys/update-settings-by-key/', { method: 'POST', body: keySettingsForm }).then(r => r.json())
    ]).then(([alertRes, keyRes]) => {
        const data = alertRes;
        if (data.status === 'ok') {
                alertConfigs[key] = {
                    min_value: data.data.min_value,
                    min_value_2: data.data.min_value_2,
                    max_value: data.data.max_value,
                    max_value_2: data.data.max_value_2,
                    color_max: data.data.color_max,
                    color_max_2: data.data.color_max_2,
                    color_min: data.data.color_min,
                    color_min_2: data.data.color_min_2,
                    alert_types: data.data.alert_types
                };
                if (keyRes.status === 'ok' && keyMeta[key]) {
                    keyMeta[key].live_background = liveBackgroundChecked;
                    keyMeta[key].value_max = parseFloat(valueMax) || 100;
                }
                
                checkAllCardsForAlerts();
                
                closeAlertConfigModal();
                AM_alert({text:"تنظیمات با موفقیت ذخیره شد",status:"success"})
            } else {
                AM_alert({text:"خطا در ذخیره تنظیمات",status:"error"})
            }
        }).catch(err => {
            console.error('Error saving config:', err);
            AM_alert({text:"خطا در ذخیره تنظیمات",status:"error"})
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



// system status
const s1 = {
    0:"خاموش",
    1:"آبگیری",
    2:"انتظار دستور ساخت",
    3:"تزریق مواد 1",
    4:"گرم کردن",
    5:"سرد کردن",
    6:"تزریق سولفات",
    7:"تزریق رنگ",
    8:"تزریق akd",
    9:"سرد کردن 2",
    10:"آبگیری هدف",
    11:"ارسال",
    12:"آبگیری و شستشو",
    13:"ارسال آب شستشو",
    14:"پایان_عدم اجازه ارسال",
}
const s2 = {
    0:"خاموش",
    1:"آبگیری",
    2:"انتظار دستور ساخت",
    3:"تزریق مواد",
    4:"هم زدن",
    5:"آبگیری هدف",
    6:"همزدن - عدم اجازه ارسال",
    7:"ارسال",
    8:"آبگیری و شستشو",
    9:"ارسال آب شستشو",
}
const s3 = {
    0:"خاموش",
    1:"آبگیری",
    2:"انتظار دستور ساخت",
    3:"تزریق مواد",
    4:"هم زدن",
    5:"آبگیری هدف",
    6:"همزدن - عدم اجازه ارسال",
    7:"ارسال",
    8:"آبگیری و شستشو",
    9:"ارسال آب شستشو",
}

const s4 = {
    0:"خاموش",
    1:"آبگیری",
    2:"انتظار دستور ساخت",
    3:"تزریق مواد",
    4:"تخلیه ریتنشن",
    5:"همزدن - عدم اجازه ارسال",
    6:"آبگیری هدف",
    7:"همزدن",
    8:"ارسال",
    9:"آبگیری شستشو",
    10:"ارسال آب شستشو",
}

const latestPLCValues = new Map();
const deltaStates = new Map();
const wa_values = {};

function update_svg_map(key, value) {
    const numValue = parseFloat(value);
    let valuable = !isNaN(numValue) && isFinite(value);
    let rec = document.querySelector(`[rec_key="rec_${key}"]`);
    let text = document.querySelectorAll(`[text_key="text_${key}"]`);
    let img_target = document.querySelector(`[img_key="img_${key}"]`);
    let systemalarm = document.getElementById("systemalarm");
    let bg_rec = document.querySelector(`[bg_rec="${key}"]`);
    let emergency = document.querySelector(`[emergency_key="${key}"]`);

    const water_flow = [...document.querySelectorAll("[waterflow]")]
    .filter(obj =>
        obj.getAttribute("waterflow")
            .split(";")
            .map(key => key.trim())
            .includes(key)
    );

    const error_keys = [...document.querySelectorAll(`[error_keys]`)]
    .filter(obj =>
        obj.getAttribute("error_keys")
            .split(";")
            .map(key => key.trim())
            .includes(key)
    );

    let custome_color;
    const config = alertConfigs[key];

    if (img_target) img_target.classList.remove("svg_pulse");
    if (rec) rec.classList.remove("svg_pulse");

    if (key.toLowerCase().includes("status")) {
        if (img_target) {
            img_target.style.filter = "";
            if (String(value) == "3") {
                img_target.style.filter = "sepia(100%) hue-rotate(310deg) saturate(400%) brightness(100%)";
                // RED
            } else if (String(value) == "4") {
                img_target.style.filter = "sepia(100%) hue-rotate(65deg) saturate(300%) brightness(100%)";
                // GREEN
            } else if (String(value) == "5") {
                img_target.style.filter = "sepia(100%) hue-rotate(150deg) saturate(220%)  brightness(100%)";
                // SKY BLUE
            } else if (String(value) == "6") {
                img_target.style.filter = "sepia(105%) hue-rotate(0deg) saturate(200%) brightness(120%)";
                // YELLOW
            } else if (String(value) == "7") {
                img_target.style.filter = "sepia(100%) hue-rotate(250deg) saturate(200%)  brightness(90%)";
                // PURPLE
            }
            
            // اضافه کردن کلاس با requestAnimationFrame
            requestAnimationFrame(() => {
                img_target.classList.add("svg_pulse");
            });
        }
    }

    if (!valuable) return;
    latestPLCValues.set(key, numValue);
    UpdateWA(key, value);

    if (text) {
        text.forEach(t=>{
            t.classList.remove("svg_pulse")
            t.innerHTML = ({ s1, s2, s3, s4 }[key]?.[Number(value)] ?? ((Number(value)) + (t.getAttribute("unit_key")?" " + t.getAttribute("unit_key"):''))); // + (rec ? " %" : "") ;
            requestAnimationFrame(() => {
                t.classList.add("svg_pulse");
            });
        })
    }

    if (alertConfigs[key]) {
        
        if (config.min_value_2 != null && config.max_value_2 && numValue > config.min_value_2 && numValue < config.max_value_2) {
            custome_color = 'lime';
        }
        if (config.max_value != null && numValue > config.max_value) {
            custome_color = config.color_max || '#ff8800';
        }
        if (config.max_value_2 != null && numValue > config.max_value_2) {
            custome_color = config.color_max_2 || '#ff4444';
        }
        if (config.min_value != null && numValue < config.min_value) {
            custome_color = config.color_min || '#ff8800';
        }
        if (config.min_value_2 != null && numValue < config.min_value_2) {
            custome_color = config.color_min_2 || '#ff4444';
        }
    }
    if(bg_rec) {
        // bg_rec.style.fill = "lime";
        if (String(value) == "1") {
            bg_rec.style.fill = "lime";
            if(custome_color) {
                bg_rec.style.fill = custome_color;
            }
        } else {
            bg_rec.style.fill = "#b3b3b3"
        }
    }
    if (rec) {
        resizeBarFromTop(rec, 0);
        // rec.style.fill = "lime";
        requestAnimationFrame(() => {
            rec.classList.add("svg_pulse");
        });
        if(custome_color) {
            rec.style.fill = custome_color;
        }
        if (numValue) {
            resizeBarFromTop(rec, numValue + 3);
        }
    }
    if (key == "t18") {
        if (String(value) == "1") {
            systemalarm.classList.add("infinite_pulse");
        } else {
            systemalarm.classList.remove("infinite_pulse");
        }
    }
    if (emergency) {
        if(String(value)=="1") {
            emergency.style.opacity= "1";
        } else {
            emergency.style.opacity= ".02";
        }
    }
    
    if (water_flow) {
        for (const x of water_flow) {
            let water_dependency = x.getAttribute("waterflow").split(";")
            let pipe_on = false;
            water_dependency.map(dependency_key => {
                if(dependency_key && wa_values[dependency_key]=="1") {
                    x.classList.add("water-flow");
                    pipe_on=true;
                }
            })
            if(!pipe_on) {
                x.classList.remove("water-flow")
            }
        }
        
    }

    let error_text = document.getElementById("error_text");
    if (error_keys) {
        for (const x of error_keys) {
            let error_dependency = x.getAttribute("error_keys").split(";")
            let error_on = false;
            error_dependency.map(dependency_key => {
                if(dependency_key && wa_values[dependency_key]=="1") {
                    x.classList.add("error_pulse");
                    error_on=true;
                    x.onmouseenter = () => {
                        error_text.textContent="";
                        error_text.textContent += keyNames[dependency_key] + ", "
                        let rect = x.getBoundingClientRect();
                        error_text.style.left = `${rect.left + rect.width / 2}px`;
                        error_text.style.top = `${rect.top - 10}px`;
                        error_text.style.transform = "translate(-50%, -100%)";

                        error_text.classList.add("active");
                    };
                    x.onmouseleave = () => {
                        error_text.textContent="";
                        error_text.classList.remove("active");
                    };
                }
            })
            if(!error_on) {
                x.classList.remove("error_pulse")
            }
        }
        
    }
}

DisplayStepsDetails();
setInterval(DisplayStepsDetails, 5000);


async function DisplayStepsDetails() {
    try {
        const response = await fetch("/api/materials/steps-details/");

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();

        function formatTime(seconds) {
            if (seconds === null || seconds === undefined) return "--";

            seconds = Number(seconds);
            if (!Number.isFinite(seconds)) return "--";

            const hours = 0; //Math.floor(seconds / 3600);
            const minutes = Math.floor(seconds / 60); //Math.floor((seconds % 3600) / 60);

            return hours > 0
                ? `${hours}:${String(minutes).padStart(2, "0")} h`
                : `${minutes}`;
        }

        document.querySelectorAll("[stepdetails]").forEach(obj => {
            const attr = obj.getAttribute("stepdetails");

            // پخت قبلی: p_s1_3_v
            if (attr.startsWith("p-")) {
                const [, tank, step, type] = attr.split("-");

                const previousStep = data?.[tank]?.previous?.[step];

                obj.classList.remove(
                    "step-active-positive",
                    "step-active-negetive"
                );

                if (!previousStep) {
                    obj.textContent = "--";
                    return;
                }

                const value = previousStep.v;
                const duration = previousStep.t;

                if (value === null || value === undefined ||
                    duration === null || duration === undefined) {
                    obj.textContent = "--";
                    return;
                }

                // مثال: 1500@22
                const minutes = Math.floor(Number(duration) / 60);

                obj.textContent = `${value}@${minutes}`;
                return;
            }

            // پخت فعلی: s1-3-v
            const [tank, step, type] = attr.split("-");
            const stepData = data?.[tank]?.[step];

            obj.classList.remove(
                "step-active-positive",
                "step-active-negetive"
            );

            if (!stepData) {
                obj.textContent = "--";
                return;
            }

            const value = stepData[type];

            if (value === undefined || value === null) {
                obj.textContent = "--";
                return;
            }

            if (type === "t") {
                obj.textContent = formatTime(value);

                if (stepData.active) {
                    obj.classList.add("step-active-positive");
                }
            } else {
                obj.textContent = value;

                if (stepData.active) {
                    const number = Number(value);

                    if (number > 0) {
                        obj.classList.add("step-active-positive");
                    } else if (number < 0) {
                        obj.classList.add("step-active-negetive");
                    }
                }
            }
        });

    } catch (error) {
        console.error("خطا در دریافت جزئیات مراحل مخزن:", error);
    }
}


function resizeBarFromTop(obj, value) {
    const maxHeight = parseFloat(obj.getAttribute('max_height'));
    const maxValue = parseFloat(obj.getAttribute('max_value')) || 100;

    const currentY = parseFloat(obj.getAttribute('y'));
    const currentHeight = parseFloat(obj.getAttribute('height'));

    let newHeight = (value / maxValue) * maxHeight;

    newHeight = Math.max(0, Math.min(newHeight, maxHeight));

    const heightDiff = newHeight - currentHeight;

    obj.setAttribute('height', newHeight);
    obj.setAttribute('y', currentY - heightDiff);

    
}
// warnings status configs

function UpdateWA(key, value) {
    if (!/^wa[1-9]-(?:[0-9]|1[0-5])$/.test(key)) return;

    wa_values[key] = String(value) === "1";

    document.querySelectorAll("[wa_key]").forEach(wa_target => {
        const parent = wa_target.parentElement;
        const keys = wa_target.getAttribute("wa_key")
            .split(";")
            .map(item => item.trim());

        const active_keys = keys.filter(item => {
            const clean_key = item.endsWith("*")
                ? item.slice(0, -1)
                : item;

            return wa_values[clean_key] === true;
        });

        const is_active = active_keys.length > 0;
        const is_handly = active_keys.some(item => item.endsWith("*"));

        parent.classList.toggle("active", is_active);
        parent.classList.toggle("handly-active", is_handly);

        let text = parent.querySelector(".handly-label");

        if (is_handly) {
            const circle = parent.querySelector("circle");

            if (!text && circle) {
                text = document.createElementNS(
                    "http://www.w3.org/2000/svg",
                    "text"
                );

                text.setAttribute("class", "handly-label");
                text.setAttribute("x", circle.getAttribute("cx"));
                text.setAttribute("y", circle.getAttribute("cy"));
                text.setAttribute("dx", "12");
                text.textContent = "دستی";

                parent.appendChild(text);
            }
        } else if (text) {
            text.remove();
        }
    });
}

// =========== flow of historical status ===========
const materialTankNames = {
    1: "نشاسته",
    2: "سولفات",
    3: "پک",
    4: "ریتنشن",
};

let selectedMaterialTank = null;
let historyRequestController = null;


function escapeHTML(value) {
    return String(value ?? "").replace(/[&<>"']/g, char => ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;",
    })[char]);
}


function formatDuration(seconds) {
    if (seconds === null || seconds === undefined) {
        return "--";
    }

    seconds = Number(seconds);

    if (!Number.isFinite(seconds) || seconds < 0) {
        return "--";
    }

    seconds = Math.floor(seconds);

    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    const time = [
        String(hours).padStart(2, "0"),
        String(minutes).padStart(2, "0"),
    ].join(":");

    return days > 0 ? `${days} روز ${time}` : time;
}


async function DisplayMaterialHistory(tank) {
    const panel = document.getElementById("historical_data_flow");

    if (!panel) return;

    selectedMaterialTank = Number(tank);

    const rangeInput = panel.querySelector(
        'input[name="historical_data_flow_range"]:checked'
    );

    const range = rangeInput
        ? rangeInput.id.replace("historical_data_flow_range_", "")
        : "48-h";

    const container = panel.querySelector(".data_flow_items");
    const title = panel.querySelector("h5");

    title.textContent =
        `تاریخچه ${materialTankNames[selectedMaterialTank] || "مخزن"} ( برای جزئیات کلیک کنید )`;

    panel.classList.add("active");

    container.innerHTML = `
        <div class="history-message">در حال دریافت اطلاعات...</div>
    `;

    // abort last request ( for example : when click other tank very fast )
    if (historyRequestController) {
        historyRequestController.abort();
    }

    const controller = new AbortController();
    historyRequestController = controller;

    try {
        const params = new URLSearchParams({
            tank: selectedMaterialTank,
            range: range,
        });

        const response = await fetch(
            `/api/materials/history/?${params.toString()}`,
            {
                method: "GET",
                signal: controller.signal,
            }
        );

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();

        // از نمایش پاسخ قدیمی جلوگیری می‌کنیم.
        if (selectedMaterialTank !== Number(data.tank)) return;

        if (!data.records.length) {
            container.innerHTML = `
                <div class="history-message">
                    در این بازه زمانی رکوردی وجود ندارد.
                </div>
            `;
            return;
        }

        container.innerHTML = "";

        data.records.forEach(record => {
            const itemHTML = `
                <div class="item ${record.is_active ? "active-material" : ""}" data-material-id="${record.id}">
                    <div class="material-number">
                        <strong>شماره مواد:</strong>
                        <span>${escapeHTML(record.material_number)}</span>
                    </div>

                    <div class="material-date">
                        <strong>تاریخ:</strong>
                        <span>${escapeHTML(record.date)}</span>
                    </div>

                    <div class="material-start">
                        <strong>شروع:</strong>
                        <span>${escapeHTML(record.start_time)}</span>
                    </div>

                    <div class="material-end">
                        <strong>پایان:</strong>
                        <span>${record.is_active
                            ? "در حال ساخت..."
                            : escapeHTML(record.end_time)}</span>
                    </div>

                    <div class="material-duration">
                        <strong>مدت ساخت (h:m):</strong>
                        <span>${formatDuration(record.duration_seconds)}</span>
                    </div>
                </div>
            `;

            container.insertAdjacentHTML("beforeend", itemHTML);
        });

        // نمایش جدیدترین رکوردها از سمت راست
        container.scrollTo({
            left: container.scrollWidth,
            behavior: "smooth",
        });

    } catch (error) {
        if (error.name === "AbortError") return;

        console.error("خطا در دریافت تاریخچه مواد:", error);

        container.innerHTML = `
            <div class="history-message">
                دریافت تاریخچه با خطا مواجه شد.
            </div>
        `;
    }
}


async function DisplayMaterialDetails(materialId) {
    const container = document.getElementById("material_details");

    if (!container) return;

    container.innerHTML = `
        <div class="history-message">در حال دریافت جزئیات ساخت...</div>
    `;

    try {
        const response = await fetch(
            `/api/materials/${materialId}/details/`
        );

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();

        const stepsHTML = data.steps.map((step, index) => {
            const change = step.weight_change;
            const changeNumber = Number(change);

            let changeClass = "weight-neutral";
            let changeText = "--";

            if (change !== null && change !== undefined &&
                Number.isFinite(changeNumber)) {

                if (changeNumber > 0) {
                    changeClass = "weight-positive";
                    changeText = `+${changeNumber}`;
                } else if (changeNumber < 0) {
                    changeClass = "weight-negative";
                    changeText = `${changeNumber}`;
                } else {
                    changeText = "0";
                }
            }

            const duration = formatDuration(step.duration_seconds);

            const hasTemperature = Array.isArray(step.temperature_keys)
                && step.temperature_keys.length > 0;

            const temperatureChange = Number(step.temperature_change);

            let temperatureClass = "weight-neutral";
            let temperatureText = "--";

            if (
                step.temperature_change !== null &&
                step.temperature_change !== undefined &&
                Number.isFinite(temperatureChange)
            ) {
                if (temperatureChange > 0) {
                    temperatureClass = "weight-positive";
                    temperatureText = `+${temperatureChange}`;
                } else if (temperatureChange < 0) {
                    temperatureClass = "weight-negative";
                    temperatureText = `${temperatureChange}`;
                } else {
                    temperatureText = "0";
                }
            }

            return `
                <div class="detail-step ${step.end ? "" : "current-step"}">
                    <div class="detail-step-header">
                        <strong>${index + 1}. ${escapeHTML(step.name || "--")}</strong>
                        ${!step.end
                            ? '<span class="live-label">مرحله جاری</span>'
                            : ""}
                    </div>

                    <div class="detail-row">
                        <div class="item">
                            <span>شروع</span>
                            <span>${escapeHTML(step.start_display || "--")}</span>
                        </div>
                        <div class="item">
                            <span>پایان</span>
                            <span>${escapeHTML(step.end_display || "در حال ساخت")}</span>
                        </div>
                    </div>

                    <div class="detail-row">
                        <span>مدت مرحله (h:m)</span>
                        <strong>${escapeHTML(duration)}</strong>
                    </div>

                    <div class="detail-row" style="border: 0;">
                        <div class="item">
                            <span>وزن شروع</span>
                            <span>${escapeHTML(step.weight_start ?? "--")}</span>
                        </div>
                        <div class="item">
                            <span>وزن پایان</span>
                            <span>${escapeHTML(step.weight_end ?? "--")}</span>
                        </div>
                    </div>

                    <div class="detail-row">
                        <span>تغییر وزن</span>
                        <strong class="${changeClass}">
                            ${escapeHTML(changeText)}
                        </strong>
                    </div>

                    ${hasTemperature ? `
                        <div class="detail-row" style="border: 0;">
                            <div class="item">
                                <span>دمای شروع</span>
                                <span>${escapeHTML(step.temperature_start ?? "--")}</span>
                            </div>
                            <div class="item">
                                <span>دمای پایان</span>
                                <span>${escapeHTML(step.temperature_end ?? "--")}</span>
                            </div>
                        </div>

                        <div class="detail-row">
                            <span>اختلاف دما</span>
                            <strong class="${temperatureClass}">
                                ${escapeHTML(temperatureText)} °C
                            </strong>
                        </div>
                    ` : ""}
                </div>
            `;
        }).join("");

        container.innerHTML = `
            <div class="details-title">
                <h5>جزئیات ساخت ${escapeHTML(data.material_number || "--")}</h5>
                <button type="button" class="close-details">×</button>
            </div>

            <div class="details-summary">
                <span>تعداد مراحل: ${data.steps.length}</span>
                <span>مدت کل (h:m): ${formatDuration(data.duration_seconds)}</span>
                <span>${data.is_active ? "در حال اجرا" : "پایان‌یافته"}</span>
            </div>

            <div class="detail-steps">
                ${stepsHTML || '<p>جزئیاتی برای این ساخت ثبت نشده است.</p>'}
            </div>
        `;

        container.classList.add("active");

        container.querySelector(".close-details").addEventListener("click", () => {
            container.classList.remove("active");
        });

    } catch (error) {
        console.error("خطا در دریافت جزئیات ساخت:", error);

        container.innerHTML = `
            <div class="history-message">
                دریافت جزئیات ساخت با خطا مواجه شد.
            </div>
        `;
    }
}


// کلیک روی کارت‌های تاریخچه
document.addEventListener("click", function (event) {
    const item = event.target.closest(
        "#historical_data_flow [data-material-id]"
    );

    if (item) {
        DisplayMaterialDetails(item.getAttribute("data-material-id"));
    }
});

// کلیک روی مخزن‌های SVG
document.addEventListener("click", function (event) {
    const tank = event.target.closest("[data-tank]");

    if (tank) {
        DisplayMaterialHistory(tank.getAttribute("data-tank"));
    }
});


// تغییر بازه زمانی
document.querySelectorAll(
        '#historical_data_flow input[name="historical_data_flow_range"]'
    )
    .forEach(input => {
        input.addEventListener("change", function () {
            if (selectedMaterialTank !== null) {
                DisplayMaterialHistory(selectedMaterialTank);
            }
        });
    });


// بستن پنل
document.querySelector(
    '#historical_data_flow [data-flow]'
)?.addEventListener("click", function () {
    document
        .getElementById("historical_data_flow")
        .classList.remove("active");
});