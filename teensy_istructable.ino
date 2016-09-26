#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioPlaySdWav           playWav1;
// Use one of these 3 output types: Digital I2S, Digital S/PDIF, or Analog DAC
//AudioOutputI2S           audioOutput;  //outputs to the 1/8" jack
//AudioOutputSPDIF       audioOutput;
AudioOutputAnalog      audioOutput; //outputs to the prop shield
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;

// Use these with the audio adaptor board
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

//acellerometer stuff:
int testX;
int testY;
int testZ;

int newZeroX = 512; //initial zero values... these are the values read when your accelerometer is at rest (1G)
int newZeroY = 512;  //these values should be about 1024/2 or 512
int newZeroZ = 512;

int delayBeforeZero = 200;  // now waiting 1 second before zero accerlation data back to 1G

double xg, yg, zg;

long now = 0;

float totAcc = 0;

//200g variables
const int zAxis = A15;  //connect your accelerometer to analog pins available
const int yAxis = A16;
const int xAxis = A17;

long reading = 0;

int zeroAccelerometerFlag = 0;

long currentTime = 0;
long lastPrint = 0;

long captureEventTime = 0;
int captureEvent = 0;
int totAccSend = 0;
int highestTotAcc = 0;
int delayAfterHit = 0;
int playSoundFile = 0;
int fileNumber = 0;
String fileNumberString;
int playScream = 0;

void setup() {

  pinMode(8, INPUT_PULLUP);
  
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH); // turn on the amplifier
  delay(10);  
  
  Serial1.begin(9600);  //sends to esp8266 over serial
  Serial.begin(115200);

  pinMode(xAxis, INPUT);
  pinMode(yAxis, INPUT);
  pinMode(zAxis, INPUT);

  
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(8);

  audioOutput.analogReference(EXTERNAL); // much louder!
  
  // Comment these out if not using the audio adaptor board.
  // This may wait forever if the SDA & SCL pins lack
  // pullup resistors
  sgtl5000_1.enable();
  sgtl5000_1.volume(1);  //this is for the 1/8" jack output

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
}

void loop() {

  currentTime = millis();

  read200G(); //200G accelerometer
  
  if(totAcc > 10 && captureEvent == 0){  //if over 15g's a hit has occured
    captureEvent = 1;
    captureEventTime = currentTime;
    totAccSend = 0;
  }
  
  if(captureEvent == 1){  //if a hit has occured, start logging biggest impact for the next 5ms
    //Serial.println(totAcc);
    if(totAcc > totAccSend){
      totAccSend = totAcc;
    }
  }

  if(captureEvent == 1 && currentTime - captureEventTime > 5 && delayAfterHit == 0){  //after 5ms the event is over, send to ESP8266
    delayAfterHit = 1;
    Serial.println(totAccSend);
    Serial1.print("~");  //ASCII 126 is first character sent to ESP8266
    Serial1.println(totAccSend); //sned highest 'hit' value and then ASCII 13 carriage return
    zeroAccelerometerFlag = 1;  //this was only required because my accelerometer was acting a bit glitchy...
    lastPrint = currentTime;  

    playSoundFile = 1;
    fileNumber = random(1,43);  //picks a random track from 1 to 42 (track are labeled numbers)
    if(totAccSend > 120){        // if it's a big hit, play screaming track
      fileNumber = 1;
    }
    fileNumberString = String(fileNumber);
  }



  if(zeroAccelerometerFlag == 1 && currentTime - lastPrint > delayBeforeZero){  //this is a way to zero out the accelerometer ~200ms after an event (assumes a resting value of 1g has returned)
    delayAfterHit = 0; //releases flag
    captureEvent = 0;
    zeroAccelerometerFlag = 0;
    newZeroX = testX;
    newZeroY = testY;
    newZeroZ = testZ;
  }

  if (playSoundFile == 1) {
    playSoundFile = 0;
    String file = String(fileNumberString + ".wav"); 
    char filename[file.length()+1];
    file.toCharArray(filename, sizeof(filename));
    Serial.print("Start playing file: ");
    Serial.println(filename);
    playFile(filename);
  
  }

}
//******************************************************** END LOOP


void playFile(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.println(filename);
  playWav1.stop();  //if playing file already... stop it first.
  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);
  // A brief delay for the library read WAV info
  delay(5);
}


void read200G(){  //read the accelerometer and calculate the total accelertion
  double xg2, yg2, zg2;
  
  int x1 = analogRead(xAxis);
  testX = x1;
  int y1 = analogRead(yAxis);
  testY = y1;
  int z1 = analogRead(zAxis);
  testZ = z1;
  if(x1 >= newZeroX){
    x1 = x1 - newZeroX;
  } else {
    x1 = newZeroX - x1;
  }
  if(y1 >= newZeroY){
    y1 = y1 - newZeroY;
  } else {
    y1 = newZeroY - y1;
  }
  if(z1 >= newZeroZ){
    z1 = z1 - newZeroZ;
  } else {
    z1 = newZeroZ - z1;
  }  

  // using +/- 8 Gs is 16/1024 = 0.0156
  // using +/- 16 Gs is 32/1024 = 0.0312
  // using +/- 200 Gs is 200/1024 = 0.1953
  xg2 = x1 * 0.1953;
  yg2 = y1 * 0.1953;
  zg2 = z1 * 0.1953;

  totAcc = sqrt(xg2*xg2 + yg2*yg2 + zg2*zg2);
  
}


