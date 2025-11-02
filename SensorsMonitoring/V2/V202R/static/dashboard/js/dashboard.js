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
