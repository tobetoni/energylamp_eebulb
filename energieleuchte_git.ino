#include <NTPClient.h>
#include <time.h>
#include <WiFiUdp.h>
#include <tinyxml2.h>
#include <String.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <HX710B.h>



using namespace tinyxml2;

// network
const char* ssid = "xxx";
const char* password = "xxx";

// Time Server details
const char* ntpServer = "pool.ntp.org";
const long gmtOffsetSeconds = 0;  // time zone offset (in seconds)
const int daylightOffsetSeconds = 0;  //  daylight saving offset (in seconds)

// API token
const char* apiToken = "xxx";

// API details
const char* apiEndpoint = "https://web-api.tp.entsoe.eu/api";
const char* documentType = "A75";
const char* processType = "A16";

const char* inDomain = "10Y1001A1001A83F";

unsigned long lastApiRequestTime = 0;
const unsigned long apiRequestInterval = 900000; // 15 minutes in milliseconds

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffsetSeconds, daylightOffsetSeconds);

const int DOUT = 35;   //sensor data pin
const int SCLK  = 21;   //sensor clock pin
const int PWR  = 32;   //sensor power vcc

HX710B pressure_sensor; 


const int pumpIn =  27;   // digital pin 27 for pump input
const int pumpOut =  13;   // digital pin 13 for pump output
const int valveIn =  19;   // digital pin 19 for valve input
const int valveOut =  5;   // digital pin 5 for valve output

const int lightPin = 16;  // Initialize digital pin 16 for LED output

float targetPressureMax = 540.0;  // target pressure when body is full
float targetPressureMin = 490.0;  // target pressure when body is empty
float pressureThresholdSwitch = 545.0;  // Pressure increase threshold when body is pushed

bool isBodyPushed = false; 

float percRenew = 0.0; // Declare percRenew and initialize with a default value

bool firstLoop = true;



void setup() {
  Serial.begin(57600);
  delay(1000);
  pressure_sensor.begin(DOUT, SCLK);


  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to Wi-Fi. IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();


  pinMode(PWR, OUTPUT);
  digitalWrite(PWR, HIGH);
  pressure_sensor.begin(DOUT, SCLK);




  // pump pins as an output
  pinMode(pumpIn, OUTPUT);
  pinMode(pumpOut, OUTPUT);
  pinMode(valveIn, OUTPUT);
  pinMode(valveOut, OUTPUT);

  
  pinMode(lightPin, OUTPUT);
  digitalWrite(lightPin, LOW);
}



