#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Stati ciclo
enum StatoCiclo { ATTESA_START, HOMING, IDLE, VERSO_DESTRA, LIBERA_DESTRA, PAUSA, VERSO_SINISTRA, LIBERA_SINISTRA };
StatoCiclo stato = ATTESA_START;

// Motori
AccelStepper motore1(AccelStepper::FULL4WIRE, 8, 10, 9, 11);
AccelStepper motore2(AccelStepper::FULL4WIRE, 4, 6, 5, 7);

// Finecorsa
const int endLeft = 12;
const int endRight = 13;

// Potenziometri
const int pot1 = A0;
const int pot2 = A1;

// Pulsanti
const int btnFwd = 2;
const int btnBack = 3;
const int startBtn = A2;

// Tempi
unsigned long tStartCiclo = 0;
unsigned long tFineDestra = 0;
unsigned long tPausa = 0;

// Contatori
int passiLiberaDestra = 0;
int passiLiberaSinistra = 0;

// =========================
//     FUNZIONI OLED
// =========================

void drawCentered(const char* text, int y, uint8_t size) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int x = (128 - w) / 2;

  display.setCursor(x, y);
  display.print(text);   // 🔥 NON usare println()
}

void showScreen(const char* r1, const char* r2="", const char* r3="", const char* r4="", uint8_t size=1) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(r1);
  display.println(r2);
  display.println(r3);
  display.println(r4);
  display.display();
}

void showPotenziometri(int v1, int v2) {
  static unsigned long lastUpdate = 0;

  // Aggiorna solo ogni 200ms per evitare sfarfallii
  if (millis() - lastUpdate < 200) return;
  lastUpdate = millis();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // --- POTENZIOMETRI ---
  display.setTextSize(2);
  display.setCursor(4, 0);
  display.print("Pot.1: ");
  display.println(v1);

  display.setCursor(4, 20);
  display.print("Pot.2: ");
  display.println(v2);

  // --- RETTANGOLO NERO FISSO SOTTO ---
  display.fillRect(0, 44, 128, 20, SSD1306_BLACK);

  // --- SCRITTA FISSA ---
  display.setTextSize(1);
  drawCentered("Regola poi START", 50, 1);

  display.display();
}


void showHoming() {
  display.clearDisplay();
  drawCentered("HOMING", 18, 2);
  drawCentered("in corso..", 48, 1);
  display.display();
}

// =========================
//        SETUP
// =========================
void setup() {
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

// 1) Mostra il logo Adafruit
  display.display();      // forza la visualizzazione del logo
  delay(2000);            // pausa di 2 secondi

// 2) Cancella il logo
  display.clearDisplay();

// 3) Mostra la scritta AVVIO
  display.setTextColor(SSD1306_WHITE);

  drawCentered("INIZIALIZZAZIONE", 32, 1);
  drawCentered("SISTEMA", 48, 1);
  drawCentered("EV-61", 0, 2);
  display.display();
  delay(2500);   // tempo che vuoi tu

// 4) Poi il programma prosegue normalmente
  pinMode(endLeft, INPUT_PULLUP);
  pinMode(endRight, INPUT_PULLUP);
  pinMode(btnFwd, INPUT_PULLUP);
  pinMode(btnBack, INPUT_PULLUP);
  pinMode(startBtn, INPUT_PULLUP);

  pinMode(A3, OUTPUT);
  digitalWrite(A3, LOW);

  motore1.setMaxSpeed(520);
  motore1.setAcceleration(1200);

  motore2.setMaxSpeed(100);
  motore2.setAcceleration(600);
}

