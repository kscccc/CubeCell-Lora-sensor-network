//Power on Vbat
//60s,Vbatt+HDC,20dBm,Vext 20ms delay, Green LED,Average 113uA (sleep 10.8uA,1PLC peak 141.57mA,CRO peak 1060mA)
//Use GPIO0 to power the HDC1080
//LED on GPIO5, blink after LoraTX

/* Heltec Automation Ping Pong communication test example
 *
 * Function:
 * 1. CubeCell read battery voltage and transmit via LoRa.
 * 
 * Description:
 * 1. Only hardware layer communicate, no LoRaWAN protocol support;
 * 2. This examplese can communicate with other ESP32 LoRa device,
 *    WiFi LoRa 32, Wireless Stick, Wireless Stick Lite, etc. 
 * 3. This example is for CubeCell hardware basic test.
 *
 * HelTec AutoMation, Chengdu, China
 * 成都惠利特自动化科技有限公司
 * https://heltec.org
 *
 * this project also realess in GitHub:
 * https://github.com/HelTecAutomation/ASR650x-Arduino
 * */

#include "LoRaWan_APP.h"
#include "Arduino.h"

#include <Wire.h>
#include "HDC1080.h"
HDC1080 hdc1080;

#define time_deepsleep 60000
static TimerEvent_t wakeUp;

#ifndef LoraWan_RGB
#define LoraWan_RGB 0
#endif

#define RF_FREQUENCY                                433000000 // Hz

#define TX_OUTPUT_POWER                             20        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false


#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 30 // Define the payload size here

char txPacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );

typedef enum
{
    LOWPOWER,
    ReadData,
    TX,
    Wait_TX_done
    
}States_t;

States_t state;
bool sleepMode = false;
bool TX_done_flag = false;
int16_t rssi,rxSize;
uint16_t voltage;
float temp;
float humd;

void onWakeUp()
{
  //Serial.printf("Woke up");
  state=ReadData;
}
void Get_data()
{
      //GPIO0 on, power the HDC1080
      pinMode(GPIO0,OUTPUT);
      digitalWrite(GPIO0,HIGH);
     
      //provide more time for GPIO0 readly, get Vbatt first    
      pinMode(VBAT_ADC_CTL,OUTPUT);
      digitalWrite(VBAT_ADC_CTL,LOW);
      voltage=analogRead(ADC)*2;
      digitalWrite(VBAT_ADC_CTL,HIGH);

      //hdc1080 setup
      // Default settings: 
      //  - Heater off
      //  - 11 bit Temperature and Humidity Measurement Resolutions
      hdc1080.begin(0x40);
      //Get Temperature and Humidity
      temp = hdc1080.readTemperature();
      humd = hdc1080.readHumidity();
      //disconnect  hdc1080
      hdc1080.end();

      //Avoid loading the pullup resistors
      digitalWrite(SDA,HIGH);
      digitalWrite(SCL,HIGH);
      //GPIO0 off
      digitalWrite(GPIO0,LOW);

}
void Send_data()
{
      char *str_temp;
      String msgT = "";
      uint8_t retryCAD = 0;

      msgT = "T"+String(temp*10,0) + "H" + String(humd*10,0) + "V"+ String(voltage);
      memset(txPacket, 0, BUFFER_SIZE);
      str_temp = (char*)(msgT).c_str();
      sprintf(txPacket,"%s", str_temp);
      
      Radio.Send( (uint8_t *)txPacket, strlen(txPacket) );

      //blink LED
        digitalWrite (GPIO5,HIGH);//LED on
        delay(10);
        digitalWrite (GPIO5,LOW);//LED off
      
}
void setup() {
    boardInitMcu( );
    //GPIO0 off
    pinMode(GPIO0,OUTPUT);
    digitalWrite (GPIO0, LOW);
    
    //LED Blink
    pinMode(GPIO5,OUTPUT);
    digitalWrite (GPIO5,HIGH);//LED on
    delay(10);
    digitalWrite (GPIO5,LOW);//LED off
  
    voltage = 0;
    rssi=0;

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    state=ReadData;
    TimerInit( &wakeUp, onWakeUp );
}

void loop()
{
  switch(state)
  {
    case TX:
    {
      Send_data(); //Send data
      state=Wait_TX_done;
      break;
    }
    case Wait_TX_done:
    {
      if (TX_done_flag) //TX done
      {
          Radio.Sleep( );
          TX_done_flag= false; //reset Flag

          TimerSetValue( &wakeUp, time_deepsleep );//Set wakeup timer
          TimerStart( &wakeUp );//start timer
          state=LOWPOWER; 
      }
      else //Data is sending
      {
        delay(5);
      }
      break;
    }
    case LOWPOWER:
    {
      lowPowerHandler(); //run many times until it sleep
      //wait wake up interrupt and jump to ReadData
      break;
    }
    
    case ReadData:
    {
      Get_data();//Get data 
      state = TX;
      break;
    }
     default:
          break;
  }
  Radio.IrqProcess( );
}

void OnTxDone( void )
{
  TX_done_flag= true;
}

void OnTxTimeout( void )
{
    Radio.Sleep( );
    //Serial.print("TX Timeout......");
    state=LOWPOWER;
}
