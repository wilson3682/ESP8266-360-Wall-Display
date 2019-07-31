
//    https://www.youtube.com/watch?v=GpseDql0HmQ&t=54s
//    https://www.hastebin.com/oduvupiyim.cs

#include<FastLED.h>

#define LED_PIN     4
#define CLOCK_PIN   5
#define LED_TYPE    WS2812B
#define COLOR_ORDER BGR
#define MILLI_AMPERE      15000    // IMPORTANT: set here the max milli-Amps of your power supply 5V 2A = 2000
#define FRAMES_PER_SECOND  200    // here you can control the speed. 
int ledMode = 4;                  // this is the starting palette
const uint8_t kMatrixWidth  = 40;
const uint8_t kMatrixHeight = 9;
const bool    kMatrixSerpentineLayout = true;
int BRIGHTNESS =           128;   // this is half brightness 
int new_BRIGHTNESS =       128;   // shall be initially the same as brightness

#include <ESP8266WiFi.h>
// comes with Huzzah installation. Enter in Arduino settings:
// http://arduino.esp8266.com/package_esp8266com_index.json

const char* ssid = "Put your SSID here";
const char* password = "Put your password here"; //after booting, check the serial monitor for the chip's assigned ip address if using webserver mode.
unsigned long ulReqcount;
unsigned long ulReconncount;

WiFiServer server(80);  // Create an instance of the server on Port 80

// This example combines two features of FastLED to produce a remarkable range of
// effects from a relatively small amount of code.  This example combines FastLED's 
// color palette lookup functions with FastLED's Perlin/simplex noise generator, and
// the combination is extremely powerful.
//
// You might want to look at the "ColorPalette" and "Noise" examples separately
// if this example code seems daunting.
//
// 
// The basic setup here is that for each frame, we generate a new array of 
// 'noise' data, and then map it onto the LED matrix through a color palette.
//
// Periodically, the color palette is changed, and new noise-generation parameters
// are chosen at the same time.  In this example, specific noise-generation
// values have been selected to match the given color palettes; some are faster, 
// or slower, or larger, or smaller than others, but there's no reason these 
// parameters can't be freely mixed-and-matched.
//
// In addition, this example includes some fast automatic 'data smoothing' at 
// lower noise speeds to help produce smoother animations in those cases.
//
// The FastLED built-in color palettes (Forest, Clouds, Lava, Ocean, Party) are
// used, as well as some 'hand-defined' ones, and some proceedurally generated
// palettes.


#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

// The leds
CRGB leds[kMatrixWidth * kMatrixHeight];

// The 16 bit version of our coordinates
static uint16_t x;
static uint16_t y;
static uint16_t z;

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward.  Try
// 1 for a very slow moving effect, or 60 for something that ends up looking like
// water.
uint16_t speed = 20; // speed is set dynamically once we've started up

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise iwll be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
uint16_t scale = 30; // scale is set dynamically once we've started up

// This is the array that we keep our computed noise values in
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];
 CRGBPalette16 currentPalette( CRGB::Black );

 CRGBPalette16 targetPalette( CRGB::Black );
uint8_t       colorLoop = 1;

void setup() {
  ulReqcount=0;         // setup globals for Webserver
  ulReconncount=0;
  // Initialize our coordinates to some random values
  x = random16();
  y = random16();
  z = random16();
  Serial.begin(9600);
  delay(1);
  // inital connect
  WiFi.mode(WIFI_STA);
  WiFiStart();
delay(1000); //safety delay
  //LEDS.addLeds<LED_TYPE,LED_PIN,CLOCK_PIN,COLOR_ORDER>(leds,NUM_LEDS);  //Use this for APA102 Led Strips
  LEDS.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds,NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  set_max_power_in_volts_and_milliamps(5, MILLI_AMPERE);
}

void WiFiStart()
{
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());
}

// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  // If we're runing at a low "speed", some 8-bit artifacts become visible
  // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
  // The amount of data smoothing we're doing depends on "speed".
  uint8_t dataSmoothing = 0;
  if( speed < 50) {
    dataSmoothing = 200 - (speed * 4);
  }
  
  for(int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for(int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;
      
      uint8_t data = inoise8(x + ioffset,y + joffset,z);

      // The range of the inoise8 function is roughly 16-238.
      // These two operations expand those values out to roughly 0..255
      // You can comment them out if you want the raw noise data.
      data = qsub8(data,16);
      data = qadd8(data,scale8(data,39));

      if( dataSmoothing ) {
        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
        data = newdata;
      }
      
      noise[i][j] = data;
    }
  }
  
  z += speed;
  
  // apply slow drift to X and Y, just for visual variation.
  x += speed / 8;
  y -= speed / 16;
}

void mapNoiseToLEDsUsingPalette()
{
  static uint8_t ihue=0;
  
  for(int i = 0; i < kMatrixWidth; i++) {
    for(int j = 0; j < kMatrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j][i];
      uint8_t bri =   noise[i][j];

      // if this palette is a 'loop', add a slowly-changing base value
      if( colorLoop) { 
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the 
      // light/dark dynamic range desired
      if( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }

      CRGB color = ColorFromPalette( currentPalette, index, bri);
      leds[XY(i,j)] = color;
    }
  }
  
  ihue+=1;
}

void loop() {
    webserver();
  // Periodically choose a new palette, speed, and scale
  ChangePaletteAndSettingsPeriodically();
      // Crossfade current palette slowly toward the target palette
    //
    // Each time that nblendPaletteTowardPalette is called, small changes
    // are made to currentPalette to bring it closer to matching targetPalette.
    // You can control how many changes are made in each call:
    // - the default of 24 is a good balance
    // - meaningful values are 1-48. 1=veeeeeeeery slow, 48=quickest
    // - "0" means do not change the currentPalette at all; freeze
//  uint8_t maxChanges = 7;
//  nblendPaletteTowardPalette( currentPalette, targetPalette, maxChanges);

  // generate noise data
  fillnoise8();
  
  // convert the noise data to colors in the LED array
  // using the current palette
  mapNoiseToLEDsUsingPalette();

  show_at_max_brightness_for_power();
  delay_at_max_brightness_for_power(1000/FRAMES_PER_SECOND); 
}
void webserver() {   /// complete web server (same for access point) ////////////////////////////////////////////////////////
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) 
  {
    return;
  }
  
  // Wait until the client sends some data
  Serial.println("new client");
  unsigned long ultimeout = millis()+250;
  while(!client.available() && (millis()<ultimeout) )
  {
    delay(1);
  }
  if(millis()>ultimeout) 
  { 
    Serial.println("client connection time-out!");
    return; 
  }
  
  // Read the first line of the request

  String sRequest = client.readStringUntil('\r');
  Serial.println(sRequest);
  client.flush();
  
  // stop client, if request is empty
  if(sRequest=="")
  {
    Serial.println("empty request! - stopping client");
    client.stop();
    return;
  }
  
  // get path; end of path is either space or ?
  // Syntax is e.g. GET /?pin=MOTOR1STOP HTTP/1.1
  String sPath="",sParam="", sCmd="";
  String sGetstart="GET ";
  int iStart,iEndSpace,iEndQuest;
  iStart = sRequest.indexOf(sGetstart);
