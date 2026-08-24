let eventSource = null;


async function createPost(method,url,data) {
    if(!url || !data) {
        console.log("no valid data/url");
        return;
    }
    try {
    const response = await fetch(url, {
      method,
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data)
    });

    if (!response.ok) {
      throw new Error(`HTTP error: ${response.status}`);
    }

    const result = await response.json();
    return result;
    } catch (error) {
    console.error("Fetch failed:", error);
  }
}

//recommended_licence_plate()


// ============= add production machine start =============

const add_production_machine_btn = document.getElementById("submit_production_machine");
const production_machine_name = document.getElementById("production_machine_name");
const production_machine_api = document.getElementById("production_machine_api");
const product_machine_nav = document.querySelector(".product_machine_nav");
const products_machine_form_content = document.getElementById("products_machine_form_content");
const add_roll_section = document.getElementById("add_roll_section");

function insert_machine(name) {
    product_machine_nav.children[0].remove();
    product_machine_nav.insertAdjacentHTML("afterbegin",`
        <div class="devider pb-2">ماشین ها</div>
        <a href="#settings#products_machine#machines_page#machine_${name}" class="nav_item active" nav_id="machine_${name}">${name}</a>
        `);
    products_machine_form_content.insertAdjacentHTML("beforeend",`
        <div class="section_page align-items-start pt-0" id="machine_${name}">
            <div class="form_content">
                <div class="devider">
                    مشخصات ماشین ${name}
                </div>
                <div class="form_section">
                    <div class="form_group">
                        <input type="text" class="form-control" id="name" name="name" placeholder="مثال: pm4-400" value="${name}" required readonly>
                        <label class="form-label" for="name">نام ماشین</label>
                    </div>
                    <div class="form_group">
                        <input type="text" class="form-control" id="name" name="name" placeholder="مثال: pm4-400" value="192.168.2.46:6010/api/" required readonly>
                        <label class="form-label" for="name">api</label>
                    </div>
                    <div class="form_group">
                        <textarea type="text" class="form-control" style="height: auto" id="name" name="name" placeholder="مثال: pm4-400" rows="3" required readonly>
                            192.168.2.46:6010/api/
                        </textarea>
                        <label class="form-label" for="name">api</label>
                    </div>
                </div>
            </div>
        </div>
        `);
    add_roll_section.insertAdjacentHTML("beforeend",`
        
        `)
}

add_production_machine_btn.onclick = ()=> {
    const machine_name = production_machine_name.value;
    const machine_api = production_machine_api.value || '';
    if (!machine_name) {
        AM_alert({status:'danger',text:'لطفا نام ماشین را وارد کنید'});
        return;
    }
    let data = {
        "machine_name":machine_name,
        "machine_api":machine_api,
    }
    createPost("POST","/api/products_machine/add",data).then(
        result => {
            if (result.status == "error") {
                AM_alert({status:'danger',text:result.msg});
                return;
            }
            AM_alert({status:'success',text:'ماشین مورد نظر با موفقیت اضافه شد'});
            insert_machine(machine_name);
        }).catch(err => {
            console.error("Error:", err);
        });
}

// ============= add production machine end =============


// ============= add products type start =============

const submit_product_type = document.getElementById("submit_product_type");
const product_type_title = document.getElementById("product_type_title");
const product_type_desc = document.getElementById("product_type_desc");


submit_product_type.onclick = ()=> {
    const title = product_type_title.value;
    const desc = product_type_desc.value;

    console.log(title,desc);
    if (!title) {
        AM_alert({status:'danger',text:'لطفا عنوان را وارد کنید'});
        return;
    }
    let data = {
        "title":title,
        "desc": desc,
    }
    createPost("POST","/api/products/products_type/add",data).then(
        result => {
            if (result.status == "error") {
                AM_alert({status:'danger',text:result.msg});
                return;
            }
            AM_alert({status:'success',text:'نوع محصول مورد نظر با موفقیت اضافه شد'});
        }).catch(err => {
            console.error("Error:", err);
        });
}
// ============= add products type end =============


// ============= add products start =============


function add_products(obj,machine_id) {
    const form = obj.parentElement.parentElement;
    const inputs = form.querySelectorAll(".form-control");

    let data = {}
    data["products_machine"] = machine_id;
    for(const x of inputs) {
        data[x.name] = x.value;
        if (!x.name.includes("desc") && !x.value) {
            AM_alert({status:'danger',text:`لطفا ${x.nextElementSibling.innerHTML} را وارد کنید`});
            return;
        }
    }
    console.log(data);
    createPost("POST","/api/products/add",data).then(
        result => {
            if (result.status == "error") {
                AM_alert({status:'danger',text:result.msg});
                return;
            }
            AM_alert({status:'success',text:'محصول مورد نظر با موفقیت اضافه شد'});
        }).catch(err => {
            AM_alert({status:'success',text:'مشکلی در ارسال اطلاعات وجود دارد'});
            console.error("Error:", err);
        });
}

