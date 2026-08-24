let eventSource = null;
let keyNames = {};
let keyOrders = {};
let keyGroups = {};
let keyMeta = {};
let alertConfigs = {};
let alertStates = {};
let currentSetting = {};
if (typeof initialSetting !== 'undefined') currentSetting = initialSetting;

function loadKeyNames() {
    fetch('/api/plc-keys/')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                keyNames = {};
                keyOrders = {};
                keyGroups = {};
                keyMeta = {};
                data.data.forEach(item => {
                    keyNames[item.key] = item.fa_name || item.name || item.key;
                    if (typeof item.order_index === 'number') {
                        keyOrders[item.key] = item.order_index;
                    }
                    if (item.group_id) {
                        keyGroups[item.key] = {
                            groupId: item.group_id,
                            groupName: item.group_name || 'گروه',
                            groupOrder: item.group_order != null ? item.group_order : 0,
                            orderInGroup: item.order_in_group != null ? item.order_in_group : 0
                        };
                    } else {
                        keyGroups[item.key] = null;
                    }
                    keyMeta[item.key] = {
                        live_background: item.live_background || false,
                        value_max: item.value_max != null ? item.value_max : 100
                    };
                });
                buildSettingsGrid();
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
    if (!value || value == '') {
        return '-';
    }
    return value;
}

function hexToRgba(hex, alpha) {
    const m = String(hex).match(/^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i);
    return m ? `rgba(${parseInt(m[1],16)},${parseInt(m[2],16)},${parseInt(m[3],16)},${alpha})` : hex;
}

// Update card style based on key and value
function updateCardStyle(card, key, value) {
    const numValue = parseFloat(value);
    const isNumeric = !isNaN(numValue) && isFinite(value);
    let alertColor = null;
    
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
    const timeEl = document.getElementById('last_update_time');
    if (timeEl) {
        timeEl.textContent = formatTimestamp(data.LastUpdate);
    }
    if (!data.setting || Object.keys(data.setting).length === 0) {
        return;
    }
    const keysBefore = Object.keys(currentSetting);
    currentSetting = data.setting;
    const keysNow = Object.keys(currentSetting);
    const sameKeySet = keysBefore.length === keysNow.length && keysNow.every(k => keysBefore.includes(k));
    const container = document.getElementById('settings_container');
    if (sameKeySet && container) {
        for (const [key, value] of Object.entries(currentSetting)) {
            const card = container.querySelector(`.setting-card[data-key="${key}"]`);
            if (!card) continue;
            const displayValue = formatDisplayValue(key, value);
            const valueEl = card.querySelector('.setting-card-value');
            if (valueEl) {
                const valueChanged = valueEl.textContent !== String(displayValue);
                if (valueChanged) {
                    if(!displayValue || displayValue == '') {
                        valueEl.textContent = '-';
                    }
                    card.classList.add('value-changed');
                    setTimeout(() => card.classList.remove('value-changed'), 500);
                }
            }
            card.dataset.rawValue = value;
            updateCardStyle(card, key, value);
        }
    } else {
        buildSettingsGrid();
    }
    initSettingCardsDrag();
    checkAllCardsForAlerts();
}