//  Serial.print("iStart "); 
//  Serial.println(iStart); 

  if (iStart>=0)
  {
    iStart+=+sGetstart.length();
//  Serial.print("iStart + sGetstart "); 
//  Serial.println(iStart); 
    iEndSpace = sRequest.indexOf(" ",iStart);
//  Serial.print("iEndSpace "); 
//  Serial.println(iEndSpace); 
    iEndQuest = sRequest.indexOf("?",iStart);
//  Serial.print("iEndQuest "); 
//  Serial.println(iEndQuest);     
    // are there parameters?
    if(iEndSpace>0)
    {
      if(iEndQuest>0)
      {
        // there are parameters
        sPath  = sRequest.substring(iStart,iEndQuest);
        sParam = sRequest.substring(iEndQuest,iEndSpace);
      }
      else
      {
        // NO parameters
        sPath  = sRequest.substring(iStart,iEndSpace);
      }
    }
  }
  
  //////////////////////////////////////////////////////////////////////////////////
  // output parameters to serial, you may connect e.g. an Arduino and react on it //
  //////////////////////////////////////////////////////////////////////////////////
  if(sParam.length()>0)
  {
    int iEqu=sParam.indexOf("=");
    if(iEqu>=0)
    {
      sCmd = sParam.substring(iEqu+1,sParam.length());
      Serial.print("We are in output Parameters, value is: ");
      Serial.println(sCmd);
      char carray[4];                                // values 0..255 = 3 digits; array = digits + 1
      sCmd.toCharArray(carray, sizeof(carray));      // convert char to the array
      new_BRIGHTNESS = atoi(carray);                 // atoi() converts an ascii character array to an integer
      if (new_BRIGHTNESS == 0) {new_BRIGHTNESS = BRIGHTNESS; }   // if something else is selected (no change in brightness)
      BRIGHTNESS = new_BRIGHTNESS;                 // works not this way 
         FastLED.setBrightness(new_BRIGHTNESS);      // that's how the new value is assigned  
      Serial.print("new Brightness: ");
      Serial.println(new_BRIGHTNESS);
    }
  }
  
  //////////////////////////////
  // format the html response //
  //////////////////////////////
  String sResponse,sHeader;
  
  ///////////////////////////////
  // 404 for non-matching path //
  ///////////////////////////////
  if(sPath!="/")
  {
    sResponse="<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
    
    sHeader  = "HTTP/1.1 404 Not found\r\n";
    sHeader += "Content-Length: ";
    sHeader += sResponse.length();
    sHeader += "\r\n";
    sHeader += "Content-Type: text/html\r\n";
    sHeader += "Connection: close\r\n";
    sHeader += "\r\n";
  }
  //////////////////////////
  // format the html page //
  //////////////////////////
  else
  {
    ulReqcount++;
    sResponse  = "<html><head><title>ESP_FastLED_Access_Point</title></head><body>";
//    sResponse += "<font color=\"#FFFFF0\"><body bgclror=\"#000000\">";  
    sResponse += "<font color=\"#FFFFF0\"><body bgcolor=\"#151B54\">";  
    sResponse += "<FONT SIZE=-1>"; 
    sResponse += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">";
    sResponse += "<h1>ESP FastLED Noise Plus Palette</h1><br>";
    sResponse += " <h2>Light Controller</h2>";

/*  this creates a list with ON / OFF buttons 
    // &nbsp is a non-breaking space; moves next character over
    sResponse += "<p>Rainbow &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"?pin=FUNCTION1ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION1OFF\"><button>--OFF--</button></a><br>";
    sResponse += "<p>Rainbow Glitter<a href=\"?pin=FUNCTION2ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION2OFF\"><button>--OFF--</button></a><br>";
    sResponse += "<p>Confetti &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"?pin=FUNCTION3ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION3OFF\"><button>--OFF--</button></a><br>";
    sResponse += "<p>Sinelon &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"?pin=FUNCTION4ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION4OFF\"><button>--OFF--</button></a><br>";
    sResponse += "<p>Juggle&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"?pin=FUNCTION5ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION5OFF\"><button>--OFF--</button></a></p>";
    sResponse += "<p>BPM&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"?pin=FUNCTION6ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION6OFF\"><button>--OFF--</button></a></p>";
    sResponse += "<p>Function 7&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"?pin=FUNCTION7ON\"><button>--ON--</button></a>&nbsp;<a href=\"?pin=FUNCTION7OFF\"><button>--OFF--</button></a></p><br>";
*/
//  This is a nice drop down menue
    sResponse += "<FONT SIZE=+1>";
    sResponse += "<form>";
//    sResponse += "Select Animation<br>";
    sResponse += "<p>Select:</p>";
    sResponse += "<select name=\"sCmd\" size=\"7\" >";
    sResponse += "<option value=\"FUNCTION1OFF\">All OFF</option>";
    sResponse += "<option value=\"FUNCTION1ON\"selected>2 Random Colors</option>";
    sResponse += "<option value=\"FUNCTION2ON\">3 Random Colors</option>";
    sResponse += "<option value=\"FUNCTION3ON\">Purple & Green</option>";
    sResponse += "<option value=\"FUNCTION4ON\">Black & White</option>";
    sResponse += "<option value=\"FUNCTION5ON\">Forest Colors</option>";
    sResponse += "<option value=\"FUNCTION6ON\">Cloud Colors</option>";
    sResponse += "<option value=\"FUNCTION7ON\">Lava Colors</option>";
    sResponse += "<option value=\"FUNCTION8ON\">Ocean Colors</option>";
    sResponse += "<option value=\"FUNCTION9ON\">Party Colors</option><br>";
    sResponse += "</select>";
    sResponse += "<br><br>";
    sResponse += "<input type= submit>";
    sResponse += "</form>";
//    sResponse += "<FONT SIZE=-1>";

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Slider          this works, however I got http://192.168.4.1/sCmd?FUNCTION_200=80  and the page was not found
//                 I needed to take the FUNCTION_200=80 apart and call only FUNCTION_200 and assign
//                 the value (=80) in "react on parameters" (line 512) to new_BRIGHTNESS

sResponse += "</p>";
sResponse += "<form action=\"?sCmd\" >";    // ?sCmd forced the '?' at the right spot  
sResponse += "<BR>Brightness &nbsp;&nbsp";  // perhaps we can show here the current value
sResponse += round(new_BRIGHTNESS /2.5);    // this is just a scale depending on the max value; round for better readability
sResponse += " %";
sResponse += "<BR>";
sResponse += "<input style=\"width:200px; height:50px\" type=\"range\" name=\"=FUNCTION_200\" id=\"cmd\" value=\"";   // '=' in front of FUNCTION_200 forced the = at the right spot
sResponse += BRIGHTNESS;
sResponse += "\" min=10 max=250 step=10 onchange=\"showValue(points)\" />";
sResponse += "<BR><BR>";
sResponse += "<input type=\"submit\">";
sResponse += "</form>";
sResponse += "<p>";
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

 sResponse += "<FONT SIZE=-1>";


    /////////////////////////
    // react on parameters //
    /////////////////////////
    if (sCmd.length()>0)
    {
      // write received command to html page
 //     sResponse += "Command: " + sCmd + "<BR>";
      
      // switch the animiation (based on your choice in the case statement (main loop) 
      if(sCmd.indexOf("FUNCTION1ON")>=0)
      {
        Serial.println("1 ON");
        ledMode = 2;
      }
      else if(sCmd.indexOf("FUNCTION1OFF")>=0)
      {
        Serial.println("1 OFF");
        ledMode = 1;
      }

      if(sCmd.indexOf("FUNCTION2ON")>=0)
      {
         Serial.println("2 ON");
        ledMode = 3;
      }
      else if(sCmd.indexOf("FUNCTION2OFF")>=0)
      {
        Serial.println("2 OFF");
        ledMode = 1;
      }

      if(sCmd.indexOf("FUNCTION3ON")>=0)
      {
        Serial.println("3 ON");
        ledMode = 4;

      }
      else if(sCmd.indexOf("FUNCTION3OFF")>=0)
      {
        Serial.println("3 OFF");
        ledMode = 1;

      }
      if(sCmd.indexOf("FUNCTION4ON")>=0)
      {
        Serial.println("4 ON");
        ledMode = 5;

      }
      else if(sCmd.indexOf("FUNCTION4OFF")>=0)
      {
        Serial.println("4 OFF");
        ledMode = 1;

      }
      if(sCmd.indexOf("FUNCTION5ON")>=0)
      {
         Serial.println("5 ON");
        ledMode = 6;

      }
      else if(sCmd.indexOf("FUNCTION5OFF")>=0)
      {
        Serial.println("5 OFF");
        ledMode = 1;

      }

      if(sCmd.indexOf("FUNCTION6ON")>=0)
      {
         Serial.println("6 ON");
        ledMode = 7;

      }
      else if(sCmd.indexOf("FUNCTION6OFF")>=0)
      {
        Serial.println("6 OFF");
        ledMode = 1;

      }
      if(sCmd.indexOf("FUNCTION7ON")>=0)
      {
        Serial.println("7 ON");
        ledMode = 8;

      }
      else if(sCmd.indexOf("FUNCTION7OFF")>=0)
      {
         Serial.println("7 OFF");
        ledMode = 1;

      }
           if(sCmd.indexOf("FUNCTION8ON")>=0)
      {
        Serial.println("8 ON");
        ledMode = 9;

      }
      else if(sCmd.indexOf("FUNCTION8OFF")>=0)
      {
         Serial.println("8 OFF");
        ledMode = 1;

      }
         if(sCmd.indexOf("FUNCTION9ON")>=0)
      {
        Serial.println("9 ON");
        ledMode = 10;

      }
      else if(sCmd.indexOf("FUNCTION9OFF")>=0)
      {
         Serial.println("9 OFF");
        ledMode = 1;

      }
         if(sCmd.indexOf("FUNCTION10ON")>=0)
      {
        Serial.println("10 ON");
        ledMode = 11; //case 10: matrix()

      }
      else if(sCmd.indexOf("FUNCTION10OFF")>=0)
      {
         Serial.println("10 OFF");
        ledMode = 1;

      }


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// oh well, I was a bit frustrated. Came up with the idea to make
// 10 digits increments and let the URL (not) react on it.
// However, I was able to assign a new_BRIGHTNESS value; 
// what after all serves the purpose. Maybe someone comes up with 
// a more ellegant way - HOPEFULLY
// (more than 400 have downloaded my code but nobody felt the need  
// to help. wtf - this is my very first attempt on HTML ! 
// Guys, I'm a simple electrician, so PLEASE help  :(          )

// do not call a new page when the slider is moved, but assign the new value
// to BRIGHTNESS (this is done in "output parameters to serial", line 314

      if(sCmd.indexOf("FUNCTION_200=20")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=30")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=40")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=50")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=60")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=70")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=80")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=90")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=100")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=110")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=120")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=130")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=140")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=150")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=160")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=170")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=180")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=190")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=200")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=210")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=220")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=230")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=240")>=0) { }
      if(sCmd.indexOf("FUNCTION_200=250")>=0) { }
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    } // end sCmd.length()>0
   
    
//    sResponse += "<FONT SIZE=-2>";
//    sResponse += "<BR>clicks on page ="; 
//    sResponse += ulReqcount;
    sResponse += "<BR>";
    sResponse += "<BR>";
    sResponse += "Powered by FastLED<BR><BR>";
    sResponse += "<FONT SIZE=-2>";
    sResponse += "<font color=\"#FFDE00\">";
    sResponse += "Noise Plus Palette by Mark Kriegsman<BR>";
    sResponse += "Webserver by Stefan Thesen<BR>";
    sResponse += "<font color=\"#FFFFF0\">";
    sResponse += "Gyro Gearloose &nbsp;&nbsp;Feb 2016<BR>";
    sResponse += "</body></html>";
    sHeader  = "HTTP/1.1 200 OK\r\n";
    sHeader += "Content-Length: ";
    sHeader += sResponse.length();
    sHeader += "\r\n";
    sHeader += "Content-Type: text/html\r\n";
    sHeader += "Connection: close\r\n";
    sHeader += "\r\n";
  }
  
  // Send the response to the client
  client.print(sHeader);
  client.print(sResponse);

  
  // and stop the client
  client.stop();
  Serial.println("Client disonnected");  
  }  // end of web server


// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can write
// code that creates color palettes on the fly.

// 1 = 5 sec per palette
// 2 = 10 sec per palette
// etc
#define HOLD_PALETTES_X_TIMES_AS_LONG 2

void ChangePaletteAndSettingsPeriodically()
{
  uint8_t maxChanges = 10; 
  nblendPaletteTowardPalette( currentPalette, targetPalette, maxChanges);
  //uint8_t secondHand = ((millis() / 1000) / HOLD_PALETTES_X_TIMES_AS_LONG) % 60; //not used with webserver
  //static uint8_t lastSecond = 99;                                                 //not used with webserver
  
    if (ledMode != 999) {
      switch (ledMode) {
      case  1: all_off(); break;
      case  2: SetupRandomPalette(); speed = 3; scale = 25; colorLoop = 1; break; //2-color palette
      case  3: SetupRandomPalette_g(); speed = 3; scale = 25; colorLoop = 1; break; //3-color palette
      case  4: SetupPurpleAndGreenPalette(); speed = 6; scale = 20; colorLoop = 1; break;
      case  5: SetupBlackAndWhiteStripedPalette(); speed = 4; scale = 20; colorLoop = 1; ; break;
      case  6: targetPalette = ForestColors_p; speed = 3; scale = 20; colorLoop = 0; break;
      case  7: targetPalette = CloudColors_p; speed =  4; scale = 20; colorLoop = 0; break;
      case  8: targetPalette = LavaColors_p;  speed =  8; scale = 19; colorLoop = 0; break;
      case  9: targetPalette = OceanColors_p; speed = 6; scale = 25; colorLoop = 0;  break;
      case  10: targetPalette = PartyColors_p; speed = 3; scale = 20; colorLoop = 1; break;
      }
  }
}

