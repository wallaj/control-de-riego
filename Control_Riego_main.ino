



/*-----------------------------------|
SISTEMA DE CONTROL DE CULTIVO 

AUTOR: MARCOS URZÚA

SEPTIEMBRE 2021. BASADO EN "SISTEMA DE CONTROL
DE CULTIVO" V1.0 MARZO 2018.

MCU: ATMEGA 328P (16Mhz)
------------------------------------|*/
//LIBBRERIAS

#include <Wire.h> //Comunicacion I2C
#include <SPI.h>
#include <SoftwareSerial.h>//Configura pines Rx y Tx de comunicación Serial
#include <DHT.h>    //Sensor T°/H 
#include <EEPROM.h> //EEPROM R-W
/*
 * EEPROM[0}--->seteo del ciclo de luz. Auto/Manual. Ocupa una celda, graba un boolean
 * EEPROM[1}--->horas del ciclo de luz. Ocupa dos celdas, graba un entera
 */
#include <RTClib.h> //RELOJ RTC //Gestión de fecha y hora
#include <LiquidCrystal_I2C.h> //Display LCD
#define TIPO DHT11 //Tipo de sensor T/H

//PINES ANALÓGICOS
#define sensorH1 A1
#define sensorH2 A2
//PINES DIGITALES
#define sensorDHT 2
#define valvula 3
#define luz 4
//#define bomba 5
//#define aire 6

//INSTANCIAS DE OBJETOS
LiquidCrystal_I2C lcd(0x3F, 16, 2); //DISPLAY
RTC_DS1307 reloj;//Reloj de tiempo real
DateTime now;//fecha y hora
DHT dht(sensorDHT, TIPO);
SoftwareSerial comBT(10, 11); // pin 10 ->RX, pin 11 ->TX de la comunicación mediante Bluetooth

/////VARIABLES
int lecturaH1;
int lecturaH2;
int humedad;
int temperatura;
int traerCicloLuz;
byte eRiego;
byte eLuz;
byte cLuz;
byte horaInicio;
byte getCicloLuz;
bool controlLuces = false; //Activa y desactiva control de luces automático
bool regando = false;//Indica si se está regando
bool luces = false;//Indica si las luces están encendidas
bool ventilacion = false;
char daysOfTheWeek[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};//Días
char buffer[50];//Buffer de datos para enviar mediante serial del comBT
int hora;
int minut;
int dia;
int mes;  
String dato;

////////////caracteres especiales
  byte sol[8] = {
    B00100,
    B10101,
    B01110,
    B11011,
    B11011,
    B01110,
    B10101,
    B00100
    
  };
  byte puntero[8] = {
    B11000,
    B11100,
    B11110,
    B11111,
    B11110,
    B11100,
    B11000,
    B00000
    
  };
  byte luna[8] = {
    B00111,
    B01100,
    B11000,
    B11000,
    B11000,
    B11000,
    B01100,
    B00111
    
  };
  byte grado[8] = {
    B00111,
    B00101,
    B00111,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000
  };
  byte flechas[8] = {
    B00100,
    B01110,
    B11111,
    B00000,
    B00000,
    B11111,
    B01110,
    B00100
  };  
  byte foco[8] = {
    B01110,
    B10001,
    B10101,
    B10001,
    B11111,
    B01110,
    B01110,
    B00100
  };
  byte fan[8] = {
    B10011,
    B01100,
    B00000,
    B10011,
    B01100,
    B00000,
    B10011,
    B01100
  };
  byte gota[8] = {
    B00100,
    B01100,
    B01110,
    B11110,
    B11101,
    B11001,
    B01111,
    B01110
  };