// function getOrderedSlots() {
//     const keys = Object.keys(currentSetting);
//     if (!keys.length) return [];
//     const orderA = (a, b) => {
//         const oa = keyOrders.hasOwnProperty(a) ? keyOrders[a] : 9999;
//         const ob = keyOrders.hasOwnProperty(b) ? keyOrders[b] : 9999;
//         if (oa !== ob) return oa - ob;
//         const ga = keyGroups[a];
//         const gb = keyGroups[b];
//         const oiga = ga ? ga.orderInGroup : 0;
//         const oigb = gb ? gb.orderInGroup : 0;
//         if (ga && gb && ga.groupId === gb.groupId) return oiga - oigb;
//         return (a || '').localeCompare(b || '');
//     };
//     keys.sort(orderA);
//     const slots = [];
//     let i = 0;
//     while (i < keys.length) {
//         const k = keys[i];
//         const g = keyGroups[k];
//         if (g) {
//             const groupKeys = [k];
//             while (i + 1 < keys.length && keyGroups[keys[i + 1]] && keyGroups[keys[i + 1]].groupId === g.groupId) {
//                 i++;
//                 groupKeys.push(keys[i]);
//             }
//             slots.push({ type: 'group', groupId: g.groupId, groupName: g.groupName, groupOrder: g.groupOrder, keys: groupKeys });
//         } else {
//             slots.push({ type: 'card', keys: [k] });
//         }
//         i++;
//     }
//     console.log(slots);
//     return slots;
// }

function getOrderedSlots() {
    const keys = Object.keys(currentSetting);
    if (!keys.length) return [];

    const orderA = (a, b) => {

        const ga = keyGroups[a]; // {groupId, groupName, order, orderInGroup}
        const gb = keyGroups[b];

        const oa = keyOrders[a] ?? 9999; // order_index
        const ob = keyOrders[b] ?? 9999;

        const goa = ga ? ga.groupOrder : null;
        const gob = gb ? gb.groupOrder : null;

        const oiga = ga ? ga.orderInGroup : 0;
        const oigb = gb ? gb.orderInGroup : 0;

        // both NOT in group → order_index
        if (!ga && !gb) {
            return oa - ob;
        }

        // both in SAME group → order_in_group
        if (ga && gb && ga.groupId === gb.groupId) {
            return oiga - oigb;
        }

        // both in DIFFERENT groups → group.order
        if (ga && gb) {
            return goa - gob;
        }

        // one grouped, one not
        if (ga && !gb) {
            return goa - ob;
        }

        if (!ga && gb) {
            return oa - gob;
        }

        return (a || '').localeCompare(b || '');
    };

    keys.sort(orderA);

    const slots = [];
    let i = 0;

    while (i < keys.length) {
        const k = keys[i];
        const g = keyGroups[k];

        if (g) {
            const groupKeys = [k];

            while (
                i + 1 < keys.length &&
                keyGroups[keys[i + 1]] &&
                keyGroups[keys[i + 1]].groupId === g.groupId
            ) {
                i++;
                groupKeys.push(keys[i]);
            }

            slots.push({
                type: 'group',
                groupId: g.groupId,
                groupName: g.groupName,
                groupOrder: g.groupOrder,
                keys: groupKeys
            });

        } else {
            slots.push({
                type: 'card',
                keys: [k]
            });
        }

        i++;
    }

    console.log(slots);
    return slots;
}

function escapeHtml(s) {
    if (s == null) return '';
    const div = document.createElement('div');
    div.textContent = s;
    return div.innerHTML;
}

function buildOneCard(key) {
    const value = currentSetting[key];
    const displayValue = formatDisplayValue(key, value);
    const card = document.createElement('div');
    card.className = 'setting-card';
    card.dataset.key = key;
    card.dataset.rawValue = value;
    card.setAttribute('draggable', 'true');
    card.setAttribute('title', 'کلیک برای تنظیمات هشدار');
    card.innerHTML = `
        <div class="setting-card-key">${escapeHtml(getKeyName(key))}</div>
        <div class="setting-card-value">${escapeHtml(String(displayValue))}</div>
    `;
    updateCardStyle(card, key, value);
    return card;
}

