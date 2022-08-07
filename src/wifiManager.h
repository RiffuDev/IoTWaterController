const char WM_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<style>
.topnav { 
  overflow: hidden; 
  background-color: #0a5500;
  style: color:white; text-align:center; font-family: Arial;
}
</style>
  <title>ESP Wi-Fi Manager</title>
</head>
<body>
  <div class="topnav">
    <h1 style= "color:white;">Wi-Fi Manager</h1>
  </div>
  <div class="content">
        <form action="/WM" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br></br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br></br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
  </div>
</body>
</html>
)rawliteral";