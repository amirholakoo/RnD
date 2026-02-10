// progress
function Progress(display = true) {
  if (display) {
      document.getElementById('progressbar').style.display = 'flex';
  } else {
      document.getElementById('progressbar').style.display = '';
  }
}

// alert
function AM_alert({display=true,status = 'info', text=false,btns=false,obj=false}) {
    clearTimeout(this.timeoutID)
    let elem = document.getElementById('Alert');
    let classname = `alert_child_${elem.children.length}`
    if (display) {
        elem.classList.add('show_up');
        elem.insertAdjacentHTML('beforeend',
        `
        <div class="Am_alert_  ${classname} ${status}">
            <span class="alert_close_btn" onclick="AM_alert({display:false,obj:this.parentElement})">
                <svg width="12" height="12" viewBox="0 0 12 12" fill="none" xmlns="http://www.w3.org/2000/svg">
                    <path d="M1 11L11 1" stroke="#B6B6B6" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    <path d="M11 11L1 1" stroke="#B6B6B6" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                </svg>
            </span>
            <div class="d-flex align-items-center ml-4 alert-text-card">
                <div class="icon">
                    <svg class="success" width="37" height="37" viewBox="0 0 37 37" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M18.5 36C28.125 36 36 28.125 36 18.5C36 8.875 28.125 1 18.5 1C8.875 1 1 8.875 1 18.5C1 28.125 8.875 36 18.5 36Z" stroke="#25B700" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                        <path d="M12 18.5L16.6612 23L26 14" stroke="#25B700" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                    <svg class="warning" width="39" height="37" viewBox="0 0 39 37" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M19.5889 12.9102V22.2051" stroke="#FFA800" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                        <path d="M19.5888 35.9814H8.32336C1.87269 35.9814 -0.822836 31.3711 2.30026 25.7384L8.10028 15.2909L13.5657 5.4755C16.8747 -0.491834 22.3029 -0.491834 25.6119 5.4755L31.0773 15.3095L36.8773 25.757C40.0004 31.3897 37.2863 36 30.8542 36H19.5888V35.9814Z" stroke="#FFA800" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                        <path d="M19.5762 27.7832H19.5929" stroke="#FFA800" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                    <svg class="info" width="37" height="37" viewBox="0 0 37 37" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M18.5 0.999998C8.875 0.999998 1 8.875 1 18.5C1 28.125 8.875 36 18.5 36C28.125 36 36 28.125 36 18.5C36 8.875 28.125 0.999999 18.5 0.999998Z" stroke="#008DFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                        <path d="M18.5 25.499L18.5 16.749" stroke="#008DFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                        <path d="M18.5117 11.501L18.496 11.501" stroke="#008DFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                    <svg class="danger" width="37" height="37" viewBox="0 0 37 37" fill="none" xmlns="http://www.w3.org/2000/svg">
                    <path d="M18.5 36C28.125 36 36 28.125 36 18.5C36 8.875 28.125 1 18.5 1C8.875 1 1 8.875 1 18.5C1 28.125 8.875 36 18.5 36Z" stroke="#FF0000" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    <path d="M13.5469 23.4524L23.4519 13.5474" stroke="#FF0000" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    <path d="M23.4519 23.4524L13.5469 13.5474" stroke="#FF0000" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                </div>
                <h6 class="alert-text">
                    ${text}
                    <div class="d-flex justify-content-center mt-2 alert_btns">
                </h6>
                    
                </div>
            </div>
        </div>
        `);
        if(btns) {
            for(const x of btns) {
                document.querySelector(`.${classname} .alert_btns`).insertAdjacentHTML('beforeend',`<a class="bg-light rounded px-3 py-2 mr-1" style="color:#333;" ${x.attr? x.attr.name+"='"+ x.attr.inner +"'":''} ${x.link? 'href='+x.link:'onclick="Am_alert({display:false,obj:this.parentElement.parentElement})"'}>${x.title}</a>`)
            }
        }
  
    } else {
        obj.remove();
        elem.classList.remove('show_up');
        classname = false
    }
    setTimeout(()=>{if(classname){document.getElementsByClassName(classname)[0].remove();elem.classList.remove('show_up');};this.timeoutID=32567},10000)
  }

$(document).ready(function () {
  $(window).scroll(function () {
    if ($(window).scrollTop() > 600) {
      $(".scroll_up").css({ opacity: "1", "pointer-events": "visible" });
    } else {
      $(".scroll_up").css({ opacity: "0", "pointer-events": "none" });
    }
  });
});