function buildSettingsGrid() {
    const container = document.getElementById('settings_container');
    if (!container) return;
    if (!Object.keys(currentSetting).length) {
        const grid = container.querySelector('.settings-grid-large');
        if (grid) grid.innerHTML = '';
        return;
    }
    const noSettingsEl = container.querySelector('.no-settings-large');
    if (noSettingsEl) noSettingsEl.remove();
    const slots = getOrderedSlots();
    const grid = document.createElement('div');
    grid.className = 'settings-grid-large';
    slots.forEach(slot => {
        if (slot.type === 'group') {
            const box = document.createElement('div');
            box.className = 'setting-group';
            // box.setAttribute("order",slot.groupOrder);
            box.dataset.groupId = String(slot.groupId);
            const header = document.createElement('div');
            header.className = 'setting-group-header';
            const nameSpan = document.createElement('span');
            nameSpan.className = 'setting-group-name';
            nameSpan.textContent = slot.groupName;
            nameSpan.title = 'کلیک برای تغییر نام';
            const actions = document.createElement('div');
            actions.className = 'setting-group-actions';
            const addBtn = document.createElement('button');
            addBtn.type = 'button';
            addBtn.className = 'setting-group-btn add-key-btn';
            addBtn.title = 'افزودن کلید به گروه';
            addBtn.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>';
            addBtn.addEventListener('click', (e) => { e.stopPropagation(); openAddKeyToGroup(slot.groupId); });
            actions.appendChild(addBtn);
            header.appendChild(nameSpan);
            header.appendChild(actions);
            box.appendChild(header);
            const keysWrap = document.createElement('div');
            keysWrap.className = 'setting-group-keys';
            slot.keys.forEach(key => {
                const cardWrap = document.createElement('div');
                cardWrap.className = 'setting-group-card-wrap';
                const card = buildOneCard(key);
                const removeBtn = document.createElement('button');
                removeBtn.type = 'button';
                removeBtn.className = 'setting-group-remove-key';
                removeBtn.title = 'خروج از گروه';
                removeBtn.innerHTML = '×';
                removeBtn.addEventListener('click', (e) => { e.stopPropagation(); removeKeyFromGroup(key); });
                cardWrap.appendChild(card);
                cardWrap.appendChild(removeBtn);
                keysWrap.appendChild(cardWrap);
            });
            box.appendChild(keysWrap);
            nameSpan.addEventListener('click', (e) => { e.stopPropagation(); openRenameGroup(slot.groupId, slot.groupName); });
            grid.appendChild(box);
        } else {
            const card = buildOneCard(slot.keys[0]);
            grid.appendChild(card);
        }
    });
    const oldGrid = container.querySelector('.settings-grid-large');
    if (oldGrid) oldGrid.replaceWith(grid); else container.appendChild(grid);
    initSettingCardsDrag();
}

function initSettingCardsDrag() {
    const grid = document.querySelector('#settings_container .settings-grid-large');
    if (!grid) return;
    grid.querySelectorAll('.setting-card').forEach(card => card.setAttribute('draggable', 'true'));
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
    if (card && card.closest('#settings_container')) card.classList.remove('dragging');
    document.querySelectorAll('#settings_container .setting-card, #settings_container .setting-group').forEach(el => el.classList.remove('drag-over'));
    hideDropIndicator();
    dropInsertBeforeIndex = -1;
    dropTargetCardKey = null;
    draggedSettingCard = null;
    setTimeout(() => { wasDragged = false; }, 100);
});

let dropInsertBeforeIndex = -1;
let dropTargetCardKey = null;
let dropIndicatorEl = null;

function getGridSlots() {
    const grid = document.querySelector('#settings_container .settings-grid-large');
    if (!grid) return [];
    return Array.from(grid.children);
}

function showDropIndicator(beforeIndex) {
    const grid = document.querySelector('#settings_container .settings-grid-large');
    if (!grid) return;
    if (!dropIndicatorEl) {
        dropIndicatorEl = document.createElement('div');
        dropIndicatorEl.className = 'setting-drop-indicator';
    }
    if (beforeIndex < 0 || beforeIndex > grid.children.length) {
        dropIndicatorEl.remove();
        return;
    }
    const ref = grid.children[beforeIndex] || null;
    if (ref) {
        grid.insertBefore(dropIndicatorEl, ref);
    } else {
        grid.appendChild(dropIndicatorEl);
    }
}

