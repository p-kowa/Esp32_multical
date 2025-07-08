#include <Arduino.h>
#include <pgmspace.h>
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
  <html>
	<head>
    <meta charset="UTF-8">
		<title>Kamstrup Data</title>
	</head>
	<body style="background-color:powderblue;">
		<style>
  .form-group {
    display: flex;
    flex-direction: column;
    margin-bottom: 10px;
  }

  .form-group label {
    margin-bottom: 4px;
  }

  .settings-form input[type="text"] {
    width: 100%;
    box-sizing: border-box;
  }

  .settings-form {
    max-width: 300px; /* Optional: begrenzt die Formularbreite rechts */
  }
  .data-card {
  background: #f8f8f8;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.08);
  padding: 16px;
  margin-top: 10px;
  max-width: 350px;
}
.data-card .label {
  font-weight: bold;
  display: inline-block;
  width: 140px;
}
.data-card .value {
  display: inline-block;
}
</style>
		<div class="container">
			<h2>Kamstrup Multical Reader</h2>
			<div style="margin-bottom: 24px;"></div>
			<table>
				<tr>
					<th>
						<button onclick="getData('current')">Current</button>
					</th>
					<th>
						<button onclick="getData('daily')">Daily</button>
					</th>
					<th>
						<button onclick="getData('monthly')">Monthly</button>
					</th>
					<th>
						<button onclick="getData('yearly')">Yearly</button>
					</th>
				</tr>
			</table>
			<br>
				<div style="margin-bottom: 24px;"></div>
				<table style="width: 100%;">
					<tr>
						<td style="vertical-align: top; width: 50%;">
							<div id="currentData"></div>
						</td>
						<td style="vertical-align: top; width: 50%;">
							<h4>Settings:</h4>
							<form class="settings-form" onsubmit="saveSettings(); return false;">
								<div class="form-group">
									<label for="fupdate">Update interval:</label>
									<input type="text" id="fupdate" name="fupdate">
								</div>
								<div class="form-group">
									<label for="mqtt_server">MQTT Server:</label>
									<input type="text" id="mqtt_server" name="mqtt_server">
								</div>
								<div class="form-group">
									<label for="mqtt_topic">MQTT Topic:</label>
									<input type="text" id="mqtt_topic" name="mqtt_topic">
								</div>
								<div class="form-group">
									<label for="mqtt_user">MQTT User:</label>
									<input type="text" id="mqtt_user" name="mqtt_user">
								</div>
								<div class="form-group">
									<label for="mqtt_pass">MQTT Password:</label>
									<input type="password" id="mqtt_pass" name="mqtt_pass">
								</div>
								<input type="submit" value="Save">
							</form>
						</td>
				</tr>
			</table>
		<div style="margin-bottom: 24px;"></div>
		<h2>WebSocket Log</h2>
		<div class="log-area" id="log" style="border:2px solid #ccc; height:500px; overflow:auto;"></div>
		<button onclick="clearLog()">Clear Log</button>
		<div class="status" id="status">Connecting to WebSocket...</div>
		<div style="margin-bottom: 24px;"></div>
    <button onclick="resetWifi()">Reset Wifi</button>
		</div>
