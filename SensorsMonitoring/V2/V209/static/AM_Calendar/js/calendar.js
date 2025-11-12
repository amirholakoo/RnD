/* all right reserved | copyright 2025 | amirmohammad mousavi */
let calendar_active_val = false;
for (const x of document.querySelectorAll('#calendar .calendar_month label')) {
  x.onclick = function() {
    calendar_active_val = !x.classList.contains('disable') ? x.getAttribute('date-data') : false;
    console.log("calendar_active_val",calendar_active_val);
  }
}
let Calendar_Data_Fix = false;
function AMCalendar(year=false,month=false,data_title=false,form=false) {
    Calendar_Data_Fix = false;
    Progress();
    const months = document.querySelectorAll('.calendar_month');
    let already_exist = false;
    let list_created = false;
    def_cal(data_title,year);
    if (already_exist) {
      document.querySelector(`[data-month-title=${data_title}][data-year-title='${year}']`).classList.add('active');
    } else {
      list_created = true;
      const html_place = document.getElementById('calendar_year');+
      $.ajax({
          type: "get",
          url: `/widgets/year=${year}month=${month}?form=${form}`,
          async: false,
          success: function (response) {
            def_cal(response.calendar[0].month[0].name,response.calendar[0].year);
            if (already_exist) {
              list_created = false;
              document.querySelector(`[data-month-title=${data_title}][data-year-title='${year}']`).classList.add('active');
              var xpath = `//option[contains(text(),'${data_title}')]`;
              var matchingElement = document.evaluate(xpath, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
              matchingElement.selected = true;
              xpath = `//option[contains(text(),'${year}')]`;
              matchingElement = document.evaluate(xpath, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
              matchingElement.selected = true;
            } else {
              for(let i =0;i<response.calendar.length;i++) {
                html_place.insertAdjacentHTML('beforeend',`<div class="calendar_month w-100 active" data-month-title="${response.calendar[i].month[0].name}" data-year-title="${response.calendar[i].year}"><div class="d-flex justify-content-right flex-row-reverse flex-wrap days"></div></div>`);
                let days_ = response.calendar[i].month[0].days;
                if(days_[0].title == 'یک‌شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('afterbegin',`<label class="day_item disable"><div class="title">30</div></label>`)
                } else if(days_[0].title == 'دوشنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('afterbegin',`<label class="day_item disable"><div class="title">29</div></label><label class="day_item disable"><div class="title">30</div></label>`)
                } else if(days_[0].title == 'سه‌شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('afterbegin',`<label class="day_item disable"><div class="title">28</div></label><label class="day_item disable"><div class="title">29</div></label><label class="day_item disable"><div class="title">30</div></label>`)
                } else if(days_[0].title == 'چهارشنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('afterbegin',`<label class="day_item disable"><div class="title">27</div></label><label class="day_item disable"><div class="title">28</div></label><label class="day_item disable"><div class="title">29</div></label><label class="day_item disable"><div class="title">30</div></label>`)
                } else if(days_[0].title == 'پنج‌شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('afterbegin',`<label class="day_item disable"><div class="title">26</div></label><label class="day_item disable"><div class="title">27</div></label><label class="day_item disable"><div class="title">28</div></label><label class="day_item disable"><div class="title">29</div></label><label class="day_item disable"><div class="title">30</div></label>`)
                } else if(days_[0].title == 'جمعه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('afterbegin',`<label class="day_item disable"><div class="title">25</div></label><label class="day_item disable"><div class="title">26</div></label><label class="day_item disable"><div class="title">27</div></label><label class="day_item disable"><div class="title">28</div></label><label class="day_item disable"><div class="title">29</div></label><label class="day_item disable"><div class="title">30</div></label>`)
                }
                for(let x=0;x<response.calendar[i].month[0].days.length;x++) {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`
                  <label for="day${year}-${month}-${x+1}" date-data="${year}-${month}-${x+1}" data-toggle="modal" data-target="#selecttime" class="calendar_day day_item ${response.calendar[i].month[0].days[x].title == 'جمعه' ? 'off' : ''} ${response.calendar[i].month[0].days[x].active ? 'bg-warning' : ''} ${ !(response.calendar[i].month[0].days[x].title) ? 'disable' : ''}" title="${response.calendar[i].month[0].days[x].title}">
                      <div class="title">
                      ${x+1}
                      </div>
                  </label>
                  `);
                  if(response.calendar[i].month[0].days[x].title) {
                    html_place.querySelector('.calendar_month.active > div .calendar_day:last-child > .title').insertAdjacentHTML('beforeend', `
                    ${response.form ? `<input type="radio" value="${year}-${month}-${x+1}" name="day" id="day${year}-${month}-${x+1}">` : ''}
                    `);
                  }
                };
                if(days_[days_.length - 1].title == 'پنج‌شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`<label class="day_item disable"><div class="title">1</div></label>`)
                } else if(days_[days_.length - 1].title == 'چهارشنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`<label class="day_item disable"><div class="title">1</div></label><label class="day_item disable"><div class="title">2</div></label>`)
                } else if(days_[days_.length - 1].title == 'سه‌شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`<label class="day_item disable"><div class="title">1</div></label><label class="day_item disable"><div class="title">2</div></label><label class="day_item disable"><div class="title">3</div></label>`)
                } else if(days_[days_.length - 1].title == 'دوشنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`<label class="day_item disable"><div class="title">1</div></label><label class="day_item disable"><div class="title">2</div></label><label class="day_item disable"><div class="title">3</div></label><label class="day_item disable"><div class="title">4</div></label>`)
                } else if(days_[days_.length - 1].title == 'یک‌شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`<label class="day_item disable"><div class="title">1</div></label><label class="day_item disable"><div class="title">2</div></label><label class="day_item disable"><div class="title">3</div></label><label class="day_item disable"><div class="title">4</div></label><label class="day_item disable"><div class="title">5</div></label>`)
                } else if(days_[days_.length - 1].title == 'شنبه') {
                  html_place.querySelector('.calendar_month.active > div').insertAdjacentHTML('beforeend',`<label class="day_item disable"><div class="title">1</div></label><label class="day_item disable"><div class="title">2</div></label><label class="day_item disable"><div class="title">3</div></label><label class="day_item disable"><div class="title">4</div></label><label class="day_item disable"><div class="title">5</div></label><label class="day_item disable"><div class="title">6</div></label>`)
                }
              }
              
              list_created = true;
              Calendar_Data_Fix = true;
            }
            Progress(false);
            console.log(Calendar_Data_Fix);
          },
          error: function (error) {
            Progress(false);
            Calendar_Data_Fix = false;
          },
        });
    }
  
    function def_cal(m,y) {
      for(let x = 0;x<months.length;x++) {
        if(months[x].classList.contains('active')){
          months[x].classList.remove('active');
        };
        if(months[x].getAttribute('data-month-title') == m && months[x].getAttribute('data-year-title') == y.toString()){
          already_exist = true;
        };
      };
      if(!data_title) {
        data_title = m;
        year = y;
      }
    }
  
      Progress(false);
  }