function hideDropIndicator() {
    if (dropIndicatorEl && dropIndicatorEl.parentNode) {
        dropIndicatorEl.remove();
    }
}

document.addEventListener('dragover', function(e) {
    if (!draggedSettingCard) return;
    const grid = document.querySelector('#settings_container .settings-grid-large');
    if (!grid || !grid.contains(draggedSettingCard)) return;
    e.preventDefault();
    if (e.dataTransfer) e.dataTransfer.dropEffect = 'move';

    document.querySelectorAll('#settings_container .setting-card').forEach(el => el.classList.remove('drag-over'));
    document.querySelectorAll('#settings_container .setting-group').forEach(el => el.classList.remove('drag-over'));

    const slots = getGridSlots();
    const draggedKey = draggedSettingCard.dataset.key;
    if (!draggedKey) return;

    let found = false;
    for (let i = 0; i < slots.length; i++) {
        const slot = slots[i];
        const rect = slot.getBoundingClientRect();
        const isRtl = document.documentElement.dir === 'rtl' || document.body.dir === 'rtl';
        const x = e.clientX - rect.left;
        const ratio = isRtl ? 1 - x / rect.width : x / rect.width;
        if (e.clientX >= rect.left && e.clientX <= rect.right && e.clientY >= rect.top && e.clientY <= rect.bottom) {
            found = true;
            const card = slot.classList.contains('setting-card') ? slot : slot.querySelector('.setting-card');
            const targetKey = card ? card.dataset.key : null;
            if (ratio >= 0.2 && ratio <= 0.8 && targetKey && targetKey !== draggedKey) {
                dropInsertBeforeIndex = -1;
                dropTargetCardKey = targetKey;
                slot.classList.add('drag-over');
                hideDropIndicator();
            } else {
                dropTargetCardKey = null;
                if (ratio < 0.2) {
                    dropInsertBeforeIndex = i;
                    showDropIndicator(i);
                } else {
                    dropInsertBeforeIndex = i + 1;
                    showDropIndicator(i + 1);
                }
            }
            break;
        }
    }
    if (!found) {
        dropTargetCardKey = null;
        dropInsertBeforeIndex = -1;
        hideDropIndicator();
    }
});

document.addEventListener('drop', function(e) {
    if (!draggedSettingCard) return;
    const grid = document.querySelector('#settings_container .settings-grid-large');
    if (!grid) return;
    e.preventDefault();
    document.querySelectorAll('#settings_container .setting-card, #settings_container .setting-group').forEach(el => el.classList.remove('drag-over'));
    hideDropIndicator();

    const draggedKey = draggedSettingCard.dataset.key;
    if (!draggedKey) return;

    if (dropTargetCardKey != null && dropTargetCardKey !== draggedKey) {
        openGroupNameModalForDrop(draggedKey, dropTargetCardKey);
    } else if (dropInsertBeforeIndex >= 0) {
        applyReorder(draggedKey, dropInsertBeforeIndex);
    }
    dropInsertBeforeIndex = -1;
    dropTargetCardKey = null;
});

function getSlotKeys(slot) {
    if (slot.classList.contains('setting-card')) {
        return [slot.dataset.key];
    }
    if (slot.classList.contains('setting-group')) {
        return Array.from(slot.querySelectorAll('.setting-group-keys .setting-card')).map(c => c.dataset.key).filter(Boolean);
    }
    return [];
}

