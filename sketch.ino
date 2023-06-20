/*
  Titolo: Centralina allarme
  Ultimo aggiornamento: 2023-06-28
  Autore: Matteo Cardellini
*/

// variabili di stato dei sensori
volatile int pin5State = LOW;
volatile int pin6State = LOW;
volatile int pin7State = LOW;
volatile int pin9State = LOW;
volatile int pin10State = LOW;
volatile int pin11State = LOW;
volatile int pin12State = LOW;

void setup() {
  // definizione dei pin di output
  DDRD |= _BV(PORTD2);   // led verde         (pin 2)
  DDRD |= _BV(PORTD3);   // led blu           (pin 3)
  DDRD |= _BV(PORTD4);   // led rosso         (pin 4)
  DDRB |= _BV(PORTB5);   // buzzer            (pin 13)
  // definizione dei pin di input
  DDRB &= ~_BV(PORTB1);  // PIR stanza 2      (pin 9)
  DDRB &= ~_BV(PORTB2);  // PIR stanza 1      (pin 10)
  DDRB &= ~_BV(PORTB3);  // PIR interno porta (pin 11)
  DDRB &= ~_BV(PORTB4);  // PIR esterno porta (pin 12)
  DDRD &= ~_BV(PORTD5);  // finestra 2        (pin 5)
  DDRD &= ~_BV(PORTD6);  // finestra 1        (pin 6)
  DDRD &= ~_BV(PORTD7);  // porta             (pin 7)
  // abilito le resistenze di pull-up per i pin di input
  PORTB |= _BV(PORTB1) | _BV(PORTB2) | _BV(PORTB3) | _BV(PORTB4);
  PORTD |= _BV(PORTD5) | _BV(PORTD6) | _BV(PORTD7);

  // abilito gli interrupt nei pin dei sensori
  enabledPCIInterrupt();
  // imposto il timer
  setTimer1();
}

void loop() {
}

// Funzione che abilità gli interrupt PCI nei pin a cui sono collegati i 
// sensori
void enabledPCIInterrupt() {
  PCICR |= _BV(PCIE0) | _BV(PCIE2);
  // Abilito i pin specifici
  // pin: 9, 10, 11, 12
  PCMSK0 |= _BV(PCINT1) | _BV(PCINT2) | _BV(PCINT3) | _BV(PCINT4);   
  // pin: 5, 6, 7
  PCMSK2 |= _BV(PCINT21) | _BV(PCINT22) | _BV(PCINT23);              
}

// Funzione che inizializza il Timer1
void setTimer1() {
  TCCR1A = 0;  // imposto a 0 i registri di controllo A (default tutti 1)
  TCCR1B = 0;  // imposto a 0 i registri di controllo B (default tutti 1)
  TCCR1B |= _BV(CS12) | _BV(CS10);  // imposto un prescaler di 1024
}

// Funzione di interrupt chiamata quando si verifica un interrupt nel gruppo 
// PCIE0
ISR(PCINT0_vect) {
  violationSeverity();
}

// Funzione di interrupt chiamata quando si verifica un interrupt nel gruppo 
// PCIE2.
ISR(PCINT2_vect) {
  violationSeverity();
}

// Funzione che legge lo stato dei pin a cui sono stati collegati i sensori.
void updatePinState() {
  pin9State = PINB & _BV(PORTB1);  
  pin10State = PINB & _BV(PORTB2);  
  pin11State = PINB & _BV(PORTB3); 
  pin12State = PINB & _BV(PORTB4); 
  pin5State = PIND & _BV(PORTD5);  
  pin6State = PIND & _BV(PORTD6);
  pin7State = PIND & _BV(PORTD7);
}

// Funzione che definisce la gravità di violazione. Viene invocata al cambio 
// di stato di uno dei 7 sensori.
void violationSeverity() {
  // aggiorno i valori delle variabili di stato
  updatePinState();

  // CASO 0: stato di allarme
  if (PIND & _BV(PORTD4)) { // il led rosso è attivo
    startAlarm();
    return;
  }

  // CASO 1: nessuna violazione
  if(noViolation()) return;

  // CASO 2: intrusione / possibile movimento
  if (checkIntrusionOrPossibleViolation()) return;

  // se arrivo qui, non c'è stato nessun movimento

  // CASO 3: possibile finestra/porta lasciata aperta
  checkDoorOrWindowLeftOpen();
}

// Funzione che attiva il buzzer in caso di intrusione e disattiva gli 
// interrupt fino a quando l'allarme non viene spenta
void startAlarm() {
  // abbiamo un'intrusione, accendo il buzzer
  PORTB |= _BV(PORTB5);
  // disabilito gli interrupt PCI
  PCICR &= ~(_BV(PCIE0) | _BV(PCIE2));
}

// Funzione che controlla se è presente qualche violazione
bool noViolation() {
  if (!(pin9State || pin10State || pin11State || pin12State) &&
    !(pin5State || pin6State || pin7State)) {
    // non c'è nessun movimento e nessuna finestra/porta aperta
    PORTD &= ~_BV(PORTD2);  // spengo led verde
    PORTD &= ~_BV(PORTD3);  // spengo led blu
    PORTD &= ~_BV(PORTD4);  // spengo led rosso
    return true;
  }
  return false;
}

// Funzione che controlla il verificarsi di un intrusione o una possibile 
// violazione
bool checkIntrusionOrPossibleViolation() {
  if ((pin9State || pin10State || pin11State || pin12State)) {  // movimento
    if (pin5State || pin6State || pin7State) { 
      // movimento + porta/ finestra aperta
      PORTD &= ~_BV(PORTD2); // spengo led verde 
      PORTD &= ~_BV(PORTD3); // spengo led blu
      PORTD |= _BV(PORTD4);  // accendo led rosso
    } else { 
      // solo movimento
      PORTD |= _BV(PORTD2);  // accendo led verde
    }
    return true;
  }
  return false;
}

// Funzione che controlla se è stata lasciata una porta o una finestra
// aperta
void checkDoorOrWindowLeftOpen() {
  if (pin5State || pin6State || pin7State) { 
    // porta / finestra aperta (breve tempo)
    PORTD &= ~_BV(PORTD4);  // spengo led rosso
    PORTD |= _BV(PORTD2);  // accendo led verde

    // resetto il timer
    // imposto il timer in modo che vada in overflow dopo 1 minuto
    TCNT1 = 65536 - (F_CPU / 1024) * 60;  
    TIMSK1 |= _BV(TOIE1); // abilito l'interrupt di overflow del Timer1
  }
}

// Funzione di interrupt che viene chiamata quando si verifica un overflow
// nel Timer1
ISR(TIMER1_OVF_vect) {
  // è ancora aperta?
  if(pin5State || pin6State || pin7State) { 
    // porta / finestra lascia aperta
    PORTD &= ~_BV(PORTD2); // spengo led verde 
    PORTD |= _BV(PORTD3);  // accendo led blu
  }
  // disabilito l'interrupt di overflow del Timer1
  TIMSK1 &= ~_BV(TOIE1);
}
