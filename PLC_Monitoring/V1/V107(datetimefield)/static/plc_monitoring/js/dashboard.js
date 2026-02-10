let eventSource = null;
let logsCount = 0;
const MAX_LOGS = 100;

function formatTimestamp(timestamp) {
    if (!timestamp) return '-';
    const date = new Date(timestamp * 1000);
    const jDate = jalaali.toJalaali(date.getFullYear(), date.getMonth() + 1, date.getDate());
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    return `${jDate.jy}/${jDate.jm}/${jDate.jd} - ${hours}:${minutes}:${seconds}`;
}

function truncateData(data, maxLength = 80) {
    if (!data) return '-';
    if (data.length > maxLength) {
        return data.substring(0, maxLength) + '...';
    }
    return data;
}

function formatLastUpdate(timestamp) {
    if (!timestamp) return '';
    const date = new Date(timestamp * 1000);
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    return `${hours}:${minutes}:${seconds}`;
}

function addLogRow(log, prepend = true) {
    const tbody = document.getElementById('logs_tbody');
    const emptyRow = tbody.querySelector('.empty-row');
    if (emptyRow) emptyRow.remove();

    const row = document.createElement('tr');
    row.className = 'log-row new-row';
    row.dataset.id = log.id;
    row.dataset.lastUpdate = log.LastUpdate;
    
    row.innerHTML = `
        <td class="log-id">${log.id}</td>
        <td class="log-device">
            <span class="device-badge">${log.plc_device_id}</span>
            ${log.plc_name ? `<span class="device-name">${log.plc_name}</span>` : ''}
        </td>
        <td class="log-data" title="${log.data || ''}">${truncateData(log.data)}</td>
        <td class="log-time">
            <div class="time-created">${formatTimestamp(log.CreationDateTime)}</div>
            <div class="time-updated">آخرین: ${formatLastUpdate(log.LastUpdate)}</div>
        </td>
    `;

    if (prepend) {
        tbody.insertBefore(row, tbody.firstChild);
    } else {
        tbody.appendChild(row);
    }

    setTimeout(() => row.classList.remove('new-row'), 500);

    const rows = tbody.querySelectorAll('.log-row');
    if (rows.length > MAX_LOGS) {
        rows[rows.length - 1].remove();
    }

    logsCount++;
    updateLogsCount();
}

function updateLogRow(log) {
    const existingRow = document.querySelector(`tr[data-id="${log.id}"]`);
    if (!existingRow) return;
    
    existingRow.dataset.lastUpdate = log.LastUpdate;
    existingRow.classList.add('updated-row');
    
    const timeCell = existingRow.querySelector('.log-time');
    if (timeCell) {
        timeCell.innerHTML = `
            <div class="time-created">${formatTimestamp(log.CreationDateTime)}</div>
            <div class="time-updated">آخرین: ${formatLastUpdate(log.LastUpdate)}</div>
        `;
    }
    
    setTimeout(() => existingRow.classList.remove('updated-row'), 500);
}

function updateLogsCount() {
    const countEl = document.getElementById('logs_count');
    const rows = document.querySelectorAll('#logs_tbody .log-row').length;
    countEl.textContent = `${rows} لاگ`;
}

function clearLogsTable() {
    const tbody = document.getElementById('logs_tbody');
    tbody.innerHTML = `
        <tr class="empty-row">
            <td colspan="4">
                <div class="empty-state">
                    <svg width="48" height="48" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
                        <path d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z"/>
                    </svg>
                    <p>در انتظار دریافت لاگ...</p>
                </div>
            </td>
        </tr>
    `;
    logsCount = 0;
    updateLogsCount();
}

function loadInitialLogs() {
    fetch('/api/logs/?limit=50')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok' && data.data.length > 0) {
                data.data.forEach(log => addLogRow(log, false));
            }
        })
        .catch(err => console.error('Error loading initial logs:', err));
}

function connectSSE() {
    if (eventSource) {
        eventSource.close();
    }

    eventSource = new EventSource('/api/logs/stream/');

    eventSource.onmessage = function(event) {
        try {
            const logs = JSON.parse(event.data);
            logs.forEach(log => {
                if (log.is_update) {
                    updateLogRow(log);
                } else {
                    const existingRow = document.querySelector(`tr[data-id="${log.id}"]`);
                    if (!existingRow) {
                        addLogRow(log, true);
                    }
                }
            });
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
    loadInitialLogs();
    connectSSE();
});

window.addEventListener('beforeunload', function() {
    if (eventSource) {
        eventSource.close();
    }
});