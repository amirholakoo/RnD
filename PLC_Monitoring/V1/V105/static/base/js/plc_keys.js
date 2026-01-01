// PLC & PLC Keys CRUD and Popup Management

let editingKeyId = null;
let editingPLCId = null;

// Toggle Collapse Section
function toggleCollapse(sectionId) {
    const section = document.getElementById(sectionId);
    const header = section.previousElementSibling;
    
    if (section.classList.contains('active')) {
        section.classList.remove('active');
        header.classList.remove('active');
    } else {
        section.classList.add('active');
        header.classList.add('active');
        
        if (sectionId === 'plc_section') {
            loadPLCList();
        } else if (sectionId === 'keys_section') {
            loadSettingsKeys();
        }
    }
}

// Toggle Settings Popup
function toggleSettingsPopup() {
    const popup = document.getElementById('settings_popup');
    const overlay = document.getElementById('settings_overlay');
    
    if (popup.classList.contains('active')) {
        popup.classList.remove('active');
        overlay.classList.remove('active');
        resetPLCForm();
        resetKeyForm();
    } else {
        popup.classList.add('active');
        overlay.classList.add('active');
    }
}

// Toggle Data Table Popup
function toggleDataTablePopup() {
    const popup = document.getElementById('datatable_popup');
    const overlay = document.getElementById('datatable_overlay');
    
    if (popup.classList.contains('active')) {
        popup.classList.remove('active');
        overlay.classList.remove('active');
    } else {
        popup.classList.add('active');
        overlay.classList.add('active');
        loadPLCsForFilter();
        loadDataTable();
    }
}

// ==================== PLC CRUD ====================

// Load PLC List
function loadPLCList() {
    fetch('/api/plcs/')
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                renderPLCTable(data.data);
            }
        })
        .catch(err => console.error('Error loading PLCs:', err));
}

// Render PLC Table
function renderPLCTable(plcs) {
    const tbody = document.querySelector('#plc_table tbody');
    
    if (plcs.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="6" class="empty-state">
                    <svg width="48" height="48" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M4 6H20M4 12H20M4 18H20" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
                    </svg>
                    <p>دستگاهی یافت نشد</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = plcs.map(plc => `
        <tr>
            <td>${plc.device_id}</td>
            <td>${plc.name || '-'}</td>
            <td>${plc.ip_address || '-'}</td>
            <td>${plc.location || '-'}</td>
            <td><span class="status-badge ${plc.Is_Known ? 'known' : 'unknown'}">${plc.Is_Known ? 'شناخته شده' : 'ناشناس'}</span></td>
            <td>
                <div class="action-btns">
                    <button type="button" class="action-btn edit" onclick='editPLC(${JSON.stringify(plc)})'>ویرایش</button>
                    <button type="button" class="action-btn delete" onclick="deletePLC(${plc.id})">حذف</button>
                </div>
            </td>
        </tr>
    `).join('');
}

// Reset PLC Form
function resetPLCForm() {
    const form = document.getElementById('plc_form');
    if (form) {
        form.reset();
        document.getElementById('plc_id').value = '';
        document.getElementById('plc_submit_btn').textContent = 'افزودن';
        editingPLCId = null;
    }
}

// Edit PLC - fill form
function editPLC(plc) {
    document.getElementById('plc_id').value = plc.id;
    document.getElementById('device_id_input').value = plc.device_id;
    document.getElementById('ip_address_input').value = plc.ip_address || '';
    document.getElementById('location_input').value = plc.location || '';
    document.getElementById('description_input').value = plc.description || '';
    document.getElementById('is_known_input').checked = plc.Is_Known;
    document.getElementById('plc_submit_btn').textContent = 'بروزرسانی';
    editingPLCId = plc.id;
}

// Delete PLC
function deletePLC(id) {
    if (!confirm('آیا از حذف این دستگاه اطمینان دارید؟ تمام کلیدهای مرتبط نیز حذف خواهند شد.')) return;
    
    const formData = new FormData();
    formData.append('id', id);
    
    fetch('/api/plcs/delete/', {
        method: 'POST',
        body: formData
    })
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                AM_alert({display: true, status: 'success', text: 'دستگاه با موفقیت حذف شد'});
                loadPLCList();
                loadPLCsForSelect();
            } else {
                AM_alert({display: true, status: 'danger', text: data.message || 'خطا در حذف دستگاه'});
            }
        })
        .catch(err => {
            console.error('Error:', err);
            AM_alert({display: true, status: 'danger', text: 'خطا در ارتباط با سرور'});
        });
}

