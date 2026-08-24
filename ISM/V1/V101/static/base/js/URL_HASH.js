let navs = document.querySelectorAll('.nav_item, .nav_link');
let pages = document.getElementsByClassName('section_page');
let Is_Hash = false;
let Step_1 = false;
if (history.pushState) {
    hashchanged();
}
$(window).bind("hashchange", function() {
    hashchanged();
});

function hashchanged() {
    Is_Hash=false;
    UrlHsh = document.location.hash;
    resetpage();
    if (UrlHsh) {
        let hash_words = UrlHsh.split('#');
        hash_words.shift()
        console.log(hash_words,hash_words.length,hash_words[3]);
        for (let i=0;i < hash_words.length;i++) {
            console.log(hash_words[i]);
            SetPage(hash_words[i]);
        }
    } else {
        console.log("no urlhash")
        SetPage("nav_home");
    }
}

function resetpage() {
    navs = document.querySelectorAll('.nav_item, .nav_link');
    pages = document.getElementsByClassName('section_page');
    for (let i = 0; i < navs.length; i++) {
        try {
            navs[i].classList.remove('active');
            pages[i].classList.remove('active');
        } catch (error) {

        }
    }
}

function SetPage(id) {
    let nav_item = document.querySelector(`[nav_id=${id}]`)
    let page = document.getElementById(id)

    if (nav_item || page) {
        if(nav_item) {nav_item.classList.add('active')};
        if(page) {page.classList.add('active')};
    } else {
        AM_alert({display:true,status: 'warning', text:"این صفحه وجود ندارد"})
        // window.location = "#nav_home";
        return;
    }
    if (nav_item && nav_item.classList.contains("step")) {
        validation_proccess_of_shipment(id,nav_item,page);
    }
    if (nav_item && nav_item.classList.contains("machine_api")) {
        call_last_roll_api(nav_item.getAttribute("machine_id"));
    }
}

function validation_proccess_of_shipment(id,nav_item,page) {
    steps = [
        {"name":"add_truck","index":1},
        {"name":"shipment_detail","index":2},
        {"name":"add_shipment_weight","index":3},
    ]

    let_index_of_steps = steps.length;
    for(const x of steps) {
        let nav_item = document.querySelector(`[nav_id=${x.name}]`)
        let page = document.getElementById(x.name)
        if (x.name == id) {
            let_index_of_steps = x.index;
            nav_item.classList.remove('complete');
            page.classList.remove('complete');
            nav_item.classList.add('active');
            page.classList.add('active');
        } else {
            if (nav_item && page) {
                nav_item.classList.remove('complete');
                page.classList.remove('complete');
            }
            if(let_index_of_steps > x.index) {
                nav_item.classList.add('complete');
                page.classList.add('complete');
            }
        }
    }
}