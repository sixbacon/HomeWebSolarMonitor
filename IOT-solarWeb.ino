/*  Measure solar radiation with an ESP8266  ESP12E / NodeMCU
Solar Radiation sensor - TSL2561 with filter
using Sparkfun library for sensor
displays values on a web site
BG 18/1/18 
*/

#include <ESP8266WiFi.h>                         //ESP specifics
#include <SparkFunTSL2561.h>                     // library for sensor
#include <Wire.h>                                // library for I2C interface

//   *********************  SETUP BEFORE UPLOADING ****************************************************//
// the following data needs to be set to match your Wi-Fi
const char* ssid     = "**********";             // Wi-Fi network
const char* password = "**********";             // Wi-Fi password
IPAddress ip(192,168, 0, 186);                   // static IP address for this ESP unit
IPAddress gateway(192,168,0,1);                  // router IP to set Static IP address
IPAddress subnet(255,255,255,0);                 // needed to set static IP address 
//   **************************************************************************************************//

//   .................... SETUP TO RECEIVE CALIBRATED READINGS ........................................//         
// set the calibration for your sensor
float solcalib=1000.0/3000;                              // 1000/(count in full sun)  with filter in place,  units (W/m/m) / count 
//   ..................................................................................................//


//I2C connections
#define SDA 0                                    // GPIO0 / D3
#define SCL 2                                    // GPIO2 / D4

// Create an SFE_TSL2561 object to access the sensor
SFE_TSL2561 light;

const float solarMax = 1000.0;                      // maximum solar radiation likely in kW m/m, max value on dashboard

void setup_solarirradiance() {
  boolean gain = 0;                              // Gain setting, 0 = X1, 1 = X16; solar sensor as in bright sun so use 0
  unsigned int sitime;                           // Integration time as read from sensor - solar sensor
  unsigned char time = 0;                        // time 13.7 milliseconds
   
  // Initialize the SFE_TSL2561 library
  light.begin();
  // Get factory ID from solar sensor:
  unsigned char ID;
  if (light.getID(ID))                           // just checking a TSL2561 sensor is there
  {
    Serial.print("Solar sensor factory ID: 0X");
    Serial.print(ID,HEX);
    Serial.println(", should be 0X5X");
  }
  else // error
  {
    Serial.println("Solar sensor not found");
  }
  // set up the configuration - see data sheet 
  Serial.print("Solar set timing...");
  light.setTiming(gain,time,sitime);
  // To start taking measurements, power up the sensor:
  Serial.println(sitime);
  delay(1000);
  Serial.println("solar powerup...");
  light.setPowerUp();
  // The sensor will now gather light during the integration time.
  // After the specified time, you can retrieve the result from the sensor.
  // Once a measurement occurs, another integration period will start.
}

float reading_totalsolar() {
  unsigned int sdata0, sdata1;                     // hold raw data from sensor
  const float error =9999.0;
  // Retrieve the data from the solar sensor:
  if (light.getData(sdata0,sdata1))
  {
    Serial.print("Raw solar sensor data - ");
    Serial.println(sdata0);
    return solcalib*sdata0;    
  }
  else
  {
    Serial.println("solar sensor error");
    return error;
  } 
}

void startWIFI(void)  {                           // use to establish Wi-Fi connection as network may be reset or lost   
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);                     // connect to Wi-Fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  WiFi.config(ip, gateway, subnet);               //set to static ip have to do after initial setup 
  Serial.println("");
  Serial.println("WiFi connected");
}

// web server 
WiFiServer myserver(80);                          // Instance of web server class to handle TCP connections 
String req;                                       // stores request containing message sent by client to this web server  

void setupWebServer()
{
  myserver.begin();                               // start the web server
}


// produce webpage with a dashboard display 
// ****************  This first section needs to be set for particular application  ****************************//
// only change the values assigned
const String messageTag = "ESPsolar";             // only messages containing with this string will be responed to  
const String dashUnit   = " W/m/m";               // Units of displayed value
const String dashTitle  = "Solar Irradiance";     // Title for dashboard
float dashFSD           = solarMax;               // Full scale value of display 

float dashCollectValue(){                         // must have this name, should return a number between 0 and dashFSD
  float value = reading_totalsolar();                   // application specific
  if (value < 0.0)     { value = 0.0; }           // range check
  if (value > dashFSD) { value = dashFSD; }
  return value;
}


// ****************  End of application specific section  *******************************************************//

// ****************   This section should not need to change ****************************************************//

String tempS;                                     // temporary string to resolve problem with including " char
int tvalue;                                       // intermediate calculation values
float value ,temp;                                // intermediate calculation values
const int xA = 50;                                // coords of start of arc inside
const int yA = 149;
const int xB = 28;                                // coords of start of arc outside
const int yB = 170;
int xC,yC,xD,yD;                                  // dynamic coords of end of arc, outer(C) and inner(D)
const int outerR = 100;                           // outer radius of dashboard
const int innerR = 70;                            // inner radius of dashboard
const int dashDeg = 270;                          // full scale arc in degrees
const int startDeg = 225;                         // starting angle clockwise from up
const int centreOffsetX =100;                     // centre of dash for screen coordinates
const int centreOffsetY =100;
const float degToRad = 0.017453;                  // conversion degrees to radians  