// ==================== PLC Keys CRUD ====================

// Load PLCs for select dropdown
function loadPLCsForSelect() {
    fetch('/api/plcs/')
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                const select = document.getElementById('plc_select');
                if (select) {
                    select.innerHTML = '<option value="">انتخاب دستگاه...</option>';
                    data.data.forEach(plc => {
                        select.innerHTML += `<option value="${plc.id}">${plc.name || plc.device_id}</option>`;
                    });
                }
            }
        })
        .catch(err => console.error('Error loading PLCs:', err));
}

// Load PLCs for filter dropdown
function loadPLCsForFilter() {
    fetch('/api/plcs/')
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                const select = document.getElementById('filter_plc');
                if (select) {
                    select.innerHTML = '<option value="">همه دستگاه‌ها</option>';
                    data.data.forEach(plc => {
                        select.innerHTML += `<option value="${plc.id}">${plc.name || plc.device_id}</option>`;
                    });
                }
            }
        })
        .catch(err => console.error('Error loading PLCs:', err));
}

// Load keys for settings table
function loadSettingsKeys() {
    fetch('/api/plc-keys/')
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                renderSettingsTable(data.data);
            }
        })
        .catch(err => console.error('Error loading keys:', err));
}

// Render settings table
function renderSettingsTable(keys) {
    const tbody = document.querySelector('#settings_keys_table tbody');
    if (!tbody) return;
    
    if (keys.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="5" class="empty-state">
                    <svg width="48" height="48" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M21 2L19 4M11.3891 11.6109C12.3844 12.6062 13 13.9812 13 15.5C13 18.5376 10.5376 21 7.5 21C4.46243 21 2 18.5376 2 15.5C2 12.4624 4.46243 10 7.5 10C9.01878 10 10.3938 10.6156 11.3891 11.6109ZM11.3891 11.6109L15.5 7.5M15.5 7.5L18.5 10.5L22 7L19 4M15.5 7.5L19 4" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                    <p>کلیدی یافت نشد</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = keys.map(key => `
        <tr>
            <td>${key.name || '-'}</td>
            <td>${key.fa_name || '-'}</td>
            <td>${key.key}</td>
            <td>${key.value || '-'}</td>
            <td>
                <div class="action-btns">
                    <button type="button" class="action-btn edit" onclick='editKey(${JSON.stringify(key)})'>ویرایش</button>
                    <button type="button" class="action-btn delete" onclick="deleteKey(${key.id})">حذف</button>
                </div>
            </td>
        </tr>
    `).join('');
}

// Escape string for JS
function escapeStr(str) {
    return str.replace(/'/g, "\\'").replace(/"/g, '\\"');
}

// Load data table
function loadDataTable() {
    const filterPlc = document.getElementById('filter_plc');
    const plcId = filterPlc ? filterPlc.value : '';
    const url = plcId ? `/api/plc-keys/?plc_id=${plcId}` : '/api/plc-keys/';
    
    fetch(url)
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                renderDataTable(data.data);
            }
        })
        .catch(err => console.error('Error loading data:', err));
}

// Convert timestamp to Persian date
function formatTimestamp(timestamp) {
    if (!timestamp) return '-';
    const date = new Date(timestamp * 1000);
    return date.toLocaleDateString('fa-IR') + ' ' + date.toLocaleTimeString('fa-IR');
}

// Render data table
function renderDataTable(keys) {
    const tbody = document.querySelector('#plc_keys_datatable tbody');
    if (!tbody) return;
    
    if (keys.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="7" class="empty-state">
                    <svg width="48" height="48" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M21 2L19 4M11.3891 11.6109C12.3844 12.6062 13 13.9812 13 15.5C13 18.5376 10.5376 21 7.5 21C4.46243 21 2 18.5376 2 15.5C2 12.4624 4.46243 10 7.5 10C9.01878 10 10.3938 10.6156 11.3891 11.6109ZM11.3891 11.6109L15.5 7.5M15.5 7.5L18.5 10.5L22 7L19 4M15.5 7.5L19 4" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                    <p>داده‌ای یافت نشد</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = keys.map((key, index) => `
        <tr>
            <td>${index + 1}</td>
            <td>${key.name || '-'}</td>
            <td>${key.fa_name || '-'}</td>
            <td>${key.key}</td>
            <td>${key.value || '-'}</td>
            <td>${formatTimestamp(key.CreationDateTime)}</td>
            <td>${formatTimestamp(key.LastUpdate)}</td>
        </tr>
    `).join('');
}

// Reset Key form
function resetKeyForm() {
    const form = document.getElementById('plc_keys_form');
    if (form) {
        form.reset();
        document.getElementById('key_id').value = '';
        document.getElementById('key_name_input').value = '';
        document.getElementById('key_fa_name_input').value = '';
        document.getElementById('key_input').value = '';
        document.getElementById('value_input').value = '';
        document.getElementById('key_description_input').value = '';
        document.getElementById('key_submit_btn').textContent = 'افزودن';
        editingKeyId = null;
    }
}

// For backward compatibility
function resetForm() {
    resetKeyForm();
}

// Edit key - fill form
function editKey(keyData) {
    document.getElementById('key_id').value = keyData.id;
    document.getElementById('key_name_input').value = keyData.name || '';
    document.getElementById('key_fa_name_input').value = keyData.fa_name || '';
    document.getElementById('key_input').value = keyData.key;
    document.getElementById('value_input').value = keyData.value || '';
    document.getElementById('key_description_input').value = keyData.description || '';
    document.getElementById('key_submit_btn').textContent = 'بروزرسانی';
    editingKeyId = keyData.id;
}

// Delete key
function deleteKey(id) {
    if (!confirm('آیا از حذف این کلید اطمینان دارید؟')) return;
    
    const formData = new FormData();
    formData.append('id', id);
    
    fetch('/api/plc-keys/delete/', {
        method: 'POST',
        body: formData
    })
        .then(res => res.json())
        .then(data => {
            if (data.status === 'ok') {
                AM_alert({display: true, status: 'success', text: 'کلید با موفقیت حذف شد'});
                loadSettingsKeys();
            } else {
                AM_alert({display: true, status: 'danger', text: data.message || 'خطا در حذف کلید'});
            }
        })
        .catch(err => {
            console.error('Error:', err);
            AM_alert({display: true, status: 'danger', text: 'خطا در ارتباط با سرور'});
        });
}

// ==================== Form Submit Handlers ====================

document.addEventListener('DOMContentLoaded', function() {
    // PLC Form Submit
    const plcForm = document.getElementById('plc_form');
    if (plcForm) {
        plcForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData(plcForm);
            const isEditing = editingPLCId !== null;
            const url = isEditing ? '/api/plcs/update/' : '/api/plcs/create/';
            
            fetch(url, {
                method: 'POST',
                body: formData
            })
                .then(res => res.json())
                .then(data => {
                    if (data.status === 'ok') {
                        AM_alert({
                            display: true,
                            status: 'success',
                            text: isEditing ? 'دستگاه با موفقیت بروزرسانی شد' : 'دستگاه با موفقیت افزوده شد'
                        });
                        resetPLCForm();
                        loadPLCList();
                        loadPLCsForSelect();
                    } else {
                        AM_alert({display: true, status: 'danger', text: data.message || 'خطا در ذخیره دستگاه'});
                    }
                })
                .catch(err => {
                    console.error('Error:', err);
                    AM_alert({display: true, status: 'danger', text: 'خطا در ارتباط با سرور'});
                });
        });
    }
    
    // PLC Keys Form Submit
    const keysForm = document.getElementById('plc_keys_form');
    if (keysForm) {
        keysForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData(keysForm);
            const isEditing = editingKeyId !== null;
            const url = isEditing ? '/api/plc-keys/update/' : '/api/plc-keys/create/';
            
            fetch(url, {
                method: 'POST',
                body: formData
            })
                .then(res => res.json())
                .then(data => {
                    if (data.status === 'ok') {
                        AM_alert({
                            display: true,
                            status: 'success',
                            text: isEditing ? 'کلید با موفقیت بروزرسانی شد' : 'کلید با موفقیت افزوده شد'
                        });
                        resetKeyForm();
                        loadSettingsKeys();
                    } else {
                        AM_alert({display: true, status: 'danger', text: data.message || 'خطا در ذخیره کلید'});
                    }
                })
                .catch(err => {
                    console.error('Error:', err);
                    AM_alert({display: true, status: 'danger', text: 'خطا در ارتباط با سرور'});
                });
        });
    }
});

// Close popups with Escape key
document.addEventListener('keydown', function(e) {
    if (e.key === 'Escape') {
        const settingsPopup = document.getElementById('settings_popup');
        const datatablePopup = document.getElementById('datatable_popup');
        
        if (settingsPopup && settingsPopup.classList.contains('active')) {
            toggleSettingsPopup();
        }
        if (datatablePopup && datatablePopup.classList.contains('active')) {
            toggleDataTablePopup();
        }
    }
});
