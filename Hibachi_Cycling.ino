/*
  Hibachi Cycling program
  
  This program runs a single Hibachi through 20 minute on cycles,
  allows it to cool to a user-defined temperature, and turns it back
  on. The number of cycles and temperature are recorded

 The circuit:
 1k RTD sensor on A0
 Control Pin for Powerswitch Tail II pin 4
 
 Prepare your SD card creating an empty folder in the SD root
 named "arduino". This will ensure that the Yún will create a link
 to the SD to the "/mnt/sd" path.

 You can remove the SD card while the Linux and the
 sketch are running but be careful not to remove it while
 the system is writing to it.
*/
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <Temboo.h>
#include <Console.h>
#include "TembooAccount.h" // contains Temboo account information
//#include "passwords.h"
String database = "/mnt/sda1/arduino/www/test2.db ";

unsigned long time = 0;
int i = 0;

//Pin assignments
int fan = 9;

unsigned long cycleStartTime = 0;

int maxTemp[5] = {0,0,0,0,0};

int cycles[5] = {0,0,0,0,0};

int temperature[5] = {0,0,0,0,0};

int enable[5] = {1,1,1,1,0};

int cycleComplete[5] = {0,0,0,0,0};

YunServer server;

void setup() {
  // Give 60s for the linux processor to get going
  delay(60000);
  
  Bridge.begin();
  
  
  analogReference(INTERNAL); //sets the analog reference to 2.56V
  analogRead(A0);
  analogRead(A1);
  analogRead(A2);
  analogRead(A3);
  analogRead(A4);
  analogRead(A0);
  analogRead(A1);
  analogRead(A2);
  analogRead(A3);
  analogRead(A4);
  
  for (i = 0; i < 4; i = i+1)  {
    pinMode(i+4,OUTPUT);
    cycles[i] = getCycle(i);
  }
  pinMode(fan,OUTPUT);
  cycleStartTime = millis();
  
  server.listenOnLocalhost();
  server.begin();
}

void loop() {
  
  // First check for any http requests
  YunClient client = server.accept();
  
  if (client) {
    process(client);
    client.stop();
  }
  
  if (millis()-time>15000)  {
    time = millis();
    
//    dateTime = getTimeStamp();
    
    for (i = 0; i < 5; i = i+1)  {
      if(enable[i])  {
        temperature[i] = readRTD(i);
        if (temperature[i] > maxTemp[i])  {maxTemp[i] = temperature[i];}
//        sendData(temperature[i]);
        delay(1000);
      }
    }
    
    if(millis()-cycleStartTime < 1500000)  { // we require 25 minutes on
      for (i = 0; i < 5; i = i+1)  {
        if(enable[i])  {
          digitalWrite(i+4,HIGH);
          cycleComplete[i] = 0; //the bbqs are in cycle, so there cyclecomplete flag should be down
        }
        else  {
          digitalWrite(i+4,LOW);
          cycleComplete[i] = 1;
        }
      }
          
    }
    else  {       
      
      // turn off the bbqs and turn on the fan
      
      for (i = 0; i < 5; i = i+1)  {
        digitalWrite(i+4,LOW);
      }
      digitalWrite(fan,HIGH); //turn on the fan
      
      // check if the cycle complete flag is been raised. if not, raise it
      for (i = 0; i < 5; i = i+1)  {
        if(cycleComplete[i] != 1 && enable[i] == 1)  {
          cycleComplete[i] = 1;
          int oldcycle = cycles[i];
          cycles[i] = getCycle(i);
          if (cycles[i] < oldcycle) {cycles[i] = oldcycle;}
          cycles[i] = cycles[i] + 1;
          setCycle(i,cycles[i]);
          // check the temperature was reached
          if (maxTemp[i] < 100)  {
            enable[i] = 0; //disable the Hibachi
            sendEmail(); //send the email alert
          }
        }
        maxTemp[i] = 0;  //reset the max_temperature
      }
    }
      
    if (millis()-cycleStartTime > 2100000)  {
      digitalWrite(fan,LOW); // turn off the fan
      cycleStartTime = millis();
    }
  }
    

  delay(50); //poll every 50ms
}

void process(YunClient client)  {
  // this is where we interpret commands
  String command = client.readStringUntil('/');
  command.trim();
  // Check what the URL says
  if (command == "status")  {
    returnStatus(client);
  }
  if (command == "enable")  {
    enableCommand(client);
  }
  if (command == "temp")  {
    tempCheck(client);
  }
}

void returnStatus(YunClient client)  {
  // this will return the curretn bbq status
  // it might make more sense to keep this information on the bridge,
  // which nicely formats it as JSON
  // set JSON header
  client.println(F("Status: 200"));
  client.println(F("Content-type: application/json"));
  client.println();
  client.print(F("{\"Dataen\" : ["));
  for (i=0;i<5;i++)
  {
    client.print("{\"dataen\" : "+String(enable[i])+"}");
    if (i<4) client.print(",");
    else client.print("]}");
  }
}