void watchForMessage()  {
  WiFiClient client = myserver.available();       // set up a client instance on contact
  if (client) {                                   // a client is contacting the web server   
    Serial.println("new client");
    while (client.connected()) {                   
      if (client.available()) {                   // a request string is coming into the buffer       
        String req = client.readStringUntil('\r');// read the request from the buffer
        Serial.println(req);
        client.flush();                           // remove anything else from buffer so next message can be received 
        //process the message from the client  
        if (req.indexOf(messageTag) != -1) {      // does it have the correct identifying string
          Serial.println("ESPmessage received");
          // take a reading and prepare for display
          // call the function to collect the value to be displayed and put in range 0 to 100
          value = 100.0/dashFSD * dashCollectValue();   // call the function to collect the value to be displayed
          // first calculate the coordinates on the outer and inner parts of the arc end of dashboard
          // broken into steps to aid understanding for first
          tvalue = (int)(value/100.0*dashDeg+startDeg) % 360;  // calculate finishing angle
          temp = sin(degToRad*tvalue) * (outerR+2);           // calculate the distance from centre
          xC = (int)temp +centreOffsetX;                  // correct to screen coordinates
          // the rest done in one step 
          yC = centreOffsetY - (int)(cos(degToRad*tvalue) * outerR);
          xD = (int)(sin(degToRad*tvalue) * innerR)+centreOffsetX;
          yD = centreOffsetY - (int)(cos(degToRad*tvalue) * innerR);

          // this section sends the HTML and SVG code to draw the web page to display a dashboard showing the value
          client.println("HTTP/1.1 200 OK");              // standard response saying request successfully received
          client.println("Content-Type: text/html");      // standard header
          client.println();                               // blank line marks end of header - it has to be there
          client.println("<!DOCTYPE HTML>");              // sets format of what follows
          client.println("<html>");
          client.println("<head><title>ESP Solar Irradiance Monitor</title>");
          tempS = "<meta http-equiv=\"refresh\" content=\"1\" >";  // tell browser to refresh page every 5 seconds
          client.println(tempS);                   // need to separate into these two lines to include " char in string
          client.println("</head>");
          client.println("<body bgcolor=\"Olive\"><body>");  // set background of web page
          client.println("<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" height=\"" + String(3*centreOffsetY)
                         + "\" width=\"" + String(3*centreOffsetY) + "\">");  
          client.println("<circle cx=\"" + String(centreOffsetX)+ "\" cy=\"" + String(centreOffsetY)
                         + "\" r=\"" + String(outerR -1)
                         + "\" stroke=\"black\" stroke-width=\"1\" fill=\"black\" />");
          if (value >(200.0/3.0)){  // have to select the correct arc quadrants to display, changeover at 180 deg
            client.println("<path d=\"M"+String(xB)+","+String(yB-1)+" A" +String(outerR)+","+String(outerR)+" 0 1 1 "
                           +String(xC)+","+String(yC)+" L"+String(xD)+","+String(yD)
                           +" A"+String(innerR)+","+String(innerR)+" 0 0 1 "+String(xA)+","+String(yA)+" Z\" fill=\"green\"/>");
          }
          else
          {           // select different quadrant when arc is greater than 180 deg i.e. input is 67
              client.println("<path d=\"M"+String(xB)+","+String(yB)+" A" +String(outerR)+","+String(outerR)+" 0 0 1 "
                             +String(xC)+","+String(yC)+" L"+String(xD)+","+String(yD)
                             +" A"+String(innerR)+","+String(innerR)+" 0 0 0 "+String(xA)+","+String(yA)+" Z\" fill=\"green\"/>");
            
          }
          //draw centre circle and values
          client.println("<circle cx=\"" + String(centreOffsetX) + "\" cy=\"" + String(centreOffsetY)
                         + "\" r=\"" + String(innerR)
                         + "\" stroke=\"silver\" stroke-width=\"1\" fill=\"silver\" />");
          client.println("<text x=\"" + String(centreOffsetX)+ "\" y=\"" + String(centreOffsetY+15)
                         + "\" style=\" font-size: 48px; text-anchor: middle\" fill=\"black\">"
                          +String((int)(value*dashFSD/100))+"</text>");
          client.println("<text x=\"" + String(centreOffsetX)+ "\" y=\"" + String(2*centreOffsetY+20)
                         + "\" style=\" font-size: 16px; text-anchor: middle\" fill=\"black\">"
                          +dashTitle+"</text>");
          client.println("<text x=\"" + String(centreOffsetX)+ "\" y=\"" + String(2*centreOffsetY+40)
                         + "\" style=\" font-size: 12px; text-anchor: middle\" fill=\"black\">FSD "
                          +String((int)dashFSD)+dashUnit+"</text>");

          //client.println("Sorry, your browser does not support inline SVG.");
          client.println("</svg>");
          client.println("</html>");
          delay(1);                                 // let background Wi-Fi operations proceed
          client.stop();                            // client told all data transmitted for this reading
        }   
        else {
          Serial.println("Not a valid message");
          // send a blank message so overwrites any message received previously
          client.println("HTTP/1.1 200 OK");        // standard response saying request successfully received
          client.println("Content-Type: text/html");// standard header
          client.println("Connection: close");      // close connection after message sent
          client.println();                         // blank line marks end of header - it has to be there
          client.println("<!DOCTYPE HTML>");        // sets format of what follows
          client.println("<html>");
          client.println("<head><title>                  </title></head>");
          client.println("<body>");
          client.print("<p>       Invalid message                             </p> ");
          client.println("</body>");
          client.println("</html>");
          delay(1);
          client.stop();     
          delay(1);
        }         
      }
    }
  }  
}

//  *****************************  End of web page preparation  **********************************************//

void setup()
{
  Serial.begin(115200);                           // used for testing
  delay(10);
  Wire.begin(SDA,SCL);                            // set up I2C
  Serial.println("ESP186 solar web");
  Serial.println("About to start WifI");
  startWIFI();                                    // start up wifi link
  setupWebServer();                               // start the web server
  setup_solarirradiance();                        //SOLAR SENSOR
  Serial.println("Waiting for client to contact...");
}

void loop()
{
  watchForMessage();                              // wait for contact from a client
}



