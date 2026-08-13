# Building a Wireless OLED Drawing Pad with ESP32

I wanted to build something with the ESP32 that combined hardware and web technology instead of making another basic LED or sensor project.

So I decided to build a small wireless drawing pad.

The idea was simple:

**Open a webpage → draw on the screen → see the drawing appear on an OLED connected to the ESP32.**

The interesting part is that the ESP32 itself hosts the webpage. There is no separate web server, backend application, or external service involved.

The final system consists of an ESP32, a 128×64 SSD1306 OLED, and a browser.

## What I Built

<img width="2004" height="1509" alt="IMG_20260809_154945529" src="https://github.com/user-attachments/assets/bb81547c-84df-47fd-862d-98587b16ceab" />


The ESP32 performs three main jobs:

1. Hosts the drawing interface.
2. Receives drawing commands from the browser.
3. Renders those commands on the OLED.

The overall architecture looks like this:

```text
                 Wi-Fi
                   │
                   ▼
          ┌─────────────────┐
          │      ESP32      │
          │                 │
          │   Web Server    │
          │       │         │
          │       ▼         │
          │ Drawing Commands│
          │       │         │
          │       ▼         │
          │   OLED Driver   │
          └───────┬─────────┘
                  │ I²C
                  ▼
          ┌───────────────┐
          │ SSD1306 OLED  │
          │   128 × 64    │
          └───────────────┘
```

The ESP32 can either connect to an existing Wi-Fi network or create its own hotspot if the connection fails. The fallback access point is built directly into the firmware.

## Hardware

The hardware is deliberately simple.

I used:

* ESP32 development board
* 0.96-inch SSD1306 OLED
* Jumper wires
* USB cable
* Phone or computer for controlling the drawing pad

The OLED communicates with the ESP32 using I²C.

### OLED Wiring

| OLED | ESP32   |
| ---- | ------- |
| VCC  | 3.3V    |
| GND  | GND     |
| SDA  | GPIO 21 |
| SCL  | GPIO 22 |

These are the GPIO pins configured in my code. The OLED is configured as a 128×64 display with I²C address `0x3C`.

## Software

The ESP32 firmware uses:

```text
WiFi.h
WebServer.h
Wire.h
Adafruit_GFX.h
Adafruit_SSD1306.h
```

One deliberate choice in this version is that I am **not using AsyncTCP or ESPAsyncWebServer**.

The project uses the standard Arduino ESP32 `WebServer` implementation.

This keeps the project relatively straightforward and avoids introducing asynchronous web-server dependencies just for this application.

## Step 1 — Connect the OLED

The first step is connecting the OLED to the ESP32.

The OLED uses I²C, so only SDA and SCL are required for communication.

```text
ESP32              SSD1306
─────              ───────
3.3V   ──────────  VCC
GND    ──────────  GND
GPIO21 ──────────  SDA
GPIO22 ──────────  SCL
```

The firmware initializes I²C using these pins and sets the I²C clock to 400 kHz.

## Step 2 — Initialize the OLED

After initializing I²C, the firmware initializes the SSD1306 display.

If the OLED cannot be detected, the ESP32 prints an error through the serial monitor and stops rather than continuing with a non-functional display.

When the display is successfully detected, the ESP32 initially displays its startup information.

This gives me a simple way to confirm that the OLED is working before dealing with the network.

## Step 3 — Connect the ESP32 to Wi-Fi

The ESP32 first attempts to connect to a configured Wi-Fi network.

The connection attempt has a 10-second timeout.

If the connection succeeds, the ESP32 obtains its local IP address.

For example:

```text
WiFi connected.

IP address:
192.168.x.x
```

That address is then displayed on the OLED so I know where to open the drawing interface.

## Step 4 — Fallback Hotspot

I also added a fallback mechanism.

If the ESP32 cannot connect to the configured Wi-Fi network within the timeout period, it switches to access-point mode and creates its own Wi-Fi hotspot.

This means the project doesn't completely depend on an existing Wi-Fi network.

The workflow becomes:

