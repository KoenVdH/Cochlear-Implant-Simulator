/*
  Cochlear Implant Simulator
  
  Copyright (C) 2018 Koen Van den Heuvel

  Goal of this project is to create awareness for the cochlear implant as a hearing solution for people with a moderate to profound hearing loss.
  Furthermore it is a nice exercise to learn about audio on Arduino.
  The algorithm is designed to show nice visuals under different noise conditions and microphones.
  It is based on textbook cochlear implant processing and is not based on any commercial cochlear implant product. 


  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "arduinoFFT.h"
#include <Adafruit_NeoPixel.h>
#define PIXEL_PIN    2    // Digital IO pin connected to the NeoPixels.
#define PIXEL_COUNT 20

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

arduinoFFT FFT = arduinoFFT();

// FFT variables
const uint16_t samples = 128; //This value MUST ALWAYS be a power of 2
const double samplingFrequency = 9200;
double vReal[samples];
double vImag[samples];
#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02
#define SCL_PLOT 0x03

// bark frequency bands
double bark[20];
double barkaverage;
double barkscaler[20];

boolean firsttime = 1;

// variables used to measure the speed of your uC
unsigned long microsecondsstart;
unsigned long microsecondssample;
unsigned long microsecondsloop;
double f;

void Startupanimation()
{
  for (uint16_t i = 0; i < 21; i++)
  {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
      strip.setPixelColor(i-1, strip.Color(0, 0, 0));
      strip.show();
      delay(20);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void setup()
{
  Serial.begin(115200);

  // Initialize all pixels to 'off'
  strip.begin();
  strip.show(); 

  for (uint16_t i = 0; i < 20; i++)
  {
    barkscaler[i] = 1;
  }

  Startupanimation();
}

void loop()
{
  microsecondsstart = micros();
  for (uint16_t i = 0; i < samples; i++)
  {
    vReal[i] = analogRead(A0);
    // monitor the samples using Serial Plotter tool
    //Serial.println(vReal[i],4);
  }
  microsecondssample = micros();
  f = samples / (double)(microsecondssample-microsecondsstart)*1000000.0;
  //Serial.print("Sample Frequency based on the speed of your uC : ");
  //Serial.print(f);
  //Serial.println("Hz");
  
  for (uint16_t i = 0; i < samples; i++)
  {
    vImag[i] = 0.0; //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
  }
  
  FFT.Windowing(vReal, samples, FFT_WIN_TYP_HAMMING, FFT_FORWARD);  /* Weigh data */
  FFT.Compute(vReal, vImag, samples, FFT_FORWARD); /* Compute FFT */
  FFT.ComplexToMagnitude(vReal, vImag, samples); /* Compute magnitudes into vReal */
  
  // bins into barkbands, this is not an exact matching to the bark frequencies
  bark[0] = vReal[2];
  bark[1] = vReal[3];
  bark[2] = vReal[4];
  bark[3] = vReal[5];
  bark[4] = vReal[6];
  bark[5] = vReal[7];
  bark[6] = vReal[8];
  bark[7] = (vReal[9] + vReal[10]) / 2.0;
  bark[8] = (vReal[11] + vReal[12]) / 2.0;
  bark[9] = (vReal[13] + vReal[14] + vReal[15]) / 3.0;
  bark[10] = (vReal[16] + vReal[17]) / 2.0;
  bark[11] = (vReal[18] + vReal[19] + vReal[20]) / 3.0;
  bark[12] = (vReal[21] + vReal[22] + vReal[23]) / 3.0;
  bark[13] = (vReal[24] + vReal[25] + vReal[26] + vReal[27]) / 4.0;
  bark[14] = (vReal[28] + vReal[29] + vReal[30] + vReal[31] + vReal[32]) / 5.0;
  bark[15] = (vReal[33] + vReal[34] + vReal[35] + vReal[36] + vReal[37]) / 5.0;
  bark[16] = (vReal[38] + vReal[39] + vReal[40] + vReal[41] + vReal[42] + vReal[43]) / 6.0;
  bark[17] = (vReal[44] + vReal[45] + vReal[46] + vReal[47] + vReal[48] + vReal[49] + vReal[50] + vReal[51]) / 8.0;
  bark[18] = (vReal[51] + vReal[52] + vReal[53] + vReal[54] + vReal[55] + vReal[56] + vReal[57] + vReal[58] + vReal[59] + vReal[60]) / 10.0;
  bark[19] = (vReal[61] + vReal[62] + vReal[63]) / 3.0;

  // at first run, calculate the barkscalers to start with
  if (firsttime == 1)
  {
    for (uint16_t i = 0; i < 20; i++)
    {
      barkscaler[i] = 100.0 / bark[i];
     }
     firsttime = 0;
   } 

  // slowly scale the bands to similar level, kind of reduces background noise and compensates for different types of microphone
  for (uint16_t i = 0; i < 20; i++)
  {
    bark[i] = bark[i] * barkscaler[i];
    if (bark[i] > 100)
    {
      barkscaler[i] = barkscaler[i] / 1.001;
    }
    else
    {
      barkscaler[i] = barkscaler[i] * 1.001;
    }
  }

  // Serial Plot bark bands
  for (uint16_t i = 0; i < 20; i++)
  {
    // show all barklevels each on a seperate line
    Serial.print(bark[i]+(i*500));
    Serial.print(" ");

    // show all barkscalers on a seperate line
    //Serial.print(barkscaler[i]*1000 + (i*500));
    //Serial.print(" ");
    
    // lines
    Serial.print(i*500);
    Serial.print(" ");
  }
  Serial.println(" ");

  // calculate average level in the bark bands
  barkaverage = 0.0;
  for (uint16_t i = 2; i < 20; i++)
  {
    barkaverage = barkaverage + bark[i];
  }
  barkaverage = barkaverage / 18.0;  

  // set the threshold for stimulation above average
  barkaverage = barkaverage * 1.65;

  // all off if not enough sound // what level to use?
  if (barkaverage < 150)
  {
    barkaverage = 500;
  }
  
  for (uint16_t i = 0; i < 20; i++)
  {
    if (bark[i] > barkaverage)
    {
      strip.setPixelColor(i, Wheel((i-2)*15));
    }
    else
    {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();

  // might be set to reduce the loop rate but faster looks better (now 35 Hz)
  //delay(38);
  
  microsecondsloop = micros();
  f = 1 / (double)(microsecondsloop-microsecondsstart)*1000000.0;
  //Serial.print("Loop Frequency based on the speed of your uC : ");
  //Serial.print(f);
  //Serial.println("Hz");
}