/////MÉTODOS
void setupText(int, int, int);
void estadoSensor1();
void estadoSensor2();
void determinarRiego();
void regar();
void cortarRiego();
int sensarHumedad();
int sensarHumedad2();
void mostrarHora(DateTime);
void printHORA(int);
void printMIN(int);
void printSEC(int);
void datosAlBuffer();
void lecturaBuffer();
void ventanaRiego();
void cicloLuz();
void encenderLuces();
void apagarLuces();
void lecturasDHT();
///////////SETUP
void setup() {
  
  
  comBT.begin(9600); //baudios por defecto en modulo
  Serial.begin(9600);
  lcd.begin();//inicio pantalla
  lcd.backlight();
  lcd.clear();
  //reloj.adjust(DateTime(F(__DATE__), F(__TIME__)));//Ajuste del Reloj con hora de compilación  
 
  //iniciar sensor
  dht.begin();
  //COMPORTAMIENTO DE PINES
  pinMode(valvula,OUTPUT);
  pinMode(luz,OUTPUT);
  //pinMode(bomba,OUTPUT);
  //pinMode(aire,OUTPUT);
  digitalWrite(valvula,HIGH);//Lógica de los modulos relay invertida
  digitalWrite(luz,HIGH);//Lógica de los modulos relay invertida

  ///Seteo inicial de flags de control en FALSO. Luego cambiaran con el recupero de datos, en el caso de 'controlLuces'
  controlLuces = false; //Activa y desactiva control de luces automático
  regando = false;//Indica si se está regando
  luces = false;//Indica si las luces están encendidas
  
  lcd.println("AUTO JARDIN");
  delay(350);
  lcd.createChar(1,sol);
  lcd.createChar(2,luna);
  lcd.createChar(3,puntero);
  lcd.createChar(4,grado);
  lcd.createChar(5,flechas);
  lcd.createChar(6,foco);
  lcd.createChar(7,fan);
  lcd.createChar(8,gota);
  lcd.clear();
  /////////////
  
  ///Recupero de datos guardados en la EEPROM
  ///EEPROM[0]--->control de luces
  ///EEPROM[1]--->duración del ciclo de luz
  ///EEPROM[2]--->hora de inicio del ciclo de luz
  controlLuces = EEPROM.get(0,controlLuces);
  cLuz = EEPROM.get(1,getCicloLuz);
  horaInicio = EEPROM.get(2,horaInicio);
  ///
  ///Impresiones de datos en Serial para fines de mantenimiento
  Serial.print("Control luces: ");
  if(controlLuces){
    Serial.println("Automatico");  
  }else{
    Serial.println("Manual");
  }
  Serial.print("Ciclo de luz: ");
  Serial.println(cLuz);
  //////////////
}

