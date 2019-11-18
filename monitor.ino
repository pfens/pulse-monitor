#include <SoftwareSerial.h> 
#include <OneWire.h> //pomocna biblioteka pri spajanju tri pina od temp. senzora u jedan izlaz (zicu)
#include <DallasTemperature.h> //biblioteka koja cini rad sa temp. senzorom laksim
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//incijalizacija heartBeatSensor pinova
int pulsePin = 0; 
int blinkPin = 13;

//signal koji ocitavamo je promena otpornosti fotootpornika na senzoru 
volatile int fotoOtpornost;
volatile int maxOtpornost = 512;        
volatile int minOtpornost = 512;          
volatile int threshold = 512;    //odrednica kada je ocitan 1 oscilacija tj otkucaj

//intervali izmedju otkucaja
volatile int IBI = 600;                        //InterBeatInterval seedovan na 600 jer je normaln IBI izmedju 600ms i 900ms
volatile unsigned long msBrojac = 0;           //brojac milisekundi
volatile unsigned long poslednjiPeriod = 0;    //izmereno vreme za poslednji otkucaj
volatile int rate[10];                         //niz u koji upisujemo 10 prvih ocitavanja otkucaja srca

//ima ili nema
volatile boolean Otkucaj = false;
volatile boolean prviOtkucaj = true;
volatile boolean drugiOtkucaj = false;
volatile boolean Puls = false;

//izlazna vrednost koju pushujemo na app
volatile int BPM;

void interruptSetup(){  
//Inicijalizuje Timer2 koji nabacuje interrupt svake 2ms
//Timer2 postara se da se podaci ocitavaju svake dve milisekunde

  TCCR2A = 0x02;     // DISABLE PWM ON DIGITAL PINS 3 AND 11, AND GO INTO CTC MODE
  TCCR2B = 0x06;     // DON'T FORCE COMPARE, 256 PRESCALER
  OCR2A = 0X7C;      // SET THE TOP OF THE COUNT TO 124 FOR 500Hz SAMPLE RATE (500Hz -> 2ms)
  TIMSK2 = 0x02;     // ENABLE INTERRUPT ON MATCH BETWEEN TIMER2 AND OCR2A
  sei();             // ukljucuje sve  interrupte globalno
}

ISR(TIMER2_COMPA_vect) {                   //InterruptServiceRoutine
  cli();                                   //gasi globalni interrupt flag u status registru
  fotoOtpornost = analogRead(PulsePin);           //signal koji ocitavamo sa senzorskog pina
  msBrojac += 2;                           //inkrementiramo brojac za vreme od 2ms
  int n = msBrojac - poslednjiPeriod;      //posmatramo vreme mereno od poslednjeg otkucaja

  //odredjujemo najmanju i najvecu vrednost signala koji primamo
  //izbegavamo zapise preskakanja srca ili ekstrasistole (dupli otkucaj tokom jednog perioda) pomocu formule (IBI / 5) * 3
  if (fotoOtpornost < threshold && n > (IBI / 5) * 3) {    
    if (fotoOtpornost < minOtpornost) {
      minOtpornost = fotoOtpornost;                                 //lowest point of the pulse
    }
  }
  if (fotoOtpornost > threshold && fotoOtpornost > maxOtpornost) {         //highest point of the pulse
    maxOtpornost = fotoOtpornost;
  }


  if (n > 250) {
    if ( (fotoOtpornost > threshold) && (Otkucaj == false) && (n > (IBI / 5) * 3) ) {
      Otkucaj = true;
      digitalWrite(blinkPin, HIGH);        
      IBI = msBrojac - poslednjiPeriod;    //prvo izmereno vreme (interval) izmedju 2 otkucaja
      poslednjiPeriod = msBrojac;

      if (prviOtkucaj == true) {
        prviOtkucaj = false;
        drugiOtkucaj = true;
        sei();   //ukljucuje interrupt
      }

      if (drugiOtkucaj) {
        drugiOtkucaj = false;
        for (int i = 0; i <= 9; i++) {     //ovako inicijalizujemo pocetnu vrednost IBIa da bi dobili realan BPM pri startapu
          rate[i] = IBI;
        }
      }

      unsigned int tempTotal = 0;       //pomocna promenjiva u kojoj cuvamo sve izmerene intervale medju otkucajima
      for (int i = 0; i <= 8; i++) {
        rate[i] = rate[i + 1];
        tempTotal += rate[i];
      }
      rate[9] = IBI;
      tempTotal += rate[9];
      tempTotal /= 10;                 //uzimamo srednju vrednost intervala radi sto vece preciznosti
      BPM = 60000 / tempTotal;         //formula: koliko ima jednakih intervala u minutu
      Puls = true;                     //nasli smo puls
    }
  }

  if (fotoOtpornost < threshold && Otkucaj == true) {
    digitalWrite(blinkPin, LOW);       //ne pali lampicu ako je preslab signal
    Otkucaj = false;
  }

  //ako 2 i po sekunde nema ni jednog otkucaja restartuj brojace
  if (n > 2500) {         
    threshold = 512;      //vrati threshold na default stanje
    maxOtpornost = 512;
    minOtpornost = 512;
    poslednjiPeriod = msBrojac;   //nema poslednjeg, idemo jono nanovo
    prviOtkucaj = true;
    drugiOtkucaj = false;
  }
  sei();
}  //kraj rutine

//setup za bluetooth modul
SoftwareSerial Bluetooth(10, 9);  
void setup() {
  Bluetooth.begin(9600);
  Serial.begin(9600);
  sensors.begin();
  interruptSetup();
}

void loop() {
  sensors.requestTemperatures();
  //azurno salje izmerene vrednosti temperature i pulsa preko bluetootha, povezano sa android aplikacijom
  if (Puls) {
    Serial.print(sensors.getTempCByIndex(0)); Serial.print(" "); Serial.println(BPM);
    Bluetooth.print(sensors.getTempCByIndex(0)); Bluetooth.print(" ") ; Bluetooth.println(BPM);
    Puls = false;
  }
  delay(1500);
}