```text
Try existing Wi-Fi
       │
       ├── Success
       │      ↓
       │   Get IP
       │
       └── Failure
              ↓
       Create ESP32 hotspot
              ↓
        Connect to ESP32
```

The OLED also displays whether the ESP32 is connected to Wi-Fi or running as a hotspot, along with the IP address to open in the browser.

## Step 5 — Host the Drawing Website Directly from the ESP32

This is one of the parts I found most interesting.

The HTML page is stored directly inside the ESP32 firmware using a `PROGMEM` string.

That means the ESP32 doesn't need to download the webpage from another server.

The HTML, CSS, and JavaScript are part of the firmware itself.

When I open the ESP32's IP address, the root route sends this page to the browser.

```text
Browser
   │
   │ GET /
   ▼
ESP32
   │
   ▼
Embedded HTML
   │
   ▼
Drawing Interface
```

The web interface has a simple dark UI with a 128×64 drawing canvas, pen-size control, connection status, and a button to clear the panel.

## Step 6 — Create a 128×64 Drawing Canvas

The browser canvas is also configured as:

```text
Width  = 128
Height = 64
```

This is intentional.

The canvas has the same dimensions as the OLED, so the browser coordinates can map directly to the OLED's pixel coordinates.

The canvas is then scaled visually using CSS so that it is easier to use on a phone or computer.

Internally, however, it remains a 128×64 coordinate system.

This makes the coordinate conversion much simpler.

## Step 7 — Convert Touch Coordinates

A phone screen might display the canvas at a much larger size than 128×64 pixels.

For example, the physical canvas displayed in the browser might be 320 pixels wide.

But internally we still want:

```text
Browser position
       ↓
128 × 64 coordinate
       ↓
OLED pixel
```

The JavaScript calculates the position of the pointer relative to the canvas and scales it to the actual canvas dimensions.

This allows the same drawing interface to work with different screen sizes.

## Step 8 — Draw Locally in the Browser

I don't wait for the ESP32 to respond before showing the drawing on the webpage.

The browser immediately draws the stroke locally.

This makes the interface feel responsive.

The JavaScript has separate functions for drawing dots and lines on the browser canvas.

At the same time, a command describing the drawing is placed into a queue for the ESP32.

This creates two simultaneous operations:

```text
User draws
    │
    ├──────────────► Browser Canvas
    │                 Immediate feedback
    │
    └──────────────► Command Queue
                      ↓
                    ESP32
                      ↓
                    OLED
```

## Step 9 — Send Drawing Commands

Instead of sending an entire image to the ESP32 every time the user moves the pointer, I send compact drawing commands.

There are three main commands.

### Point

```text
P,x,y,size
```

For example:

```text
P,20,30,2
```

This tells the ESP32 to draw a point at `(20,30)` with a pen size of `2`.

### Line

```text
L,x0,y0,x1,y1,size
```

For example:

```text
L,20,30,40,35,2
```

This represents a line from `(20,30)` to `(40,35)`.

### Clear

```text
CLR
```

This tells the ESP32 to clear the OLED.

The ESP32 parses these commands in `handleCommand()`.

## Step 10 — Batch Multiple Commands

Sending an HTTP request for every single movement would be inefficient.

Instead, the browser puts drawing commands into a queue.

Approximately every 30 milliseconds, the queued commands are combined into a single request.

For example:

```text
P,10,20,2;L,10,20,30,40,2;L,30,40,50,50,2
```

The ESP32 then receives the batch and processes each command separately.

The request is sent to:

```text
/cmd?data=...
```

using an HTTP GET request.

This reduces the number of HTTP requests generated while drawing.

## Step 11 — Process the Commands on the ESP32

The `/cmd` endpoint receives the drawing data.

The ESP32 checks that:

* The `data` parameter exists.
* The data isn't empty.
* The request isn't larger than the allowed size.

The command is then converted into a character buffer and passed to the command parser.

The parser separates multiple commands using semicolons:

```text
Command 1 ; Command 2 ; Command 3
```

Each command is then handled individually.

## Step 12 — Render Lines on the OLED

For a simple one-pixel line, the code uses the SSD1306 library's `drawLine()` function.

For thicker lines, I implemented my own line-rendering logic.