void loop() {
  unsigned long currentTime = millis();
  
  // Check if enough time has passed to make the next API request
  if (currentTime - lastApiRequestTime >= apiRequestInterval || firstLoop == true) {
    firstLoop = false;
    lastApiRequestTime = currentTime;

    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    time_t timeInSeconds = static_cast<time_t>(epochTime);
    struct tm timeinfo;
    gmtime_r(&timeInSeconds, &timeinfo);

    // Adjust the time to 1 hour and 15 minutes ago for period start
    time_t periodStartEpoch = epochTime - (75 * 60);
    struct tm* periodStartTimeinfo = localtime(&periodStartEpoch);

    // Flatten the minutes to the nearest quarter hour
    int quarterHours = periodStartTimeinfo->tm_min / 15;
    periodStartTimeinfo->tm_min = quarterHours * 15;

    char periodStart[13];
    strftime(periodStart, sizeof(periodStart), "%Y%m%d%H%M", periodStartTimeinfo);

    // Adjust the time to 1 hour ago for period end
    time_t periodEndEpoch = epochTime - (60 * 60);
    struct tm* periodEndTimeinfo = localtime(&periodEndEpoch);

    // Flatten the minutes to the nearest quarter hour
    quarterHours = periodEndTimeinfo->tm_min / 15;
    periodEndTimeinfo->tm_min = quarterHours * 15;

    char periodEnd[13];
    strftime(periodEnd, sizeof(periodEnd), "%Y%m%d%H%M", periodEndTimeinfo);



    //creaTE API Link
    String apiLink = String(apiEndpoint) + "?securityToken=" + String(apiToken) +
                    "&documentType=" + String(documentType) +
                    "&processType=" + String(processType) +
                    //"&psrType=" + String(psrType) +
                    "&in_Domain=" + String(inDomain) +
                    "&periodStart=" + String(periodStart) +
                    "&periodEnd=" + String(periodEnd);

    Serial.println(apiLink);

    // Create an HTTPClient object
    HTTPClient http;

    // Send the GET request to the API
    http.begin(apiLink);
    int httpResponseCode = http.GET();

    // Check the response code
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      // Get the XML response
      String xmlResponse = http.getString();

      // Create a TinyXML2 XMLDocument object
      tinyxml2::XMLDocument xmlDoc;

      Serial.println("xmlData:");
      Serial.println(xmlResponse);

      // Parse the XML response
      tinyxml2::XMLError parsingError = xmlDoc.Parse(xmlResponse.c_str());

      if (parsingError == tinyxml2::XML_SUCCESS) {
        Serial.println("XML parsed successfully");

        // Define variables to store quantity values for each psrType
        int quantityB01 = 0;
        int quantityB02 = 0;
        int quantityB03 = 0;
        int quantityB04 = 0;
        int quantityB05 = 0;
        int quantityB06 = 0;
        int quantityB07 = 0;
        int quantityB08 = 0;
        int quantityB09 = 0;
        int quantityB10 = 0;
        int quantityB10_neg = 0;
        int quantityB11 = 0;
        int quantityB12 = 0;
        int quantityB13 = 0;
        int quantityB14 = 0;
        int quantityB20 = 0;
        int quantityB15 = 0;
        int quantityB16 = 0;
        int quantityB16_neg = 0;      
        int quantityB17 = 0;
        int quantityB18 = 0;
        int quantityB19 = 0;



        // Extract psrType, position, and quantity values
        XMLElement* timeSeriesElement = xmlDoc.FirstChildElement("GL_MarketDocument")->FirstChildElement("TimeSeries");
        while (timeSeriesElement) {
          XMLElement* mktPSRTypeElement = timeSeriesElement->FirstChildElement("MktPSRType");
          if (mktPSRTypeElement) {
            XMLElement* psrTypeElement = mktPSRTypeElement->FirstChildElement("psrType");
            if (psrTypeElement) {
              const char* psrType = psrTypeElement->GetText();
              if (strlen(psrType) > 0) {
                Serial.print("psrType: ");
                Serial.println(psrType);

                XMLElement* mRIDElement = timeSeriesElement->FirstChildElement("mRID");
                if (mRIDElement) {
                  const char* mRID = mRIDElement->GetText();
                  if (strlen(mRID) > 0) {
                    Serial.print("mRid: ");
                    Serial.println(mRID);
                              
                        

                    XMLElement* pointElement = timeSeriesElement->FirstChildElement("Period")->FirstChildElement("Point");
                    if (pointElement) {
                      XMLElement* quantityElement = pointElement->FirstChildElement("quantity");
                      if (quantityElement) {
                        const char* quantity = quantityElement->GetText();
                        if (strlen(quantity) > 0) {
                          int quantityValue = atoi(quantity);

                          // Assign quantity value to the corresponding variable based on psrType
                          if (strcmp(psrType, "B01") == 0) {
                            quantityB01 = quantityValue;
                          } else if (strcmp(psrType, "B02") == 0) {
                            quantityB02 = quantityValue;
                          }else if (strcmp(psrType, "B03") == 0) {
                            quantityB03 = quantityValue;
                          } else if (strcmp(psrType, "B04") == 0) {
                            quantityB04 = quantityValue;
                          } else if (strcmp(psrType, "B05") == 0) {
                            quantityB05 = quantityValue;
                          } else if (strcmp(psrType, "B06") == 0) {
                            quantityB06 = quantityValue;
                          }else if (strcmp(psrType, "B07") == 0) {
                            quantityB07 = quantityValue;
                          }else if (strcmp(psrType, "B08") == 0) {
                            quantityB08 = quantityValue;
                          } else if (strcmp(psrType, "B09") == 0) {
                            quantityB09 = quantityValue;
                          } else if (strcmp(psrType, "B10") == 0 && strcmp(mRID, "7") == 0) {
                            quantityB10 = quantityValue;
                          } else if (strcmp(psrType, "B10") == 0 && strcmp(mRID, "8") == 0) {
                            quantityB10_neg = quantityValue;
                          } else if (strcmp(psrType, "B11") == 0) {
                            quantityB11 = quantityValue;
                          } else if (strcmp(psrType, "B12") == 0) {
                            quantityB12 = quantityValue;
                          } else if (strcmp(psrType, "B13") == 0) {
                            quantityB13 = quantityValue;
                          }else if (strcmp(psrType, "B14") == 0) {
                            quantityB14 = quantityValue;
                          } else if (strcmp(psrType, "B20") == 0) {
                            quantityB20 = quantityValue;
                          } else if (strcmp(psrType, "B15") == 0) {
                            quantityB15 = quantityValue;
                          } else if (strcmp(psrType, "B16") == 0 && strcmp(mRID, "14") == 0) {
                            quantityB16 = quantityValue;
                          } else if (strcmp(psrType, "B16") == 0 && strcmp(mRID, "15") == 0) {
                            quantityB16_neg = quantityValue;
                          } else if (strcmp(psrType, "B17") == 0) {
                            quantityB17 = quantityValue;
                          } else if (strcmp(psrType, "B18") == 0) {
                            quantityB18 = quantityValue;
                          } else if (strcmp(psrType, "B19") == 0) {
                            quantityB19 = quantityValue;
                          }
                      
                          
                        }
                      }
                    }
                  }
                }
              }
            }
          }

          // Move to the next TimeSeries element
          timeSeriesElement = timeSeriesElement->NextSiblingElement("TimeSeries");
        }

        //variables for the sum of renewable and conventional sources

        int renewSum = quantityB01 + quantityB09 + quantityB10 + quantityB11 + quantityB12 + quantityB13 + quantityB15 + quantityB16 + quantityB17 + quantityB18 + quantityB19 - quantityB10_neg - quantityB16_neg;
        int convSum = quantityB02 + quantityB03 + quantityB04 + quantityB05 + quantityB06 + quantityB07 + quantityB08 + quantityB14 + quantityB20;
        int allSum = renewSum + convSum;
        percRenew = static_cast<float>(renewSum) / allSum * 100;


        //Print the values
        Serial.print("quantityB01 biomass: ");
        Serial.println(quantityB01);
        Serial.print("quantityB02 fossil brown coal: ");
        Serial.println(quantityB02);
        Serial.print("quantityB04 fossil gas: ");
        Serial.println(quantityB04);
        Serial.print("quantityB05 fossil hard coal: ");
        Serial.println(quantityB05);
        Serial.print("quantityB06 fossil oil: ");
        Serial.println(quantityB06);
        Serial.print("quantityB09 fossil oil shale: ");
        Serial.println(quantityB09);
        Serial.print("quantityB10 hydro-pumped generation: ");
        Serial.println(quantityB10);
        Serial.print("quantityB10 hydro-pumped consumption: ");
        Serial.println(quantityB10_neg);
        Serial.print("quantityB11 hydro run-of-river: ");
        Serial.println(quantityB11);
        Serial.print("quantityB12 hydro water-reservoir: ");
        Serial.println(quantityB12);
        Serial.print("quantityB14 nuclear: ");
        Serial.println(quantityB14);
        Serial.print("quantityB20 other generation: ");
        Serial.println(quantityB20);
        Serial.print("quantityB15 other renewable generation: ");
        Serial.println(quantityB15);
        Serial.print("quantityB16 solar generation: ");
        Serial.println(quantityB16);
        Serial.print("quantityB16 solar consumption: ");
        Serial.println(quantityB16_neg);
        Serial.print("quantityB17 waste: ");
        Serial.println(quantityB17);
        Serial.print("quantityB18 wind offshore: ");
        Serial.println(quantityB18);
        Serial.print("quantityB19 wind onshore: ");
        Serial.println(quantityB19);
        Serial.print("Summary of renewable energy ressources: ");
        Serial.println(renewSum);
        Serial.print("Summary of conventional energy ressources: ");
        Serial.println(convSum);
        Serial.print("Summary of all energy ressources: ");
        Serial.println(allSum);
        Serial.print("Percentage of renewable ressources: ");
        Serial.println(percRenew);

        
      } else {
        Serial.println(parsingError);
        Serial.println(xmlDoc.ErrorStr());
      }
    } else {
      Serial.print("Error accessing API. Response code: ");
      Serial.println(httpResponseCode);
    }

    http.end();


  }

  float pressure = getPressure();
  Serial.print("Pressure before while loop: ");
  Serial.println(pressure);

  // Check if the body is pushed to toggle the LED light
  if (!isBodyPushed && pressure >= pressureThresholdSwitch) {
    isBodyPushed = true;
    digitalWrite(lightPin, !digitalRead(lightPin));  // Toggle the LED light state
  } else if (isBodyPushed && pressure <= pressureThresholdSwitch) {
    isBodyPushed = false;
  }

  //conditions for inflating and deflating
  while (percRenew >= 50) {
    if (pressure < 540) {
      inflate();
    } else {
      hold();
      break; // Exit the while loop
    }
    pressure = getPressure(); // Update pressure value
    Serial.println(pressure);
  }

  while (percRenew < 50) {
    if (pressure > 490.00) {
      deflate();
    } else {
      hold();
      digitalWrite(lightPin, LOW);
      break; // Exit the while loop
    }
    pressure = getPressure(); // Update pressure value
    Serial.println(pressure);
  }

  



}



//additional functions

float getPressure() {
  delay(500);  
  
  if (pressure_sensor.is_ready()) {
    return pressure_sensor.pascal();
  } else {
    Serial.println("Pressure sensor not found.");
  }

}



void inflate() {
  digitalWrite(pumpOut, LOW);
  digitalWrite(valveOut, LOW);
  digitalWrite(valveIn, HIGH);
  digitalWrite(pumpIn, HIGH);
}


void deflate() {
  digitalWrite(pumpIn, LOW);
  digitalWrite(valveIn, LOW);
  digitalWrite(valveOut, HIGH);
  digitalWrite(pumpOut, HIGH);
}

void hold() {
  digitalWrite(pumpIn, LOW);
  digitalWrite(valveIn, LOW);
  digitalWrite(valveOut, LOW);
  digitalWrite(pumpOut, LOW);
}
