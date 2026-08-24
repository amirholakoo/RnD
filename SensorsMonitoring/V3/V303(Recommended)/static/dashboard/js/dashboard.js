// Calendar Modal Functionality
let currentDateInput = null; // Track which input field triggered the modal
    
const dateRangeStartInput = document.getElementById('date_range_start');
const dateRangeEndInput = document.getElementById('date_range_end');
const calendarModal = document.getElementById('calendarModal');
const calendarCloseButton = document.querySelector('.calendar-modal-close');

if (dateRangeStartInput) {
    dateRangeStartInput.addEventListener('click', function() {
        currentDateInput = this;
        openCalendarModal();
    });
}

if (dateRangeEndInput) {
    dateRangeEndInput.addEventListener('click', function() {
        currentDateInput = this;
        openCalendarModal();
    });
}

if (calendarCloseButton) {
    calendarCloseButton.addEventListener('click', function() {
        closeCalendarModal();
    });
}

if (calendarModal) {
    // Close modal when clicking outside
    calendarModal.addEventListener('click', function(e) {
        if (e.target === this) {
            closeCalendarModal();
        }
    });

    // Use event delegation for calendar day clicks (works regardless of when calendar.js runs)
    calendarModal.addEventListener('click', function(e) {
        // Check if clicked element is a calendar day label
        const calendarLabel = e.target.closest('#calendarModal #calendar .calendar_month label.calendar_day');
        if (calendarLabel && !calendarLabel.classList.contains('disable')) {
            const dateValue = calendarLabel.getAttribute('date-data');
            if (dateValue && currentDateInput) {
                // Update calendar_active_val (as calendar.js does)
                calendar_active_val = dateValue;
                console.log("calendar_active_val", calendar_active_val);
                
                // Update the input field and close modal
                currentDateInput.value = dateValue;
                closeCalendarModal();
            }
        }
    });
}

function openCalendarModal() {
    document.getElementById('calendarModal').style.display = 'flex';
}

function closeCalendarModal() {
    document.getElementById('calendarModal').style.display = 'none';
    currentDateInput = null;
}


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
const deviceEditForm = document.getElementById('deviceEditForm');
if (deviceEditForm) {
    deviceEditForm.addEventListener('submit', function(e) {
        e.preventDefault();
        const formData = new FormData(this);
        const csrfInput = this.querySelector('[name=csrfmiddlewaretoken]');
        const csrftoken = csrfInput ? csrfInput.value : '';

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
}

// Handle Sensor Form Submission
const sensorEditForm = document.getElementById('sensorEditForm');
if (sensorEditForm) {
    sensorEditForm.addEventListener('submit', function(e) {
        e.preventDefault();
        const formData = new FormData(this);
        const csrfInput = this.querySelector('[name=csrfmiddlewaretoken]');
        const csrftoken = csrfInput ? csrfInput.value : '';

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
}

function applyDateRangeFilter(sensorId) {
    Progress(true);
    var date_range_start = document.querySelector('input[name=date_range_start]').value;
    var date_range_end = document.querySelector('input[name=date_range_end]').value;
    console.log("date_range_start",date_range_start);
    console.log("date_range_end",date_range_end);
    Progress(false);
}   

function exportData(sensorId) {
    Progress(true);
    let date_range_start = document.querySelector('input[name="date_range_start"]').value;
    let date_range_end = document.querySelector('input[name="date_range_end"]').value;
    let time_choices = document.querySelector('input[name="time_choices"]:checked').value;
    console.log("date_range_start",date_range_start);
    console.log("date_range_end",date_range_end);
    console.log("time_choices",time_choices);
    if(time_choices == 1){
        time_choices = 'hourly';
    } else {
        time_choices = 'minute';
    }
    var time_filter = document.querySelector('.chart-filter .active');
    var export_choices = document.querySelector('input[name=export_choices]:checked').value;
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
            export_choices: export_choices,
            date_range_start: date_range_start,
            date_range_end: date_range_end,
            time_choices: time_choices,
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

function showAllCharts() {
    const show_all_charts = document.getElementById('show_all_charts');
    const Charts_To_Show = document.querySelector('.chart-container');
    if(show_all_charts.checked){
        Charts_To_Show.classList.remove('default');
    } else {
        Charts_To_Show.classList.add('default');
    }
}