The line is traversed point by point, and a filled circle is drawn at each position.

This creates a thicker continuous stroke instead of simply drawing a thin one-pixel line.

This is useful because the browser allows the pen size to be changed between 1 and 8 pixels.

## Step 13 — Render Dots

When the user initially touches or clicks the canvas, the browser sends a point command.

The ESP32 handles this separately using `drawDot()`.

The point is converted into a filled circle based on the selected pen size.

This prevents the beginning of a stroke from appearing as a missing pixel or gap.

## Step 14 — Clear the Display

The clear button performs two operations.

First, JavaScript clears the browser canvas.

Then it sends:

```text
CLR
```

to the ESP32.

The ESP32 receives this command and calls `clearDisplay()` on the OLED.

So both displays are cleared together.

## Step 15 — Prevent Excessive OLED Updates

One issue with an OLED is that constantly refreshing the entire display can generate unnecessary I²C traffic.

Instead of immediately calling `display.display()` every time a drawing operation occurs, I use a `needsRender` flag.

When the display needs updating, the flag is set.

The main loop then refreshes the OLED at most once every 20 milliseconds.

The main loop looks conceptually like this:

```text
Handle web requests
       ↓
Check whether OLED needs updating
       ↓
Has 20 ms passed?
       │
       ├── No → continue
       │
       └── Yes
             ↓
        Update OLED
```

This keeps the drawing responsive without unnecessarily refreshing the display after every individual drawing operation.

## The Complete Data Flow

Putting everything together:

```text
                PHONE / LAPTOP
                      │
                      │ Wi-Fi
                      ▼
              ┌───────────────┐
              │     ESP32     │
              │               │
              │  Web Server   │
              └───────┬───────┘
                      │
                 GET /cmd
                      │
                      ▼
              Command Parser
                      │
          ┌───────────┼───────────┐
          │           │           │
          ▼           ▼           ▼
         CLR          P            L
          │           │           │
          ▼           ▼           ▼
       Clear       Draw Dot    Draw Line
                      │           │
                      └─────┬─────┘
                            ▼
                      OLED Buffer
                            │
                       Every 20 ms
                            ▼
                     SSD1306 OLED
```

## Final Result

The final result is a small wireless drawing system controlled entirely through a web browser.

I can connect to the ESP32, open its IP address, and use my phone or computer as the drawing interface.

The drawing appears locally in the browser and is simultaneously converted into compact commands that are sent to the ESP32.

The ESP32 processes those commands and renders the result on the physical OLED.

The entire system is self-contained:

```text
Browser
   +
ESP32 Web Server
   +
SSD1306 OLED
```

No external backend is required.

## What I Learned

This project ended up teaching me much more than just how to control an OLED.

I worked with:

* ESP32 Wi-Fi
* Embedded web servers
* HTML
* CSS
* JavaScript
* Browser pointer events
* Canvas API
* HTTP requests
* Coordinate conversion
* Command parsing
* I²C
* SSD1306 OLED graphics
* Real-time data transfer
* Buffering and batching
* Embedded memory considerations

The part I found most interesting was the communication between the browser and the microcontroller.

Instead of treating the ESP32 as just a device controlled by buttons and sensors, I was able to turn it into a small web application server.

## What I Would Improve Next

There are several directions I could take this project next.

I could add:

* Undo and redo
* Save/load drawings
* Download drawings as images
* Multiple drawing tools
* Eraser mode
* Better mobile controls
* WebSocket communication
* Persistent storage
* Multiple OLED displays
* Drawing synchronization between multiple clients

The biggest technical improvement would probably be moving from repeated HTTP GET requests to WebSockets. That would make the communication more suitable for continuous real-time drawing.

## Conclusion

This project started with a simple idea: draw something in a browser and display it on an OLED.

But implementing it required connecting several different areas of technology.

The browser handles the user interface and drawing input.

The ESP32 handles the web server and command processing.

The OLED provides the physical output.

Together, they form a small embedded application that combines web development, networking, and hardware.

That's what made this project interesting to me—not just making an OLED display something, but making a **web interface control physical hardware in real time using an ESP32**.