void enableCommand(YunClient client)  {
  String command = client.readStringUntil('/');
  command.trim();
  int oldval;
  for (i=0;i<5;i++)  {
    oldval = enable[i];
    enable[i] = byte(command.charAt(i)-48);
    //update the serial numbers and cycles
    cycles[i] = getCycle(i);
  }
    // set JSON header
  client.println(F("Status: 200"));
  client.println(F("Content-type: application/json"));
  client.println();
  // return ok status
  client.print(F("{\"ret\":\"ok\"}"));
}

void tempCheck(YunClient client)  {
  // this will return the curretn bbq status
  // it might make more sense to keep this information on the bridge,
  // which nicely formats it as JSON
  // set JSON header
  client.println(F("Status: 200"));
  client.println(F("Content-type: application/json"));
  client.println();
  client.print(F("{\"Temps\" : ["));
  for (i=0;i<5;i++)
  {
    client.print("{\"temp\" : "+String(temperature[i])+"}");
    if (i<4) client.print(",");
    else client.print("]}");
  }
}

//// This function return a string with the time stamp
//String getTimeStamp() {
//  String result;
//  Process time;
//  // date is a command line utility to get the date and the time
//  // in different formats depending on the additional parameter
//  time.begin("date");
//  time.addParameter("+%Y-%m-%d %H:%M:%S");  // parameters: D for the complete date mm-dd-yy
//  //             T for the time hh:mm:ss
//  time.run();  // run the command
//
//  // read the output of the command
//  while (time.available() > 0) {
//    char c = time.read();
//    if (c != '\n')
//      result += c;
//  }
//
//  return result;
//}

int readRTD(int pin)  {
  // Read an RTD sensor and translate to degress celcius
  double sensor = analogRead(pin);
  double rtdVoltage = sensor*2.56/1024.0;
  double rtdOhms = 4658*rtdVoltage/(5-rtdVoltage);
  double celcius = 3383.66 - (2.17965E-9)*sqrt(2.77436E24-(3.64461E20)*rtdOhms);
  return (int) celcius;
}

void sendEmail()  {
  // This function has been having some trouble, probably memory related. Right now its a place holder
  // only try to send the email if we haven't already sent it successfully
  
    TembooChoreo SendEmailChoreo;

    // invoke the Temboo client
    // NOTE that the client must be reinvoked, and repopulated with
    // appropriate arguments, each time its run() method is called.
    SendEmailChoreo.begin();
    
    // set Temboo account credentials
    SendEmailChoreo.setAccountName(TEMBOO_ACCOUNT);
    SendEmailChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
    SendEmailChoreo.setAppKey(TEMBOO_APP_KEY);

    // identify the Temboo Library choreo to run (Google > Gmail > SendEmail)
    SendEmailChoreo.setChoreo("/Library/Google/Gmail/SendEmail");
 

    // set the required choreo inputs
    // see https://www.temboo.com/library/Library/Google/Gmail/SendEmail/ 
    // for complete details about the inputs for this Choreo

    // the first input is your Gmail email address
    SendEmailChoreo.addInput(F("Username"), F("bram.evert@gmail.com"));
    // next is your Gmail password.
    SendEmailChoreo.addInput(F("Password"), F("spxokizuozqpmuez"));
    // who to send the email to
    SendEmailChoreo.addInput(F("ToAddress"), F("bevert@thermoceramix.com"));
    // then a subject line
    SendEmailChoreo.addInput("Subject", "FAILURE");
    
    String msgBody = String("Pos: " + String(i) + ", Cycles: " + String(cycles[i]) + ", Max Temp: " + String(maxTemp[i]));
     // next comes the message body, the main content of the email
    SendEmailChoreo.addInput("MessageBody", msgBody);

    // tell the Choreo to run and wait for the results. The 
    // return code (returnCode) will tell us whether the Temboo client 
    // was able to send our request to the Temboo servers
    unsigned int returnCode = SendEmailChoreo.run();
    // a return code of zero (0) means everything worked
    if (returnCode == 0) {
    } 
    else {
      // a non-zero return code means there was an error
      // read and print the error message
      while (SendEmailChoreo.available()) {
        char c = SendEmailChoreo.read();
      }
    } 
    SendEmailChoreo.close(); 
}

int getCycle(int location)  {
  //get the previous cycle number from the database
  Process p;
  String cmd = "sqlite3 ";
  String sql = "'SELECT cycles FROM hibachis WHERE position = " + String(location) + ";'";
  p.runShellCommand(cmd + database + sql);
  
  String response;
  while (p.available()>0) {
    response = p.readStringUntil('\n');
  }
  return response.toInt();
  //CREATE TABLE hibachis (serial_number TEXT,cycles INT, position INT);
  //CREATE TABLE temp_data (time DATETIME, temperature INT, cycle INT, serial_number TEXT);

}

void setCycle(int location,int cycle)  {
  //get the previous cycle number from the database
  Process p;
  String cmd = "sqlite3 ";
  String sql = "'UPDATE hibachis SET cycles = " + String(cycle) + " WHERE position = " + String(location) + ";'";
  p.runShellCommand(cmd + database + sql);
  //CREATE TABLE hibachis (serial_number TEXT,cycles INT, position INT);
  //CREATE TABLE temp_data (time DATETIME, temperature INT, cycle INT, serial_number TEXT);
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
