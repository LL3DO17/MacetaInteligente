#include <WiFiNINA.h>
#include <Firebase.h>
#include <Firebase_Arduino_WiFiNINA.h>

#include <BQ24195.h>

#include "arduino_secrets.h"
#include "batteryControl_JFL.h"

#define PIN_SENSOR_HUMEDAD A1  // Pin analógico conectado al sensor de humedad
#define PIN_RELAY 2            // Pin digital conectado al relay

String host = FIREBASE_HOST;
String auth = FIREBASE_AUTH;
String ssid_str = SECRET_SSID;
String pass_str = SECRET_PASS;

FirebaseData MacetaInteligenteDefinitiva;
String ruta = "MacetaInteligente";

// Declaracion de funciones
int tiempoLecturasFB();
int lecturasHumedadSuelo();
int humedadMaximaFB();
int humedadMinimaFB();
int tiempoRiegoFB();
int tiempoEntreRiegosFB();

void procesoRiegoAuto(int, int);
void activacionBomba();
void tiempomillis();

bool peticionRiegoFB();
bool petRiegoStringtoBool();

String checkEstadoMaceta(int, int);

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;

// Declaracion de variables del codigo
int humedadSuelo;
int humedadMin;
int humedadMax;

long tiempoLecturas;
long tiempoRiego;
long tiempoEntreRiegos;

bool estadoBomba = false;
bool peticionRiego = false;
String estadoMaceta;


void setup() {

  Serial.begin(9600);

  batterySetup();

  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW); // Apagar el relay al inicio

  while (status != WL_CONNECTED) {

    Serial.print("Trying to connect with: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);

  }

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Firebase.begin(host, auth, ssid_str, pass_str);
  Firebase.reconnectWiFi(true);

  // Establecer en Firebase inicialmente "peticionRiego" = false (creo que no hace falta)
  Firebase.setBool(MacetaInteligenteDefinitiva, ruta + "/Peticion Riego", peticionRiego);

}


void loop() {

  batteryCheck(); // Funcion que comprueba el nivel y el estado de la bateria Li-Po
  getChargeStatusMessage(); // Funcion que comprueba el estado de la batería Li-Po

  // Enviamos nivel y estado de la bateria a Firebase
  Firebase.setInt(MacetaInteligenteDefinitiva, ruta + "/Nivel Bateria", batteryCheck());
  Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Bateria", getChargeStatusMessage());

  // Leer y mostrar valores que detecta el sensor de humedad
  humedadMax = humedadMaximaFB();
  humedadMin = humedadMinimaFB();
  humedadSuelo = lecturasHumedadSuelo();

  if (humedadSuelo > humedadMax) { // Llamamos a la funcion de "humedadMax" y la comparamos con "humedadSuelo"

    // Actualizamos "estadoMaceta" y lo enviamos a Firebase
    estadoMaceta = "HumedadAlta";
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);

  } else if (humedadSuelo < humedadMin) { // Llamamos a la funcion de "humedadMin" y la comparamos con "humedadSuelo"

    procesoRiegoAuto(humedadSuelo, humedadMin);

    // Comprobamos el estado de la maceta despues de regar automaticamente y lo enviamos a Firebase
    estadoMaceta = checkEstadoMaceta(humedadSuelo, humedadMax);
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);

  } else {

    // Actualizamos "estadoMaceta" y lo enviamos a Firebase
    estadoMaceta = "HumedadCorrecta";
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);

  }

  peticionRiego = peticionRiegoFB(); // Leer de Firebase si se ha solicitado regar manualmente

  if (peticionRiego == true && estadoMaceta != "Regando...") { // Activamos el riego manual si se cumplen ambas condiciones

    // Actualizamos "estadoMaceta" y lo enviamos a Firebase
    estadoMaceta = "Regando...";
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);   

    activacionBomba();

    tiempoEntreRiegos = tiempoEntreRiegosFB();

    tiempomillis();

    // Comprobamos el estado de la maceta despues de regar automaticamente y lo enviamos a Firebase
    estadoMaceta = checkEstadoMaceta(humedadSuelo, humedadMax);
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);

    // Volvemos a establecer "peticionRiego" a false y lo enviamos a Firebase
    peticionRiego = false;
    Firebase.setBool(MacetaInteligenteDefinitiva, ruta + "/Peticion Riego", peticionRiego);  

  }

  // Leer "tiempoLecturas" y establecerlo como delay
  delay(tiempoLecturasFB());

} 


int tiempoLecturasFB() {

  if (Firebase.getString(MacetaInteligenteDefinitiva, ruta + "/Tiempo Lecturas")) {

    String tmpLecturasStr = MacetaInteligenteDefinitiva.stringData();

    // Obtener el valor numérico eliminando los caracteres adicionales
    tmpLecturasStr = tmpLecturasStr.substring(2, tmpLecturasStr.length() - 2);
    tiempoLecturas = tmpLecturasStr.toInt();

  }

  return tiempoLecturas;

}

int lecturasHumedadSuelo() {

  // Leer y declarar humedad que registra el sensor de humedad
  int valorHumedad = analogRead(PIN_SENSOR_HUMEDAD);

  // Convertir el valor obtenido a humedad relativa (%HR)
  int humedadSuelo = map(valorHumedad, 1023, 0, 0, 100);

  // Imprimimos el valor de humedad obtenido
  Serial.print(F("Humedad del Suelo: "));
  Serial.print(humedadSuelo);
  Serial.println(F("%"));

  // Enviamos "humedadSuelo" a Firebase
  Firebase.setInt(MacetaInteligenteDefinitiva, ruta + "/Humedad Suelo", humedadSuelo);

  return humedadSuelo;
}