// This function generates a random palette that's a gradient
// between four different colors.  The first is a dim hue, the second is 
// a bright hue, the third is a bright pastel, and the last is 
// another bright hue.  This gives some visual bright/dark variation
// which is more interesting than just a gradient of different hues.

// LED animations ###############################################################################
void all_off() {  fill_solid( targetPalette, 16, CRGB::Black);}
void SetupRandomPalette()//two colors
{
  EVERY_N_MILLISECONDS( 8000 ){ //new random palette every 8 seconds. Might have to wait for the first one to show up
  CRGB black  = CRGB::Black;
  CRGB random1 = CHSV( random8(), 255, 255);
  CRGB random2 = CHSV( random8(), 255, 255);
  targetPalette = CRGBPalette16( 
//                      CRGB( random8(), 255, 32), 
//                      CHSV( random8(), 255, 255),
                      random1,random1,black, black, 
                      random2,random2,black, black,
                      random1,random1,black, black, 
                      random2,random2,black, black);
//                      CHSV( random8(), 128, 255), 
//                      CHSV( random8(), 255, 255), );
}
}
void SetupRandomPalette_g()//three colors
{
  EVERY_N_MILLISECONDS( 8000 ){ //new random palette every 8 seconds
  CRGB black  = CRGB::Black;
  CRGB random1 = CHSV( random8(), 255, 255);
  CRGB random2 = CHSV( random8(), 200, 100);
  CRGB random3 = CHSV( random8(), 150, 200);
  targetPalette = CRGBPalette16( 
//                      CRGB( random8(), 255, 32), 
//                      CHSV( random8(), 255, 255),
                      random1,random1,black, black, 
                      random2,random2,black, random3,
                      random1,random1,black, black, 
                      random2,random2,black, random3);
//                      CHSV( random8(), 128, 255), 
//                      CHSV( random8(), 255, 255), );
}
}
// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
  CRGB purple = CHSV( HUE_PURPLE, 255, 255);
  CRGB green  = CHSV( HUE_GREEN, 255, 255);
  CRGB black  = CRGB::Black;
  
  targetPalette = CRGBPalette16( 
   green,  green,  black,  black,
   purple, purple, black,  black,
   green,  green,  black,  black,
   purple, purple, black,  black );
}
// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
  // 'black out' all 16 palette entries...
  fill_solid( targetPalette, 16, CRGB::Black);
  // and set every eighth one to white.
  currentPalette[0] = CRGB::White;
 // currentPalette[4] = CRGB::White;
  currentPalette[8] = CRGB::White;
//  currentPalette[12] = CRGB::White;
}
//
// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
//
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  if( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }
  if( kMatrixSerpentineLayout == true) {
    if( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }
  return i;
}
void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
  uint8_t brightness = 255;
  
  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, colorIndex + sin8(i*16), brightness);
    colorIndex += 3;
  }
}
