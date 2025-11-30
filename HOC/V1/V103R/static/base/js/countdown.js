var setint = setInterval(Timer, 1000);
var seconds = 60;

// Store countdown interval globally for cleanup
window.countdownInterval = setint;
function Timer() {
  if (document.getElementById("countdown")) {
    if (seconds <= 0) {
      clearInterval(setint);
      document.getElementById("countdown").innerHTML = "00";
      data = { number: document.getElementById('login_number').value, csrfmiddlewaretoken: document.getElementsByName('csrfmiddlewaretoken')[0].value }
      $.ajax({
        type: "post",
        url: "/accounts/delete_random_code/",
        data: data,
        success: function (response) {
          document.getElementById("countdown").classList.add('text-danger');
          document.getElementById("countdown").innerHTML = response;
          document.getElementsByClassName("send_again")[0].style.display = "block";
        },
        error: function () {

        },
      });
    } else {
      document.getElementById("countdown").innerHTML = seconds + " ثانیه";
    }
  }
  seconds -= 1;
}


function SendAgain(num) {
  num = num.toString();
  console.log(`/accounts/send_again/for=${num}`,num.toString());
  // data = { number: num }
  $.ajax({
    type: "get",
    url: `/accounts/send_again/for=${num}`,
    success: function (response) {
      seconds = 60
      document.getElementById("countdown").classList.remove('text-danger');
      setint = setInterval(Timer, 1000);
      window.countdownInterval = setint;
      document.getElementsByClassName("send_again")[0].style.display = "none";
    },
    error: function () {

    },
  });
}