// =========================
//          LOOP
// =========================
void loop() {

  int speed1 = map(analogRead(pot1), 0, 1023, 200, 550);
  int speed2 = map(analogRead(pot2), 0, 1023, 2, 100);

  switch (stato) {

    // ============================
    //   ATTESA START
    // ============================
    case ATTESA_START: {
      static unsigned long lastSwitch = 0;
      static bool toggle = false;

      // Gestione display senza delay
      if (millis() - lastSwitch > 1500) {
        lastSwitch = millis();
        toggle = !toggle;

        display.clearDisplay();

      if (toggle) {
        drawCentered("SISTEMA", 16, 2);
        drawCentered("PRONTO", 36, 2);
        } 
      else {
        drawCentered("START", 0, 2);
        drawCentered("Per", 22, 2);
        drawCentered("HOMING", 50, 2);
          
        }
        display.display();
      }

      // Lettura pulsante immediata
      if (digitalRead(startBtn) == LOW) {
        // Un piccolo debounce di 50ms va bene, ma non 1000!
        delay(50); 
      if (digitalRead(startBtn) == LOW) {
        showHoming();
        while (digitalRead(startBtn) == LOW); // Aspetta rilascio
        stato = HOMING;
        }
      }
    }
    break;
    // ============================
    //          HOMING
    // ============================
    case HOMING: {
    // 1. Avvicinamento veloce verso il finecorsa (sinistra)
        motore1.setSpeed(-350); 
      
      if (digitalRead(endLeft) == HIGH) {
        motore1.runSpeed();
      } else {
        // 2. Il finecorsa è stato toccato!
        
        // Movimento di rincorsa/stacco lento (verso destra)
        // Usiamo un ciclo for per muoverlo di N passi precisi
        int passiDiStacco = 400; // Regola questo numero per la distanza desiderata
        
        motore1.setSpeed(150); // Velocità lenta per lo stacco
        for(int i = 0; i < passiDiStacco; i++) {
            motore1.runSpeed();
            delay(2); // Piccolo delay per dare il tempo fisico ai passi
        }

        motore1.setCurrentPosition(0); // Imposta lo ZERO in questa nuova posizione
        
        display.clearDisplay();

// Riga 1 centrata (titolo grande)
  drawCentered("HOMING", 18, 2);

// Riga 2 centrata (sottotitolo)
  drawCentered("Completato", 48, 1);

  display.display();
  delay(1600);
  stato = IDLE;
      }
    }
  break;

    // ============================
    //     CONTROLLO MANUALE
    // ============================
    
  case IDLE: {
      // --- MOSTRA POTENZIOMETRI (ma NON bloccare il loop) ---
      static unsigned long lastPotUpdate = 0;
      if (millis() - lastPotUpdate > 200) {
        lastPotUpdate = millis();
        showPotenziometri(speed1, speed2);
      }

      // --- CONTROLLO MANUALE MOTORE 2 ---
      if (digitalRead(btnFwd) == LOW) {
        motore2.setSpeed(speed2);
      } 
      else 
      if (digitalRead(btnBack) == LOW) {
        motore2.setSpeed(-speed2);
      } 
      else {
        motore2.setSpeed(0);
      }

      // ESEGUE SEMPRE IL MOTORE
        motore2.runSpeed();

      // --- AVVIO CICLO ---
      if (digitalRead(startBtn) == LOW) {
        display.clearDisplay();
        drawCentered("IN CICLO", 20, 2);
        display.display(); 
        
        tStartCiclo = millis();
        digitalWrite(A3, HIGH);

        motore1.setSpeed(speed1);
        motore2.setSpeed(speed2);

        stato = VERSO_DESTRA;

        while(digitalRead(startBtn) == LOW);
      }
    }
    break;

    // ============================
    //       CICLO AUTOMATICO
    // ============================
    case VERSO_DESTRA:
      if (digitalRead(endRight) == HIGH) {
        motore1.setSpeed(speed1);
        motore2.setSpeed(speed2);
        motore1.runSpeed();
        motore2.runSpeed();
      } else {
        tFineDestra = millis() - tStartCiclo;
        passiLiberaDestra = 80;
        stato = LIBERA_DESTRA;
      }
      break;

    case LIBERA_DESTRA:
      if (passiLiberaDestra-- > 0) {
        motore1.setSpeed(250);
        motore1.runSpeed();
      } else {
        tPausa = millis();
        stato = PAUSA;
      }
      break;

    case PAUSA:
      if (millis() - tPausa > 800) stato = VERSO_SINISTRA;
      break;

    case VERSO_SINISTRA:
      if (digitalRead(endLeft) == HIGH) {
        motore1.setSpeed(-speed1);
        motore2.setSpeed(-speed2);
        motore1.runSpeed();
        motore2.runSpeed();
      } else {
        passiLiberaSinistra = 80;
        stato = LIBERA_SINISTRA;
      }
      break;

    case LIBERA_SINISTRA:
      if (passiLiberaSinistra-- > 0) {
        motore1.setSpeed(250);
        motore1.runSpeed();
      } else {
        // 2. Il finecorsa è stato toccato!
        
        // Movimento di rincorsa/stacco lento (verso destra)
        // Usiamo un ciclo for per muoverlo di N passi precisi
        int passiDiStacco = 400; // Regola questo numero per la distanza desiderata
        
        motore1.setSpeed(150); // Velocità lenta per lo stacco
        for(int i = 0; i < passiDiStacco; i++) {
        motore1.runSpeed();
        delay(2); // Piccolo delay per dare il tempo fisico ai passi
        }

  motore2.stop();
  digitalWrite(A3, LOW);
  display.clearDisplay();

// Riga 1 centrata (titolo grande)
  drawCentered("CICLO", 16, 2);

// Riga 2 centrata (sottotitolo)
  drawCentered("Completato", 40, 1);

  display.display();
  delay(1200);

// Tempo DX → SX
  unsigned int tempoDx = tFineDestra / 1000;

// Tempo totale ciclo (A3 HIGH)
  unsigned int tempoTot = (millis() - tStartCiclo) / 1000;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Riga 1
  display.setCursor(8, 9);
  display.print("Da DX a SX: ");
  display.print(tempoDx);
  display.println(" Sec.");

// Riga 2
  display.setCursor(34, 42);
  display.print("Totali ");
  display.print(tempoTot);   // solo numero
  display.display();
  delay(5000);
  stato = IDLE;
    
  }
  break;
   }
  }