void loop() {
  /*UTILIZADO para comunicación en configuración
   * if(comBT.available()){
    Serial.write(comBT.read());
  }
  if(Serial.available()){
    comBT.write(Serial.read());
  }*///////////////////////////////////////
  
  
  //MUESTRAS DE ESTADO EN SERIAL--->PARA FINES DE ANÁLISIS/COMPROBACIÓN Y MANTENIMIENTO
  Serial.print("Control luces: ");
  Serial.println(EEPROM.get(0,controlLuces));
  Serial.print("Ciclo de luz: ");
  Serial.println(EEPROM.get(1,getCicloLuz));
  Serial.print("Inicio del Ciclo: ");
  Serial.println(EEPROM.get(2,getCicloLuz));
  
  now = reloj.now(); //Obtener hora del reloj
  DateTime tiempoRiego(now+TimeSpan(0,0,1,0));//Duración del riego(d,h,m,s). En este caso 10 minutos
  mostrarHora(now);//Muestra hora en display
  
  ///RECEPCIÓN DE DATOS DEL BLUETOOTH
  lecturaBuffer();
  
  ///LECTURAS DHT
  lecturasDHT();
  
    //////CONTROL HUMEDAD SUELO. Mostrados en serial con fines de mantenimiento/////// 
  Serial.print("Sensor 1: ");
  Serial.println(sensarHumedad());
  Serial.print("Sensor 2: ");
  Serial.println(sensarHumedad2());
 
  ///DETERMINAR RIEGO
  determinarRiego();
  
  ///CONTROL CICLO DE LUZ
  controlCicloLuz();
  
  ///ENVÍO DE DATOS DEL BLUETOOTH 
  datosAlBuffer();
 
  delay(600);
  
  
  ////////////////////
 
}
void lecturaBuffer(){
  byte datoRecibido;
  
  if(comBT.available()>0){
    datoRecibido = comBT.read();
    Serial.println("Datos disponibles");

    //Condiciones de funcionamiento según dato recibido
    if(datoRecibido>=0 && datoRecibido<=24){
      cLuz = datoRecibido;
      horaInicio = now.hour();//Guarda la hora del seteo del ciclo. Desde aquí iniciara
      EEPROM.update(1,cLuz);///Grabado en EEPROM.
      EEPROM.update(2,horaInicio);
      Serial.print("Ciclo de luz seteado en: ");///Impresiones en el serial para fines mantenimiento 
      Serial.print(cLuz);
      Serial.print(" comenzando a las: ");
      Serial.print(horaInicio);
      Serial.println("hs");
    }else if(datoRecibido>24){
      switch(datoRecibido){
        case 65://'A'--->ASCII 65. Activa control automático de luces
          controlLuces = true;  
          Serial.println("Control de luces automatico: ON");///Impresiones en el serial para fines mantenimiento 
          EEPROM.update(0,controlLuces);///Grabado en EEPROM. Ocupa un byte.
          break;
        case 66://'B'--->ASCII 66. Desactiva control automático de luces 
          controlLuces = false;
          Serial.println("Control de luces automatico: OFF");///Impresiones en el serial para fines mantenimiento
          EEPROM.update(0,controlLuces);///Grabado en EEPROM. Ocupa un byte.
          break;
        case 67://'C'--->ASCII 67. Enciede luces manualmente
          encenderLuces();
          break;
        case 68://'D'--->ASCII 68. Apaga luces manualmente
          digitalWrite(luz,HIGH);//lógica invertida
          luces=false;
          Serial.println("Luces apagadas");///Impresiones en el serial para fines mantenimiento
          break;
        case 69://'E'--->ASCII 69. Enciede riego manualmente
          regar();
          Serial.println("Riego encendido");///Impresiones en el serial para fines mantenimiento
          break;
        case 70://'F'--->ASCII 70. Apaga riego manualmente
          cortarRiego();
          Serial.println("Riego apagado");///Impresiones en el serial para fines mantenimiento
          break;                                 
      }
    }
  }
  
}
void datosAlBuffer(){
  //////////////////////////////////////////////////////////////////////////////////////////////////
  ///ORDEN DE ALMACENAMIENTO DE DATOS EN BUFFER PARA TRANSMISIÓN
  ///t,hu,h,m,d,ms,cLuz,eLuz,eRiego,controlLuces
  //////////////////////////////////////////////////////////////////////////////////////////////////
  
  ///definimos estado del dato riego según estado de regando true=1, false=0
  if(regando){
    eRiego = 1;
  }else{
    eRiego = 0;
  }
  ///definimos estado del dato eLuz según estado de luces true=1, false=0
  if(luces){
    eLuz = 1;
  }else{
    eLuz = 0;
  }  
  
  //armado de cadena de datos en el buffer
  sprintf(buffer, "%d,%d,%d,%d,%d,%d,%d,%d,%d",temperatura,humedad,hora,minut,dia,mes,cLuz,eLuz,eRiego);
  comBT.println(buffer);
  Serial.println(buffer);
  
}
void determinarRiego(){
  if(regando){
      ventanaRiego();
      if((sensarHumedad()<370)&&(sensarHumedad2()<370)){///TOMA HUMEDAD CON SENSOR 2(BAJO)
        cortarRiego();  
      }
  }else if((!regando)&&(sensarHumedad()>970)&&(sensarHumedad2()>370)){///REGAR SI NO ESTÁ REGANDO Y ESTÁ SENSANDO POR ENCIMA DE 970 EL SENSOR H1
          
      regar();
      Serial.print(hora);
      Serial.print(":");
      Serial.print(minut);
      Serial.print(" ");
      Serial.print(dia);
      Serial.print("/");
      Serial.println(mes);
      Serial.println("ARRIBA HORA Y FECHA DE RIEGO");  
  }  
}
int sensarHumedad(){
  return analogRead(sensorH1);
}
int sensarHumedad2(){
  return analogRead(sensorH2);
}
void regar(){///ABRE ELECTROVÁLVULA Y GUARDA HORA DE APERTURA
    digitalWrite(valvula,LOW);//Lógica invertida
    regando = true;    //Flag en verdadero
    hora = now.hour();
    minut = now.minute();
    dia = now.day();
    mes = now.month();
}
void cortarRiego(){
    digitalWrite(valvula,HIGH);///CIERRA ELECTROVÁLVULA
    regando = false;///Flag en falso
}
void estadoSensor1(){
  if(sensarHumedad()>1022){
    lcd.setCursor(0,0);
    lcd.print("               ");  
    lcd.setCursor(5,0);
    lcd.print("SENSOR 1!!!");  
  }
}
void estadoSensor2(){
  if(sensarHumedad2()>1022){
    lcd.setCursor(0,1);
    lcd.print("               ");  
    lcd.setCursor(5,1);
    lcd.print("SENSOR 2!!!");  
  }
}
void controlCicloLuz(){

  //Si el control automático de luces está activado
  if(controlLuces){
    int hora = now.hour();///Toma la hora actual del sistema, luego se compara con horaInicio
    int tiempoCiclo;///El tiempo transcurrido entre el inicio del ciclo y la duración del ciclo
    tiempoCiclo = (int)horaInicio+(int)cLuz;
    int resto = (tiempoCiclo-24);
    if(tiempoCiclo<=23){
      Serial.print("Apagar luces a las: ");
      Serial.print(tiempoCiclo);
      Serial.println(" hs");
    }else if(tiempoCiclo>23){
      Serial.print("Apagar luces a las: ");
      Serial.print(resto);
      Serial.println(" hs");
    }
    //Encender Luces, iniciado el ciclo
    if(!luces){
      if(hora==horaInicio){
        encenderLuces();//Enciende luces
      }
    }
    //Apagar luces
    if(luces){
      if(tiempoCiclo<=23){
        if(hora==tiempoCiclo){
          apagarLuces();
        }
      }else if(tiempoCiclo>23){
        if(hora==resto){
          apagarLuces();
        }
      }
    }
  }
}

