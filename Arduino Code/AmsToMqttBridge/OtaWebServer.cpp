#include "OtaWebServer.h"

#include <stdint.h>

#include "ArduinoJson.h"
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Update.h>


/* Login page */
static const char* loginIndex =
   "<html>\n"
   "  <head>\n"
   "    <script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>\n"
   "    <script type='text/javascript' src='//ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js'></script>\n"
   "    <script type='text/javascript'>\n"
   "      // Load the Visualization API and the piechart package.\n"
   "      google.charts.load('current', {'packages':['corechart']});\n"
   "\n"
   "      // Set a callback to run when the Google Visualization API is loaded.\n"
   "      google.charts.setOnLoadCallback(drawChart);\n"
   "\n"
   "      function drawChart() {\n"
   "        $.getJSON('powerData', function(json)\n"
   "          {\n"
   "            dots = new Array;\n"
   "\n"
   "            for (i = 0; i < json.length; i++) {\n"
   "              dots.push([new Date(json[i].timestamp*1000), json[i].value]);\n"
   "            }\n"
   "\n"
   "            var data = new google.visualization.DataTable();\n"
   "            data.addColumn('date', 'timestamp');\n"
   "            data.addColumn('number', 'watt');\n"
   "\n"
   "            data.addRows(dots);\n"
   "\n"
   "            var options = {\n"
   "              title: 'Power used (in watts)',\n"
   "              curveType: 'function',\n"
   "              legend: { position: 'bottom' }\n"
   "            };\n"
   "\n"
   "            var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));\n"
   "            chart.draw(data, options);\n"
   "          });\n"
   "      }\n"
   "    </script>\n"
   "  </head>\n"
   "  <body>\n"
   "    <div style='display: block; margin: 0 auto;' align='center'>\n"
   "      <div id='curve_chart' style='width: 900px; height: 500px;'></div>\n"
   "    </div>\n"
   "    <form name='loginForm'>\n"
   "      <table width='20\%' bgcolor='A09F9F' align='center'>\n"
   "        <tr>\n"
   "          <td colspan=2>\n"
   "            <center><font size=4><b>ESP32 Login Page</b></font></center>\n"
   "            <br>\n"
   "          </td>\n"
   "          <br>\n"
   "          <br>\n"
   "        </tr>\n"
   "        <td>Username:</td>\n"
   "        <td><input type='text' size=25 name='userid'><br></td>\n"
   "        </tr>\n"
   "        <br>\n"
   "        <br>\n"
   "        <tr>\n"
   "          <td>Password:</td>\n"
   "          <td><input type='Password' size=25 name='pwd'><br></td>\n"
   "          <br>\n"
   "          <br>\n"
   "        </tr>\n"
   "        <tr>\n"
   "          <td><input type='submit' onclick='checkLoginForm(this.form)' value='Login'></td>\n"
   "        </tr>\n"
   "      </table>\n"
   "    </form>\n"
   "    <script>\n"
   "      function checkLoginForm(form)\n"
   "      {\n"
   "        if(form.userid.value=='admin' && form.pwd.value=='admin')\n"
   "        {\n"
   "          window.open('/serverIndex')\n"
   "        }\n"
   "        else\n"
   "        {\n"
   "          alert('Error Password or Username')/*displays error message*/\n"
   "        }\n"
   "      }\n"
   "    </script>\n"
   "  </body>\n"
   "</html>\n";


/* Server Index Page */
static const char* serverIndex =
   "<html>\n"
   "  <head>\n"
   "    <script type='text/javascript' src='//ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js'></script>\n"
   "  </head>\n"
   "  <body>\n"
   "    <form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>\n"
   "      <input type='file' name='update'>\n"
   "      <input type='submit' value='Update'>\n"
   "    </form>\n"
   "    <div id='prg'>progress: 0%</div>\n"
   "    <script>\n"
   "      $('form').submit(function(e) {\n"
   "        e.preventDefault();\n"
   "        var form = $('#upload_form')[0];\n"
   "        var data = new FormData(form);\n"
   "        $.ajax({\n"
   "          url: '/update',\n"
   "          type: 'POST',\n"
   "          data: data,\n"
   "          contentType: false,\n"
   "          processData:false,\n"
   "          xhr: function() {\n"
   "            var xhr = new window.XMLHttpRequest();\n"
   "            xhr.upload.addEventListener('progress', function(evt) {\n"
   "              if (evt.lengthComputable) {\n"
   "                var per = evt.loaded / evt.total;\n"
   "                $('#prg').html('progress: ' + Math.round(per*100) + '%');\n"
   "              }\n"
   "            }, false);\n"
   "            return xhr;\n"
   "          },\n"
   "          success: function(d, s) {\n"
   "            console.log('success!')\n"
   "          },\n"
   "          error: function (a, b, c) {\n"
   "            console.log('error!')\n"
   "          }\n"
   "        });\n"
   "     });\n"
   "    </script>\n"
   "  </body>\n"
   "</html>\n";