int humedadMaximaFB() {

  if (Firebase.getString(MacetaInteligenteDefinitiva, ruta + "/Humedad Maxima")) {

    String humMaxStr = MacetaInteligenteDefinitiva.stringData();

    // Obtener el valor numérico eliminando los caracteres adicionales
    humMaxStr = humMaxStr.substring(2, humMaxStr.length() - 2);
    Serial.print(F("Humedad Maxima: "));
    Serial.print(humMaxStr);
    Serial.println(F("%"));
    humedadMax = humMaxStr.toInt();

  }

  return humedadMax;

}

int humedadMinimaFB() {

  if (Firebase.getString(MacetaInteligenteDefinitiva, ruta + "/Humedad Minima")) {

    String humMinStr = MacetaInteligenteDefinitiva.stringData();

    // Obtener el valor numérico eliminando los caracteres adicionales
    humMinStr = humMinStr.substring(2, humMinStr.length() - 2);
    Serial.print(F("Humedad Minima: "));
    Serial.print(humMinStr);
    Serial.println(F("%"));
    humedadMin = humMinStr.toInt();

  }

  return humedadMin;

}

void procesoRiegoAuto(int humedadSuelo, int humedadMin) {

  while (humedadSuelo < humedadMin + 10) {

    // Actualizamos "estadoMaceta" y lo enviamos a Firebase
    estadoMaceta = "HumedadBaja";
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);
    delay(2000); // Para que el mensaje "Humedad Baja" se pueda apreciar por unos instantes
    estadoMaceta = "Regando...";
    Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Maceta", estadoMaceta);

    activacionBomba();

    tiempoEntreRiegos = tiempoEntreRiegosFB();

    tiempomillis();

    //Actualizamos "humedadMin" y "humedadSuelo" por si se modifican durante el proceso y lo enviamos a Firebase
    humedadMin = humedadMinimaFB();
    humedadSuelo = lecturasHumedadSuelo();

  }

}

int tiempoRiegoFB() {

  if (Firebase.getString(MacetaInteligenteDefinitiva, ruta + "/Tiempo Riego")) {

    String tmpRiegoStr = MacetaInteligenteDefinitiva.stringData();

    // Obtener el valor numérico eliminando los caracteres adicionales
    tmpRiegoStr = tmpRiegoStr.substring(2, tmpRiegoStr.length() - 2);
    Serial.print(F("Tiempo de riego: "));
    Serial.print(tmpRiegoStr);
    Serial.println(F("s"));
    tiempoRiego = tmpRiegoStr.toInt();

  }

  return tiempoRiego;

}

void activacionBomba() {

  // Activamos el relay y por lo tanto, tambien la bomba
  digitalWrite(PIN_RELAY, HIGH);
  Serial.println("Bomba encendida");

  // Actualizamos "estadoBomba" y lo enviamos a Firebase
  estadoBomba = true;
  Firebase.setBool(MacetaInteligenteDefinitiva, ruta + "/Estado Bomba", estadoBomba);

  delay(tiempoRiegoFB());

  // Desactivamos el relay y por lo tanto, tambien la bomba
  digitalWrite(PIN_RELAY, LOW);
  Serial.println("Bomba apagada");

  // Actualizamos "estadoBomba" y lo enviamos a Firebase
  estadoBomba = false;
  Firebase.setBool(MacetaInteligenteDefinitiva, ruta + "/Estado Bomba", estadoBomba);

}

int tiempoEntreRiegosFB() {

  // Lectura y delay de "tiempoEntreRiegos" desde Firebase
  if (Firebase.getString(MacetaInteligenteDefinitiva, ruta + "/Tiempo entre Riegos")) {

  String tmpEntreRiegosStr = MacetaInteligenteDefinitiva.stringData();

  // Obtener el valor numérico eliminando los caracteres adicionales
  tmpEntreRiegosStr = tmpEntreRiegosStr.substring(2, tmpEntreRiegosStr.length() - 2);
  tiempoEntreRiegos = tmpEntreRiegosStr.toInt();

  }

  return tiempoEntreRiegos;

}

bool peticionRiegoFB() {

  peticionRiego = petRiegoStringtoBool();

  return peticionRiego;

}

bool petRiegoStringtoBool() {
  
  if (Firebase.getString(MacetaInteligenteDefinitiva, ruta + "/Peticion Riego")) {

    String petRiegoStr = MacetaInteligenteDefinitiva.stringData();

    // Obtener el valor booleano eliminando los caracteres adicionales
    petRiegoStr = petRiegoStr.substring(2, petRiegoStr.length() - 2);

    if (petRiegoStr == "true") {

      return true;

    } else if (petRiegoStr == "false") {

      return false;

    } else {

      return nanl;

    }   

  }

}

void tiempomillis() {

  int tiempoActual = millis();
  tiempoEntreRiegos = tiempoEntreRiegosFB();

  while (millis() < tiempoActual + tiempoEntreRiegos) {

    humedadSuelo = lecturasHumedadSuelo();

    delay(30000); 

  }

}

String checkEstadoMaceta(int humedadSuelo, int humedadMax) {

  if (humedadSuelo > humedadMax) { // Humedad de la maceta por encima del maximo

    estadoMaceta = "HumedadAlta";

  } else { // Humedad por encima del minimo y por debajo del maximo

    estadoMaceta = "HumedadCorrecta";

  }

  return estadoMaceta;

}