void encenderLuces(){
  digitalWrite(luz,LOW);//lógica invertida
  luces=true;
  Serial.println("Luces encendidas");///Impresiones en el serial para fines mantenimiento
}
void apagarLuces(){
  digitalWrite(luz,HIGH);//lógica invertida
  luces=false;
  Serial.println("Luces apagadas");///Impresiones en el serial para fines mantenimiento
}
void lecturasDHT(){//Lee temperatura y humedad relativa y las muestra en el LCD
  humedad = dht.readHumidity();
  temperatura = dht.readTemperature();

  ////MOSTRAR EN DISPLAY
  lcd.setCursor(0,0);
  lcd.print(temperatura);
  lcd.setCursor(2,0);
  lcd.write(4);
  lcd.setCursor(3,0);
  lcd.print('C');
  lcd.setCursor(0,1);
  lcd.print(humedad);
  lcd.setCursor(2,1);
  lcd.print('%');  
}
void mostrarHora(DateTime ahora){//IMPRIME HORA CON FORMATO
  lcd.setCursor(8,1);
  if(ahora.hour() < 10){
   lcd.setCursor(8,1);
   lcd.print('0');
   lcd.setCursor(9,1);
   lcd.print(ahora.hour(),DEC);
  }else if(ahora.hour()>=10){
   lcd.setCursor(8,1); 
   lcd.print(ahora.hour(),DEC);
  }
  lcd.setCursor(10,1);
  lcd.print(':');
  if(ahora.minute() < 10){
   lcd.setCursor(11,1);
   lcd.print('0');
   lcd.setCursor(12,1);
   lcd.print(ahora.minute(),DEC);
  }else if(ahora.minute()>=10){
   lcd.setCursor(11,1); 
   lcd.print(ahora.minute(),DEC);
  }
  lcd.setCursor(13,1);
  lcd.print(':');
  if(ahora.second() < 10){
   lcd.setCursor(14,1);
   lcd.print('0');
   lcd.setCursor(15,1);
   lcd.print(ahora.second(),DEC);
 }else if(ahora.second()>=10){
   lcd.setCursor(14,1); 
   lcd.print(ahora.second(),DEC);
 }
 /*Serial.print(ahora.hour(),DEC);
 Serial.print(":");
 Serial.print(ahora.minute(),DEC);
 Serial.print(":");
 Serial.println(ahora.second(),DEC);*/
}
void ventanaRiego(){
   for(int i=0;i<4;i++){
     lcd.setCursor(12+i,0);
     lcd.write(8);
     delay(300);        
   }
   for(int i=0;i<4;i++){
     lcd.setCursor(12+i,0);
     lcd.print(" ");
   }
}