function applyReorder(draggedKey, insertBeforeIndex) {
    const slots = getGridSlots();
    const keysInOrder = [];
    slots.forEach(slot => keysInOrder.push(...getSlotKeys(slot)));
    const fromIdx = keysInOrder.indexOf(draggedKey);
    if (fromIdx === -1) return;
    const newOrder = keysInOrder.filter(k => k !== draggedKey);
    let insertAt = 0;
    for (let i = 0; i < insertBeforeIndex; i++) insertAt += getSlotKeys(slots[i]).length;
    if (fromIdx < insertAt) insertAt--;
    newOrder.splice(insertAt, 0, draggedKey);

    const orders = {};
    newOrder.forEach((k, i) => { orders[k] = i; });
    const container = document.getElementById('settings_container');
    if (container) container.classList.add('saving-order');

    Promise.resolve().then(() => {
        const wasInGroup = keyGroups[draggedKey];
        if (wasInGroup) {
            const fd = new FormData();
            fd.append('key', draggedKey);
            return fetch('/api/plc-keys/groups/remove-key/', { method: 'POST', body: fd }).then(r => r.json()).then(() => ({ removed: true }));
        }
        return { removed: false };
    }).then(() => {
        const formData = new FormData();
        formData.append('orders', JSON.stringify(orders));
        return fetch('/api/plc-keys/order/bulk/', { method: 'POST', body: formData }).then(r => r.json());
    }).then(result => {
        if (result && result.status === 'ok') loadKeyNames();
    }).catch(err => console.error('Reorder error:', err)).finally(() => {
        if (container) container.classList.remove('saving-order');
    });
}

function openGroupNameModalForDrop(draggedKey, targetKey) {
    const targetGroup = keyGroups[targetKey];
    if (targetGroup) {
        addKeyToGroup(targetGroup.groupId, draggedKey);
        return;
    }
    const titleEl = document.getElementById('group_name_modal_title');
    const inputEl = document.getElementById('group_name_input');
    if (!titleEl || !inputEl) return;
    titleEl.textContent = 'نام گروه جدید';
    inputEl.value = 'گروه جدید';
    inputEl.readOnly = false;
    const confirm = () => {
        const name = inputEl.value.trim() || 'گروه جدید';
        createGroup([draggedKey, targetKey], name);
        closeGroupNameModal();
    };
    document.getElementById('group_name_modal_confirm').onclick = confirm;
    openGroupNameModal(confirm);
}

function openGroupNameModal(onConfirm, skipFocus) {
    const modal = document.getElementById('group_name_modal');
    const inputEl = document.getElementById('group_name_input');
    if (!modal || !inputEl) return;
    modal.style.display = 'flex';
    if (!skipFocus) setTimeout(() => inputEl.focus(), 50);
}

function closeGroupNameModal() {
    const modal = document.getElementById('group_name_modal');
    const inputEl = document.getElementById('group_name_input');
    if (modal) modal.style.display = 'none';
    if (inputEl) { inputEl.value = ''; inputEl.readOnly = false; }
}

function createGroup(keyIds, name) {
    const formData = new FormData();
    formData.append('key_ids', JSON.stringify(keyIds));
    formData.append('name', name);
    fetch('/api/plc-keys/groups/create/', { method: 'POST', body: formData })
        .then(r => r.json())
        .then(result => {
            if (result.status === 'ok') loadKeyNames();
            else if (result.message) showLocalAlert(result.message);
        }).catch(err => console.error('Create group error:', err));
}

function addKeyToGroup(groupId, key) {
    const formData = new FormData();
    formData.append('group_id', groupId);
    formData.append('key', key);
    fetch('/api/plc-keys/groups/add-key/', { method: 'POST', body: formData })
        .then(r => r.json())
        .then(result => {
            if (result.status === 'ok') loadKeyNames();
            else if (result.message) showLocalAlert(result.message);
        }).catch(err => console.error('Add to group error:', err));
}

function removeKeyFromGroup(key) {
    const formData = new FormData();
    formData.append('key', key);
    fetch('/api/plc-keys/groups/remove-key/', { method: 'POST', body: formData })
        .then(r => r.json())
        .then(result => {
            if (result.status === 'ok') loadKeyNames();
            else if (result.message) showLocalAlert(result.message);
        }).catch(err => console.error('Remove from group error:', err));
}