function call_last_roll_api(id) {
    const roll_number = document.getElementById(`product_form_roll_number_${id}`);
    const breaks = document.getElementById(`product_form_breaks_${id}`);
    const printed_length = document.getElementById(`product_form_length_${id}`);
    // if (!machine_name) {
    //     AM_alert({status:'danger',text:'لطفا نام ماشین را وارد کنید'});
    //     return;
    // }
    let data = {
        "machine_id":id,
    }
    createPost("POST","/api/products/call_api_for_last_roll",data).then(
        result => {
            if (result.status == "error") {
                AM_alert({status:'danger',text:result.msg});
                return;
            }
            // AM_alert({status:'info',text:'آخرین شماره رول موجود برای شما بارگذاری شد'});
            roll_number.value = result.data;
            breaks.value = result.breaks;
            printed_length.value = result.length;
        }).catch(err => {
            console.error("Error:", err);
        });
}
// ============= add products end =============

// ============= stream products list start ===========

function connectSSE() {
    if (eventSource) {
        eventSource.close();
    }
    
    eventSource = new EventSource(`/api/products/products_list_stream`);
    
    eventSource.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            update_product_list(data)
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

const products_list_table = document.getElementById("products_list_table");
const qr_code_page = document.querySelector("#qrcode_page .form_content");
function update_product_list(data) {
    i=1
    for (const x of data) {
        if(!document.querySelector(`[data-name=product_${x.id}]`)) {
            products_list_table.children[0].insertAdjacentHTML("beforeend",`
            <tr data-name="product_${x.id}">
                <td>${i}</td>
                <td>#</td>
                <td>${x.roll_number}</td>
                <td>${x.products_machine_id}</td>
                <td>${x.width}</td>
                <td>${x.grammage}</td>
                <td>${x.length}</td>
                <td>${x.breaks}</td>
                <td>${x.type_id}</td>
                <td>${x.profile}</td>
                <td>
                    <a href="#qrcode_page#qr_${x.roll_number}" class="nav_item" nav_id="qr_${x.roll_number}">
                        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-qr-code w-4 h-4"><rect width="5" height="5" x="3" y="3" rx="1"></rect><rect width="5" height="5" x="16" y="3" rx="1"></rect><rect width="5" height="5" x="3" y="16" rx="1"></rect><path d="M21 16h-3a2 2 0 0 0-2 2v3"></path><path d="M21 21v.01"></path><path d="M12 7v3a2 2 0 0 1-2 2H7"></path><path d="M3 12h.01"></path><path d="M12 3h.01"></path><path d="M12 16v.01"></path><path d="M16 12h1"></path><path d="M21 12v.01"></path><path d="M12 21v-1"></path></svg>
                    </a>
                </td>
            </tr>
            `)
            i++;
        }
        if (!document.querySelector(`[id="qr_${x.roll_number}"]`)) {
            qr_code_page.insertAdjacentHTML("beforeend",`
                <div class="section_page" id="qr_${x.roll_number}">
                    <div class="qr_gallery">
                        <img src="${x.qr}" alt="">
                        <h4>${x.roll_number}</h4>
                    </div>
                </div>
                `)
        }
    }
}
// ============= stream products list end ===========


document.addEventListener('DOMContentLoaded', function() {
    connectSSE();
})

window.addEventListener('beforeunload', function() {
    if (eventSource) {
        eventSource.close();
    }
});

// check if mouse is near the edge when qr_code is active
const threshold = 100; // pixels from the edge

document.addEventListener('mousemove', (event) => {
    const mouseX = event.clientX;
    const mouseY = event.clientY;
    const width = window.innerWidth;
    const height = window.innerHeight;
    let btn = document.querySelector("#qrcode_page .back_button");
    if (
        document.getElementById("qrcode_page").classList.contains("active") &&
        (mouseX <= threshold ||
        mouseX >= width - threshold ||
        mouseY <= threshold ||
        mouseY >= height - threshold)
    ) {
        if(!btn.classList.contains("active")) {
            btn.classList.add("active");
        }
    } else {
        if(btn.classList.contains("active")) {
            btn.classList.remove("active");
        }
    }
});


// qr settings
const qr_setting_save_btn = document.getElementById("qr_setting_save_btn")
const qr_code_settings_page = document.getElementById("qr_code_settings");

qr_setting_save_btn.onclick = ()=> {
    const qr_code_custome = document.getElementById("qr_code_custome");
    let inputs = qr_code_settings_page.querySelectorAll("input:not(:checked)");
    let excluded_fields = []
    for(const key of inputs) {
        excluded_fields.push(key.value);
    }
    
    let data = {
        "excluded_fields": excluded_fields,
        "qr_code_custome": qr_code_custome.value,
    }
    createPost("POST","/api/products/qr_setting",data).then(
        result => {
            if (result.status == "error") {
                AM_alert({status:'danger',text:result.msg});
                return;
            }
            AM_alert({status:'success',text:'تنظیمات qr بروزرسانی شد'});
            
        }).catch(err => {
            console.error("Error:", err);
        });
}