static WebServer server(80);
static HardwareSerial* debugger = NULL;


typedef struct {
    int32_t timestamp;
    int32_t value;
} sample_t;


//  downsampled_start 0 downsampled_end 303 msg.size() 11412 json.memoryUsage() 14544 json.size() 303
//  downsampled_start 21 downsampled_end 20 msg.size() 18889 json.memoryUsage() 23952 json.size() 499
#define DOWNSAMPLED_NUM 500
static sample_t downsampled[DOWNSAMPLED_NUM] = {0};
static int downsampled_start = 0;
static int downsampled_end = 0;


void addSample(const sample_t& sample)
{
    downsampled[downsampled_end] = sample;
    downsampled_end++;

    if (downsampled_end   >= DOWNSAMPLED_NUM) downsampled_end = 0;
    if (downsampled_end == downsampled_start) downsampled_start++;
    if (downsampled_start >= DOWNSAMPLED_NUM) downsampled_start = 0;
}


void downSample(int32_t timestamp, int32_t value)
{
    static sample_t last_added = {0};
    static sample_t previous = {0};
    sample_t sample = {.timestamp = timestamp, .value = value};

    // Always add first sample
    if (last_added.timestamp == 0) {
      last_added = sample;
      previous = sample;
      addSample(sample);
      return;
    }

    // Add sample if value changes 'enough'
    if (abs(last_added.value - sample.value) > 100)
    {
      // Also add previous, to get a correct graph
      if (previous.timestamp != 0) {
        addSample(previous);
      }
      addSample(sample);
      last_added = sample;
      previous.timestamp = 0;
    } // Add sample if 'enough' time has passed
    else if (abs(last_added.timestamp - sample.timestamp) > 15*60)
    {
      addSample(sample);
      last_added = sample;
      previous.timestamp = 0;
    }
    else
    {
      previous = sample;
    }
}


void OtaWebServerSetup(HardwareSerial* debugger_in) {
  debugger = debugger_in;

  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (debugger) debugger->printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        if (debugger) Update.printError(*debugger);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        if (debugger) Update.printError(*debugger);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        if (debugger) debugger->printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        if (debugger) Update.printError(*debugger);
      }
    }
  });

  server.on("/powerData", HTTP_GET, []() {
    DynamicJsonDocument json(30000); // TODO: Need to find aproriate size
    JsonArray array = json.to<JsonArray>();

    for (int i = 1; i < DOWNSAMPLED_NUM - 1; i++)
    {
      int index = (downsampled_start + i - 1) % DOWNSAMPLED_NUM;
      if (downsampled[index].timestamp == 0) break;
      JsonObject sample = array.createNestedObject();
      sample["timestamp"] = downsampled[index].timestamp;
      sample["value"] = downsampled[index].value;
    }

    String msg;
    serializeJson(json, msg);

    server.sendHeader("Connection", "close");
    server.send(200, "application/json", msg);

    if (debugger)
    {
      debugger->print(" downsampled_start ");
      debugger->print(downsampled_start);
      debugger->print(" downsampled_end ");
      debugger->print(downsampled_end);
      debugger->print(" msg.size() ");
      debugger->print(msg.length());
      debugger->print(" json.memoryUsage() ");
      debugger->print(json.memoryUsage());
      debugger->print(" json.size() ");
      debugger->println(json.size());
    }
  });

  server.begin();
}


void OtaWebServerLoop() {
  server.handleClient();
}


void OtaWebServerActivePower(int32_t timestamp, int32_t p)
{
  return downSample(timestamp, p);
}