function openRenameGroup(groupId, currentName) {
    const inputEl = document.getElementById('group_name_input');
    const titleEl = document.getElementById('group_name_modal_title');
    if (!inputEl || !titleEl) return;
    titleEl.textContent = 'تغییر نام گروه';
    inputEl.value = currentName;
    inputEl.readOnly = false;
    const confirm = () => {
        const name = inputEl.value.trim();
        if (!name) return;
        const formData = new FormData();
        formData.append('group_id', groupId);
        formData.append('name', name);
        fetch('/api/plc-keys/groups/update-name/', { method: 'POST', body: formData })
            .then(r => r.json())
            .then(result => {
                if (result.status === 'ok') loadKeyNames();
                closeGroupNameModal();
            }).catch(err => console.error('Rename group error:', err));
    };
    document.getElementById('group_name_modal_confirm').onclick = confirm;
    openGroupNameModal(confirm);
}

function openAddKeyToGroup(groupId) {
    const slots = getOrderedSlots();
    const inGroup = {};
    slots.forEach(s => {
        if (s.type === 'group') s.keys.forEach(k => { inGroup[k] = true; });
    });
    const availableKeys = Object.keys(currentSetting).filter(k => !inGroup[k]);
    if (!availableKeys.length) {
        showLocalAlert('کلید دیگری برای افزودن به گروه وجود ندارد');
        return;
    }
    const name = prompt('کلید را وارد کنید (مثلاً ' + availableKeys.slice(0, 3).join(', ') + '):', availableKeys[0]);
    if (name == null || !name.trim()) return;
    const key = availableKeys.find(k => k === name.trim()) || name.trim();
    addKeyToGroup(groupId, key);
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
                colorMaxInput.value = config.color_max || '#ff8800';
                colorMax2Input.value = config.color_max_2 || '#ff4444';
                colorMinInput.value = config.color_min || '#ff4444';
                colorMin2Input.value = config.color_min_2 || '#ff8800';
                
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
            colorMaxInput.value = '#ff8800';
            colorMax2Input.value = '#ff4444';
            colorMinInput.value = '#ff4444';
            colorMin2Input.value = '#ff8800';
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
            if (keyRes && keyRes.status === 'ok' && keyMeta[key]) {
                keyMeta[key].live_background = liveBackgroundChecked;
                keyMeta[key].value_max = parseFloat(valueMax) || 100;
            }
            checkAllCardsForAlerts();
            closeAlertConfigModal();
            let msg = 'تنظیمات با موفقیت ذخیره شد';
            if (!keyRes || keyRes.status !== 'ok') {
                msg += (keyRes && keyRes.message) ? ' (تنظیمات کلید: ' + keyRes.message + ')' : ' (تنظیمات کلید ذخیره نشد)';
            }
            showLocalAlert(msg);
        } else {
            showLocalAlert('خطا: ' + (data.message || 'خطا در ذخیره تنظیمات'));
        }
    }).catch(err => {
        console.error('Error saving config:', err);
        showLocalAlert('خطا در ارتباط با سرور');
    });
}

document.addEventListener('DOMContentLoaded', function() {
    loadKeyNames();
    loadAlertConfigs();
    if (typeof PLC_ID !== 'undefined') connectSSE();

    const groupModal = document.getElementById('group_name_modal');
    if (groupModal) {
        groupModal.addEventListener('click', function(e) {
            if (e.target === groupModal) closeGroupNameModal();
        });
        const closeBtn = document.getElementById('group_name_modal_close');
        const cancelBtn = document.getElementById('group_name_modal_cancel');
        if (closeBtn) closeBtn.addEventListener('click', closeGroupNameModal);
        if (cancelBtn) cancelBtn.addEventListener('click', closeGroupNameModal);
    }

    const modal = document.getElementById('alert_config_modal');
    if (modal) {
        modal.addEventListener('click', function(e) {
            if (e.target === modal) closeAlertConfigModal();
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
