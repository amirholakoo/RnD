// Device Modal Functions
function openDeviceModal(deviceId) {
    fetch(`/dashboard/get_device_data/${deviceId}/`)
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                document.getElementById('device_id').value = data.device.id;
                document.getElementById('device_name').value = data.device.name || '';
                document.getElementById('device_location').value = data.device.location || '';
                document.getElementById('device_ip_address').value = data.device.ip_address || '';
                document.getElementById('device_description').value = data.device.description || '';
                document.getElementById('deviceModal').style.display = 'flex';
            } else {
                AM_alert({display:true,status:'danger',text:'خطا در دریافت اطلاعات دستگاه: ' + data.message});
            }
        })
        .catch(error => {
            console.error('Error:', error);
            AM_alert({display:true,status:'danger',text:'خطا در دریافت اطلاعات دستگاه'});
        });
}

function closeDeviceModal() {
    document.getElementById('deviceModal').style.display = 'none';
}

// Sensor Modal Functions
function openSensorModal(sensorId) {
    fetch(`/dashboard/get_sensor_data/${sensorId}/`)
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                document.getElementById('sensor_id').value = data.sensor.id;
                document.getElementById('sensor_type').value = data.sensor.sensor_type || '';
                document.getElementById('sensor_description').value = data.sensor.description || '';
                document.getElementById('sensorModal').style.display = 'flex';
            } else {
                AM_alert({display:true,status:'danger',text:'خطا در دریافت اطلاعات سنسور: ' + data.message});
            }
        })
        .catch(error => {
            console.error('Error:', error);
            AM_alert({display:true,status:'danger',text:'خطا در دریافت اطلاعات سنسور'});
        });
}

function closeSensorModal() {
    document.getElementById('sensorModal').style.display = 'none';
}

// Close modals when clicking outside
window.onclick = function(event) {
    const deviceModal = document.getElementById('deviceModal');
    const sensorModal = document.getElementById('sensorModal');
    if (event.target == deviceModal) {
        closeDeviceModal();
    }
    if (event.target == sensorModal) {
        closeSensorModal();
    }
}

// Handle Device Form Submission
document.getElementById('deviceEditForm').addEventListener('submit', function(e) {
    e.preventDefault();
    const formData = new FormData(this);
    const csrftoken = document.querySelector('[name=csrfmiddlewaretoken]').value;

    fetch('/dashboard/update_device/', {
        method: 'POST',
        headers: {
            'X-CSRFToken': csrftoken
        },
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            AM_alert({display:true,status:'success',text:'اطلاعات دستگاه با موفقیت به‌روزرسانی شد'});
            Progress(true);
            closeDeviceModal();
            location.reload();
        } else {
            AM_alert({display:true,status:'danger',text:'خطا در به‌روزرسانی: ' + data.message});
            Progress(false);
        }
    })
    .catch(error => {
        console.error('Error:', error);
        AM_alert({display:true,status:'danger',text:'خطا در به‌روزرسانی اطلاعات دستگاه'});
        Progress(false);
    });
});

// Handle Sensor Form Submission
document.getElementById('sensorEditForm').addEventListener('submit', function(e) {
    e.preventDefault();
    const formData = new FormData(this);
    const csrftoken = document.querySelector('[name=csrfmiddlewaretoken]').value;

    fetch('/dashboard/update_sensor/', {
        method: 'POST',
        headers: {
            'X-CSRFToken': csrftoken
        },
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            AM_alert({display:true,status:'success',text:'اطلاعات سنسور با موفقیت به‌روزرسانی شد'});
            Progress();
            closeSensorModal();
            location.reload();
        } else {
            AM_alert({display:true,status:'danger',text:'خطا در به‌روزرسانی: ' + data.message});
            Progress(false);
        }
    })
    .catch(error => {
        console.error('Error:', error);
        AM_alert({display:true,status:'danger',text:'خطا در به‌روزرسانی اطلاعات سنسور'});
        Progress(false);
    });
});


function exportData(sensorId) {
    Progress(true);
    var time_filter = document.querySelector('.chart-filter .active');
    var time_key = '';
    let filter_data = [];
    let chart_value_filter = document.querySelectorAll('.chart-value-filter');
    chart_value_filter.forEach(item=>{
        filter_data.push({
            range: item.dataset.range,
            min_value: item.querySelector('input[id^="min-value-"]').value,
            max_value: item.querySelector('input[id^="max-value-"]').value,
            type: item.dataset.type,
        })
    });
    
    if (time_filter) {
        if (time_filter.classList.contains("week")) {
            time_key = 'week';
        } else if (time_filter.classList.contains("month")) {
            time_key = 'month';
        } else if (time_filter.classList.contains("daily")) {
            time_key = 'daily';
        } else {
            time_key = 'hourly';
        }
    }
    
    fetch(`/dashboard/export_data/${sensorId}/?${time_key}`, {
        method: 'POST',
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify({
            filter_data: filter_data,
        })
    })
    .then(response => {
        // Check if response is a file or JSON error
        const contentType = response.headers.get('content-type');
        if (contentType && (contentType.includes('application/vnd.openxmlformats') || contentType.includes('text/csv'))) {
            // File download
            return response.blob().then(blob => {
                // Get filename from Content-Disposition header
                const disposition = response.headers.get('Content-Disposition');
                let filename = 'export_data.xlsx';
                if (disposition && disposition.indexOf('filename=') !== -1) {
                    const filenameRegex = /filename[^;=\n]*=((['"]).*?\2|[^;\n]*)/;
                    const matches = filenameRegex.exec(disposition);
                    if (matches != null && matches[1]) { 
                        filename = matches[1].replace(/['"]/g, '');
                    }
                }
                
                // Create download link
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.style.display = 'none';
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                
                AM_alert({display:true,status:'success',text:'فایل با موفقیت دانلود شد'});
                Progress(false);
            });
        } else {
            // JSON error response
            return response.json().then(data => {
                AM_alert({display:true,status:'danger',text:'خطا در انجام عملیات: ' + data.message});
                Progress(false);
            });
        }
    })
    .catch(error => {
        console.error('Error:', error);
        AM_alert({display:true,status:'danger',text:'خطا در انجام عملیات'});
        Progress(false);
    });
}