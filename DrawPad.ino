/*
  ESP32 + SSD1306 OLED
  Live Web Draw Pad

  ESP32 version
  No AsyncTCP
  No AsyncWebServer
  Uses standard WebServer

  OLED:
    SDA -> GPIO 21
    SCL -> GPIO 22
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// WIFI SETTINGS
// ============================================================

const char* WIFI_SSID     = "SSID_NAME";
const char* WIFI_PASSWORD = "SSID_PASS";

// Fallback ESP32 hotspot
const char* AP_SSID       = "Frames_of_Rooban";
const char* AP_PASSWORD   = "12345678";

// ============================================================
// OLED
// ============================================================

#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_ADDR    0x3C
#define OLED_RESET   -1

#define OLED_SDA     21
#define OLED_SCL     22

Adafruit_SSD1306 display(
  OLED_WIDTH,
  OLED_HEIGHT,
  &Wire,
  OLED_RESET
);

// ============================================================
// WEB SERVER
// ============================================================

WebServer server(80);

// ============================================================
// RENDER SETTINGS
// ============================================================

volatile bool needsRender = false;

unsigned long lastRender = 0;

const unsigned long RENDER_INTERVAL_MS = 20;

// ============================================================
// DRAW THICK LINE
// ============================================================

void drawThickLine(
  int x0,
  int y0,
  int x1,
  int y1,
  int size
)
{
  x0 = constrain(
    x0,
    0,
    OLED_WIDTH - 1
  );

  x1 = constrain(
    x1,
    0,
    OLED_WIDTH - 1
  );

  y0 = constrain(
    y0,
    0,
    OLED_HEIGHT - 1
  );

  y1 = constrain(
    y1,
    0,
    OLED_HEIGHT - 1
  );

  if (size <= 1)
  {
    display.drawLine(
      x0,
      y0,
      x1,
      y1,
      SSD1306_WHITE
    );

    needsRender = true;

    return;
  }

  int r = size / 2;

  int dx = abs(x1 - x0);
  int sx = (x0 < x1) ? 1 : -1;

  int dy = -abs(y1 - y0);
  int sy = (y0 < y1) ? 1 : -1;

  int err = dx + dy;

  int e2;

  while (true)
  {
    display.fillCircle(
      x0,
      y0,
      r,
      SSD1306_WHITE
    );

    if (
      x0 == x1 &&
      y0 == y1
    )
    {
      break;
    }

    e2 = 2 * err;

    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }

    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }

  needsRender = true;
}

// ============================================================
// DRAW DOT
// ============================================================

void drawDot(
  int x,
  int y,
  int size
)
{
  x = constrain(
    x,
    0,
    OLED_WIDTH - 1
  );

  y = constrain(
    y,
    0,
    OLED_HEIGHT - 1
  );

  int r = max(
    0,
    size / 2
  );

  display.fillCircle(
    x,
    y,
    r,
    SSD1306_WHITE
  );

  needsRender = true;
}

// ============================================================
// CLEAR OLED
// ============================================================

void clearPanel()
{
  display.clearDisplay();

  needsRender = true;
}

// ============================================================
// HANDLE ONE DRAW COMMAND
//
// CLR
//
// P,x,y,size
//
// L,x0,y0,x1,y1,size
// ============================================================

void handleCommand(
  char* cmd
)
{
  char* type =
    strtok(
      cmd,
      ","
    );

  if (!type)
  {
    return;
  }

  // ----------------------------------------------------------
  // CLEAR
  // ----------------------------------------------------------

  if (
    strcmp(
      type,
      "CLR"
    ) == 0
  )
  {
    clearPanel();

    return;
  }

  // ----------------------------------------------------------
  // LINE
  // ----------------------------------------------------------

  if (
    strcmp(
      type,
      "L"
    ) == 0
  )
  {
    char* a =
      strtok(
        NULL,
        ","
      );

    char* b =
      strtok(
        NULL,
        ","
      );

    char* c =
      strtok(
        NULL,
        ","
      );

    char* d =
      strtok(
        NULL,
        ","
      );

    char* e =
      strtok(
        NULL,
        ","
      );

    if (
      a &&
      b &&
      c &&
      d &&
      e
    )
    {
      drawThickLine(
        atoi(a),
        atoi(b),
        atoi(c),
        atoi(d),
        atoi(e)
      );
    }

    return;
  }

  // ----------------------------------------------------------
  // DOT
  // ----------------------------------------------------------

  if (
    strcmp(
      type,
      "P"
    ) == 0
  )
  {
    char* a =
      strtok(
        NULL,
        ","
      );

    char* b =
      strtok(
        NULL,
        ","
      );

    char* c =
      strtok(
        NULL,
        ","
      );

    if (
      a &&
      b &&
      c
    )
    {
      drawDot(
        atoi(a),
        atoi(b),
        atoi(c)
      );
    }

    return;
  }
}

// ============================================================
// HANDLE BATCH OF COMMANDS
//
// Example:
//
// P,10,20,2;L,10,20,30,40,2;L,30,40,50,50,2
// ============================================================

void handleMessage(
  char* data,
  size_t len
)
{
  static char buffer[700];

  if (
    len >= sizeof(buffer)
  )
  {
    len =
      sizeof(buffer) - 1;
  }

  memcpy(
    buffer,
    data,
    len
  );

  buffer[len] = '\0';

  char* saveptr = nullptr;

  char* token =
    strtok_r(
      buffer,
      ";",
      &saveptr
    );

  while (token)
  {
    handleCommand(token);

    token =
      strtok_r(
        NULL,
        ";",
        &saveptr
      );
  }
}

// ============================================================
// WEB PAGE
// ============================================================

const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(

<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width,
           initial-scale=1,
           maximum-scale=1,
           user-scalable=no"
>

<title>ESP32 OLED Draw Pad</title>

<style>

:root {
  --bg: #0a0d10;
  --panel: #12171c;
  --panel2: #161c22;
  --border: #293138;
  --accent: #6ee7d8;
  --text: #e6edf0;
  --muted: #7c8b94;
  --warning: #ffb454;
}

* {
  box-sizing: border-box;
  -webkit-tap-highlight-color: transparent;
}

html,
body {
  margin: 0;
  min-height: 100%;
}

body {
  background: var(--bg);
  color: var(--text);

  font-family:
    Arial,
    sans-serif;

  display: flex;

  flex-direction: column;

  align-items: center;

  padding: 20px 16px 30px;

  user-select: none;
}

.topbar {
  width: 100%;
  max-width: 640px;

  display: flex;

  justify-content: space-between;

  align-items: center;

  margin-bottom: 18px;
}

.brand {
  font-family: monospace;

  font-size: 15px;
}

.status {
  font-family: monospace;

  font-size: 11px;

  color: var(--muted);

  padding: 6px 10px;

  border: 1px solid var(--border);

  border-radius: 20px;
}

.status.live {
  color: var(--accent);
}

.device {
  width: 100%;
  max-width: 640px;

  background:
    linear-gradient(
      180deg,
      var(--panel),
      var(--panel2)
    );

  border: 1px solid var(--border);

  border-radius: 18px;

  padding: 20px;
}

.screen-wrap {
  background: #000;

  padding: 14px;

  border: 1px solid var(--border);

  border-radius: 6px;
}

canvas {
  display: block;

  width: 100%;

  height: auto;

  aspect-ratio: 2 / 1;

  background: #000;

  image-rendering: pixelated;

  touch-action: none;

  cursor: crosshair;
}

.controls {
  width: 100%;
  max-width: 640px;

  margin-top: 16px;

  background: var(--panel);

  border: 1px solid var(--border);

  border-radius: 14px;

  padding: 16px;

  display: flex;

  flex-direction: column;

  gap: 14px;
}

.row {
  display: flex;

  align-items: center;

  gap: 12px;
}

label {
  font-family: monospace;

  font-size: 11px;

  color: var(--muted);

  min-width: 70px;
}

input[type=range] {
  flex: 1;
}

#penVal {
  font-family: monospace;

  min-width: 35px;
}

button {
  font-family: monospace;

  font-size: 12px;

  padding: 12px 16px;

  border-radius: 9px;

  border: 1px solid #4a3a24;

  background: transparent;

  color: var(--warning);

  cursor: pointer;

  width: 100%;
}

</style>

</head>


<body>

<div class="topbar">

  <div class="brand">
    DRAW · PAD
  </div>

  <div
    class="status live"
    id="status"
  >
    ● ready
  </div>

</div>


<div class="device">

  <div class="screen-wrap">

    <canvas
      id="pad"
      width="128"
      height="64"
    ></canvas>

  </div>

</div>


<div class="controls">

  <div class="row">

    <label>
      PEN SIZE
    </label>

    <input
      type="range"
      id="penSize"
      min="1"
      max="8"
      value="2"
    >

    <span id="penVal">
      2px
    </span>

  </div>


  <button id="clearBtn">
    CLEAR PANEL
  </button>

</div>


<script>

const canvas =
  document.getElementById("pad");

const ctx =
  canvas.getContext("2d");


ctx.fillStyle = "#000";

ctx.fillRect(
  0,
  0,
  canvas.width,
  canvas.height
);


let penSize =
  parseInt(
    document.getElementById(
      "penSize"
    ).value
  );


const penInput =
  document.getElementById(
    "penSize"
  );


const penVal =
  document.getElementById(
    "penVal"
  );


penInput.addEventListener(
  "input",
  function()
  {
    penSize =
      parseInt(
        penInput.value
      );

    penVal.textContent =
      penSize + "px";
  }
);


// ========================================================
// SEND COMMANDS TO ESP32
// ========================================================

let queue = [];

let sending = false;


function queueSend(command)
{
  queue.push(command);
}


// Send accumulated commands
// approximately every 30 ms

setInterval(
  async function()
  {
    if (
      sending ||
      queue.length === 0
    )
    {
      return;
    }

    sending = true;

    const data =
      queue.join(";");

    queue = [];


    try
    {
      await fetch(
        "/cmd?data=" +
        encodeURIComponent(data),
        {
          method: "GET",
          cache: "no-store"
        }
      );
    }
    catch(error)
    {
      console.log(
        "ESP32 communication error:",
        error
      );
    }

    sending = false;

  },
  30
);


// ========================================================
// CONVERT TOUCH → OLED COORDINATES
// ========================================================

function toCanvasPoint(
  clientX,
  clientY
)
{
  const rect =
    canvas.getBoundingClientRect();


  let x =
    Math.round(
      (
        clientX -
        rect.left
      )
      /
      rect.width
      *
      canvas.width
    );


  let y =
    Math.round(
      (
        clientY -
        rect.top
      )
      /
      rect.height
      *
      canvas.height
    );


  x =
    Math.max(
      0,
      Math.min(
        canvas.width - 1,
        x
      )
    );


  y =
    Math.max(
      0,
      Math.min(
        canvas.height - 1,
        y
      )
    );


  return [x, y];
}


// ========================================================
// DRAW LOCALLY
// ========================================================

function localDot(
  x,
  y,
  size
)
{
  ctx.fillStyle =
    "#fff";

  ctx.beginPath();

  ctx.arc(
    x,
    y,
    Math.max(
      0.5,
      size / 2
    ),
    0,
    Math.PI * 2
  );

  ctx.fill();
}


function localLine(
  x0,
  y0,
  x1,
  y1,
  size
)
{
  ctx.strokeStyle =
    "#fff";

  ctx.lineWidth =
    size;

  ctx.lineCap =
    "round";

  ctx.lineJoin =
    "round";

  ctx.beginPath();

  ctx.moveTo(
    x0,
    y0
  );

  ctx.lineTo(
    x1,
    y1
  );

  ctx.stroke();

  localDot(
    x1,
    y1,
    size
  );
}


// ========================================================
// DRAWING
// ========================================================

let drawing = false;

let lastX = 0;

let lastY = 0;


canvas.addEventListener(
  "pointerdown",
  function(e)
  {
    e.preventDefault();

    canvas.setPointerCapture(
      e.pointerId
    );


    const [x, y] =
      toCanvasPoint(
        e.clientX,
        e.clientY
      );


    drawing = true;

    lastX = x;

    lastY = y;


    localDot(
      x,
      y,
      penSize
    );


    queueSend(
      "P," +
      x +
      "," +
      y +
      "," +
      penSize
    );
  }
);


canvas.addEventListener(
  "pointermove",
  function(e)
  {
    if (!drawing)
    {
      return;
    }

    e.preventDefault();


    const [x, y] =
      toCanvasPoint(
        e.clientX,
        e.clientY
      );


    if (
      x === lastX &&
      y === lastY
    )
    {
      return;
    }


    localLine(
      lastX,
      lastY,
      x,
      y,
      penSize
    );


    queueSend(
      "L," +
      lastX +
      "," +
      lastY +
      "," +
      x +
      "," +
      y +
      "," +
      penSize
    );


    lastX = x;

    lastY = y;
  }
);


function stopDrawing(e)
{
  if (!drawing)
  {
    return;
  }

  drawing = false;

  try
  {
    canvas.releasePointerCapture(
      e.pointerId
    );
  }
  catch(error)
  {
  }
}


canvas.addEventListener(
  "pointerup",
  stopDrawing
);

canvas.addEventListener(
  "pointercancel",
  stopDrawing
);


// ========================================================
// CLEAR
// ========================================================

document
  .getElementById("clearBtn")
  .addEventListener(
    "click",
    function()
    {
      ctx.fillStyle =
        "#000";

      ctx.fillRect(
        0,
        0,
        canvas.width,
        canvas.height
      );


      queue = [];

      queueSend(
        "CLR"
      );
    }
  );

</script>

</body>

</html>

)HTMLPAGE";


// ============================================================
// WEB PAGE ROUTE
// ============================================================

void handleRoot()
{
  server.send_P(
    200,
    "text/html",
    INDEX_HTML
  );
}

// ============================================================
// COMMAND ROUTE
// ============================================================

void handleCommandRequest()
{
  if (
    !server.hasArg("data")
  )
  {
    server.send(
      400,
      "text/plain",
      "Missing data"
    );

    return;
  }


  String data =
    server.arg("data");


  if (
    data.length() == 0
  )
  {
    server.send(
      400,
      "text/plain",
      "Empty data"
    );

    return;
  }


  // Prevent oversized requests

  if (
    data.length() >= 650
  )
  {
    server.send(
      400,
      "text/plain",
      "Command too long"
    );

    return;
  }


  char buffer[700];


  data.toCharArray(
    buffer,
    sizeof(buffer)
  );


 handleMessage(
    buffer,
    strlen(buffer)
);


  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(200);


  Serial.println();
  Serial.println(
    "=============================="
  );

  Serial.println(
    "ESP32 OLED DRAW PAD"
  );

  Serial.println(
    "=============================="
  );


  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  Wire.setClock(
    400000
  );


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  )
  {
    Serial.println(
      "SSD1306 not found!"
    );

    while (true)
    {
      delay(1000);
    }
  }


  Serial.println(
    "OLED detected."
  );


  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(
    0,
    0
  );

  display.println(
    "ESP32 DRAW PAD"
  );

  display.println();

  display.println(
    "Connecting WiFi..."
  );

  display.display();


  // ----------------------------------------------------------
  // WiFi
  // ----------------------------------------------------------

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  Serial.print(
    "Connecting to WiFi"
  );


  unsigned long start =
    millis();

  bool connected = false;


  while (
    millis() - start <
    10000
  )
  {
    if (
      WiFi.status() ==
      WL_CONNECTED
    )
    {
      connected = true;

      break;
    }


    delay(250);

    Serial.print(".");
  }


  Serial.println();


  IPAddress ip;


  // ----------------------------------------------------------
  // Connected to existing WiFi
  // ----------------------------------------------------------

  if (connected)
  {
    ip =
      WiFi.localIP();


    Serial.print(
      "WiFi connected."
    );

    Serial.println();


    Serial.print(
      "IP address: "
    );

    Serial.println(
      ip
    );
  }

  // ----------------------------------------------------------
  // Fallback access point
  // ----------------------------------------------------------

  else
  {
    Serial.println(
      "WiFi failed."
    );

    Serial.println(
      "Starting ESP32 hotspot..."
    );


    WiFi.mode(
      WIFI_AP
    );


    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );


    ip =
      WiFi.softAPIP();


    Serial.print(
      "Hotspot: "
    );

    Serial.println(
      AP_SSID
    );


    Serial.print(
      "Password: "
    );

    Serial.println(
      AP_PASSWORD
    );


    Serial.print(
      "Open: http://"
    );

    Serial.println(
      ip
    );
  }


  // ----------------------------------------------------------
  // OLED network information
  // ----------------------------------------------------------

  display.clearDisplay();

  display.setCursor(
    0,
    0
  );


  if (connected)
  {
    display.println(
      "WiFi connected"
    );
  }
  else
  {
    display.println(
      "Hotspot active"
    );

    display.println(
      AP_SSID
    );
  }


  display.println();

  display.println(
    "Open browser:"
  );

  display.println();

  display.println(
    ip
  );

  display.display();


  // ----------------------------------------------------------
  // Web routes
  // ----------------------------------------------------------

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/cmd",
    HTTP_GET,
    handleCommandRequest
  );


  server.begin();


  Serial.println(
    "HTTP server started."
  );


  Serial.print(
    "Open: http://"
  );

  Serial.println(
    ip
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  server.handleClient();


  unsigned long now =
    millis();


  // Only update OLED at most every 20ms.
  // This prevents excessive I2C traffic.

  if (
    needsRender &&
    (
      now - lastRender >=
      RENDER_INTERVAL_MS
    )
  )
  {
    display.display();

    needsRender = false;

    lastRender = now;
  }
}