<script>
      var gateway = `ws://${window.location.hostname}/ws`;
      var websocket;
      window.addEventListener('load', onLoad);
  
      function onLoad(event) {
        initWebSocket();
      }
  
      function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
        websocket.onerror = onError;
      }
  
      function onOpen(event) {
        console.log('WebSocket opened');
        document.getElementById('status').innerText = 'Connected to WebSocket.';
      }
  
      function onClose(event) {
        console.log('WebSocket closed');
        document.getElementById('status').innerText = 'WebSocket disconnected. Retrying in 3s...';
  
        if (websocket) {
          websocket.close();
          websocket = null;
        }
  
        setTimeout(initWebSocket, 3000); // Reconnect on close
      }
  
      function onError(event) {
        console.error('WebSocket error:', event);
        document.getElementById('status').innerText = 'WebSocket error.';
      }
  
      function onMessage(event) {
        var logDiv = document.getElementById('log');
        var message = event.data;
        var span = document.createElement('span');
  
        var timestamp = new Date().toLocaleTimeString('en-US', { hour12: false });
        var timestampText = `[${timestamp}] `;

        try {
          var parts = message.split('|');
          if (parts.length === 2) {
            var directionAndHex = parts[0];
            // var ascii = parts[1]; // ascii variable is declared but not used
            var directionParts = directionAndHex.split(':');
  
            if (directionParts.length === 2) {
              var direction = directionParts[0].trim();
              var hexString = directionParts[1].trim();
              var lineContent = "";

              if (direction === 'PC->EYE') {
                span.className = 'pc-to-eye';
                lineContent = `PC → EYE: ${hexString}`;
              } else if (direction === 'EYE->PC') {
                span.className = 'eye-to-pc';
                lineContent = `EYE → PC: ${hexString}`;
              } else {
                lineContent = message; // Fallback for unexpected format
              }
              span.innerHTML = timestampText + lineContent + "<br>";
            } else {
              // Malformed direction/hex part
              span.innerHTML = timestampText + "Malformed data: " + directionAndHex + "<br>";
            }
          } else {
            // Fallback for unexpected message structure
            span.innerHTML = timestampText + message + "<br>";
          }
        } catch (error) {
          console.error('Error parsing WebSocket message:', error);
          span.innerHTML = timestampText + `Error: ${message}` + "<br>"; // Display the raw message as a fallback
        }
  
        logDiv.appendChild(span);
        logDiv.scrollTop = logDiv.scrollHeight; // Scroll to bottom
      }
  
      function clearLog() {
        document.getElementById('log').innerHTML = '';
      }
     function getData(idvalue) {
  fetch('/callDataHandler?type=' + idvalue)
    .then(response => response.json())
    .then(data => {
      let values;
      switch(idvalue.toLowerCase()) {
        case 'daily':
          values = {
            consumption: data.WaterConsumption + ' l',
            flow: data.WaterFlowMax + ' l/h',
            waterTemp: data.WaterTempAvg + ' °C',
            deviceTemp: data.DeviceTempAvg + ' °C',
            flowLabel: 'Water flow max:',
            waterTempLabel: 'Water temp avg:',
            deviceTempLabel: 'Device temp avg:'
          };
          break;
        case 'monthly':
          values = {
            consumption: data.WaterConsumption + ' l',
            flow: data.WaterFlowMax + ' l/h',
            waterTemp: data.WaterTempAvg + ' °C',
            deviceTemp: data.DeviceTempAvg + ' °C',
            flowLabel: 'Water flow max:',
            waterTempLabel: 'Water temp avg:',
            deviceTempLabel: 'Device temp avg:'
          };
          break;
        case 'yearly':
          values = {
            consumption: data.WaterConsumption + ' l',
            flow: data.WaterFlowMax + ' l/h',
            waterTemp: data.WaterTempMin + ' - ' + data.WaterTempMax + ' °C',
            deviceTemp: data.DeviceTempMin + ' - ' + data.DeviceTempMax + ' °C',
            flowLabel: 'Water flow max:',
            waterTempLabel: 'Water temp range:',
            deviceTempLabel: 'Device temp range:'
          };
          break;
        default: // current
          values = {
            consumption: data.WaterConsumption,
            flow: data.WaterFlow + ' l/h',
            waterTemp: data.WaterTemp + ' °C',
            deviceTemp: data.DeviceTemp + ' °C',
            flowLabel: 'Water flow:',
            waterTempLabel: 'Water temp:',
            deviceTempLabel: 'Device temp:'
          };
      }

      const html = `
        <div class="data-card">
          <div><span class="label">Water consumption:</span> <span class="value">${values.consumption}</span></div>
          <div><span class="label">${values.flowLabel}</span> <span class="value">${values.flow}</span></div>
          <div><span class="label">${values.waterTempLabel}</span> <span class="value">${values.waterTemp}</span></div>
          <div><span class="label">${values.deviceTempLabel}</span> <span class="value">${values.deviceTemp}</span></div>
          <div><span class="label">Serial number:</span> <span class="value">${data.SerialNumber}</span></div>
        </div>
      `;
      document.getElementById("currentData").innerHTML = html;
    })
    .catch(error => {
      console.error('Error:', error);
      document.getElementById("currentData").innerHTML = `<div class="data-card error">Error loading data</div>`;
    });
}
    function resetWifi() {
      fetch('/resetWifi')
        .then(response => response.text())
        .then(data => {
        // Optionally handle the response, e.g. show an alert
        alert(data);
    });
    }
	  function saveSettings() {
        var fupdate = document.getElementById("fupdate").value;
        var mqtt_server = document.getElementById("mqtt_server").value;
        var mqtt_topic = document.getElementById("mqtt_topic").value;
        var mqtt_user = document.getElementById("mqtt_user").value;
        var mqtt_pass = document.getElementById("mqtt_pass").value;

        // Build a query string
        var params = `fupdate=${encodeURIComponent(fupdate)}&mqtt_server=${encodeURIComponent(mqtt_server)}&mqtt_topic=${encodeURIComponent(mqtt_topic)}&mqtt_user=${encodeURIComponent(mqtt_user)}&mqtt_pass=${encodeURIComponent(mqtt_pass)}`;

        fetch('/saveSettings?' + params)
  .then(response => response.text().then(data => {
    if (response.ok) {
      alert("Settings saved: " + data);
    } else {
      alert("Settings not saved: " + data);
    }
  }));
      }
      window.addEventListener('DOMContentLoaded', (event) => {
        fetch('/getSettings')
         .then(response => response.json())
         .then(data => {
           document.getElementById('fupdate').value = data.fupdate;
           document.getElementById('mqtt_server').value = data.mqtt_server;
           document.getElementById('mqtt_topic').value = data.mqtt_topic;
           document.getElementById('mqtt_user').value = data.mqtt_user;
           document.getElementById('mqtt_pass').value = data.mqtt_pass;
     });
});     
    </script>
											</body>
										</html>
  )rawliteral";