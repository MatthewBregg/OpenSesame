const char* GetHtml() {
  const char* html = R"(
  
  <!DOCTYPE html> <html>
  <meta content="text/html;charset=utf-8" http-equiv="Content-Type">
  <meta content="utf-8" http-equiv="encoding">
  <head><meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>!!!Open Sesame!!!</title>
  <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
  body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}
  .button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}
  .button-on {background-color: #3498db;}
  .button-on:active {background-color: #2980b9;}
  .button-off {background-color: #34495e;}
  .button-off:active {background-color: #2c3e50;}
  p {font-size: 14px;color: #888;margin-bottom: 10px;}
  </style>
  </head>
  <body>
  <a class="button" onclick="handleDoor();" id="door_button">Open</a>
  <h1> OTA Enabled: <span id="ota"></span></h1>
  <h1> HEAP Data: <span id="free_heap"></span>, <span id="max_heap"</span></h1>
  <h1> Door was Last Opened At: <span id="opened_at"></span></h1>
  <h1> Uptime: <span id="uptime"></span></h1>
  <script>
    setInterval(function() {
      // Call a function repetatively with second interval
      getData();
    }, 1000); 

    function set_door_button_appearence(on) {
      let doorButton = document.getElementById("door_button");
      if (on) {
            doorButton.classList.remove('button-on');
            doorButton.classList.add('button-off');
          } else {
            doorButton.classList.remove('button-off');
            doorButton.classList.add('button-on');
          }
    }
    
    function handleDoor() {
      set_door_button_appearence(true);
      var xhttp = new XMLHttpRequest();
      // Why 4?
      // This gets called 4 times total.  When readyState is 4, that is the completed call.
      // See the below link for more info.
      // https://stackoverflow.com/questions/30522565/what-is-meaning-of-xhr-readystate-4/30522680
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          let json = JSON.parse(this.responseText);
          console.log("door opened successfully!");  
          console.log(this.responseText);
        } else if (this.readyState == 4 && this.status != 200) {
          console.log("Error! Failed to open door!");
        }
      };
      console.log("Opened Door!");
      xhttp.open("GET", "open_door_ajax", true);
      xhttp.send();
      // Return the door button to inactive in .5 seconds.
      setTimeout(()=> { set_door_button_appearence(false); }, 500);
    }
   
    function getData() {
      var xhttp = new XMLHttpRequest();
      // Why 4?
      // https://stackoverflow.com/questions/30522565/what-is-meaning-of-xhr-readystate-4/30522680
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          let json = JSON.parse(this.responseText);
          //console.log(this.responseText);
          let opened_at = json["opened_at"];
          let uptime = json["uptime"];
          let ota = json["ota"];
          let free_heap = json["free_heap"];
          let max_heap = json["max_heap"];
         
          document.getElementById("opened_at").innerHTML = opened_at;
          document.getElementById("uptime").innerHTML = uptime;
          document.getElementById("ota").innerHTML = ota;
          document.getElementById("free_heap").innerHTML = free_heap;
          document.getElementById("max_heap").innerHTML = max_heap;
        }
      };
      xhttp.open("GET", "status_json", true);
      xhttp.send();
    }
    getData();
    set_door_button_appearence(false);
  </script>
  <div> Version 1.4 </div>
  </body>
  )";
  return html;
}