// upload preview
function readURL(input) {
    Progress();
    if (input.files && input.files[0]) {
        var reader = new FileReader();

        reader.onload = function(e) {
            $('#blah')
                .attr('src', e.target.result);
        };

        reader.readAsDataURL(input.files[0]);
    }
    Progress(false);
}


// sse for unknown devices
const unknown_devices = document.querySelector('#unknown_devices');
let LogsData = document.querySelector('#LogsData');
let device_id = document.querySelector('#device_id');
let sensor_type = document.querySelector('#sensor_type');
let Is_Popup = false;
let source = null;

if (typeof(EventSource) !== "undefined") {
    source = new EventSource("/dashboard/ask_for_device/");
    source.onmessage = function(event) {
        let data = JSON.parse(event.data);
        console.log(data);
        if (data && data.device_id) {
            if (LogsData && device_id.textContent == data['device_id'] && sensor_type.textContent == data['sensor_type']) {
                LogsData.querySelector('tbody').insertAdjacentHTML('afterbegin',`
                    <tr class="data-row">
                        <td class="time-cell">
                        <div class="time-content">
                            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                                <circle cx="12" cy="12" r="10" stroke="currentColor" stroke-width="2"></circle>
                                <polyline points="12,6 12,12 16,14" stroke="currentColor" stroke-width="2"></polyline>
                            </svg>
                            ${data['CreationDateTime']}
                        </div></td>
                        <td class="data-cell">${data['data']}</td>
                        <td class="device-id-cell">${data['device_id']}</td>
                        <td class="ip-cell">${data['ip_address']}</td>
                        <td class="status-cell">
                        <span class="status-badge status-known">${data['Is_Known'] ? 'شناخته شده' : 'ناشناس'}</span>
                        </td>
                    </tr>
                    `);
            } else {
                if(document.querySelector('#Alert').children.length < 3){
                    //AM_alert({display:true,status:'info',text:`دستگاه ${data['device_id']} با نوع سنسور ${data['sensor_type']} در حال ارسال دیتا میباشد`});
                }
            }
            if(!Is_Popup && data['Is_Known'] == false && unknown_devices){
                unknown_devices.style.display = 'flex';
                unknown_devices.querySelector('input[name="device_id"]').value = data['device_id'];
                unknown_devices.querySelector('input[name="ip_address"]').value = data['ip_address'];
                unknown_devices.querySelector('input[name="sensor_type"]').value = data['sensor_type'];
                unknown_devices.querySelector('input[name="CreationDateTime"]').value = data['CreationDateTime'];
                Is_Popup = true;
            }
        }
    };
}

function cleanupOnPageLeave() {
    if (source) {
        source.close();
        console.log('EventSource connection closed');
    }
    
    Progress(false);
    
    if (window.sensorStatusInterval) {
        clearInterval(window.sensorStatusInterval);
        console.log('Sensor status interval cleared');
    }
    
    if (window.countdownInterval) {
        clearInterval(window.countdownInterval);
        console.log('Countdown interval cleared');
    }
}
window.addEventListener('beforeunload', cleanupOnPageLeave);
window.addEventListener('pagehide', cleanupOnPageLeave);
window.addEventListener('popstate', function(event) {
    cleanupOnPageLeave();
});

const unknown_devices_form = document.getElementById('unknown_devices_form');
// add device
unknown_devices_form.addEventListener('submit',(e)=>{
    e.preventDefault();
    let data = new FormData(unknown_devices_form);
    console.log(data);
    fetch('/dashboard/add_device/',{
        method: 'POST',
        body: data,
    }).then(res=>res.json()).then(data=>{
        if(data['status'] == 'ok'){
            unknown_devices.style.display = 'none';
            Is_Popup = false;
            AM_alert({display:true,status:'success',text:'دستگاه با موفقیت افزوده شد'});
        }else{
            AM_alert({display:true,status:'danger',text:'مشکلی پیش آمد!'});
        }
    });
});

// toggle device body
const device_cards = document.querySelectorAll('.device-card');
device_cards.forEach(device_card=>{
    device_card.addEventListener('click',()=>{
        device_cards.forEach(d=>{
            if(d.parentElement.classList.contains('active') && d != device_card){
                d.parentElement.classList.remove('active');
            }
        });
        device_card.parentElement.classList.toggle('active');
    });
});

document.addEventListener('click',(e)=>{
    const ClassListToCheck = ["device-card","device-body","device-header","device-name","device-location"]
    if(!ClassListToCheck.some(cls => e.target.classList.contains(cls))){
        console.log("find")
        device_cards.forEach(device_card=>{
            if(device_card.parentElement.classList.contains('active')){
                device_card.parentElement.classList.remove('active');
            }
        });
    }
});
