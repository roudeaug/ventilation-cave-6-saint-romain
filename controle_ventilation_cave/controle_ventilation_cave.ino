/**************************************************************************************************
Nom ......... : controle_ventilation_cave.ino
Rôle ........ : Réalise des mesures de température, humidité, pression athmosphérique toute les 30 minutes
                et ajuste la puissance de ventilation nécessaire dans la cave en fonction.
Auteur ...... : Gaëtan ROUDEAU
Version ..... : V1.0 du 25/10/2024
Licence ..... : Copyright (C) 2024  Gaëtan ROUDEAU

                This program is free software: you can redistribute it and/or modify
                it under the terms of the GNU General Public License as published by
                the Free Software Foundation, either version 3 of the License, or
                (at your option) any later version.

                This program is distributed in the hope that it will be useful,
                but WITHOUT ANY WARRANTY; without even the implied warranty of
                MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                GNU General Public License for more details.

                You should have received a copy of the GNU General Public License
                along with this program.  If not, see <http://www.gnu.org/licenses/>

Compilation . : Avec l'IDE Arduino
****************************************************************************************************/

/* LIBRAIRIES */
#include <Adafruit_BME280.h>                            // Librairie du capteur BME280 <version 2.2.4>
#include <Adafruit_SSD1306.h>                           // Librairie de l'écran OLED <version 2.5.12>
#include "Stepper.h"                                    // Librairie du moteur pas à pas <version 1.1.3>

/* DÉCLARATION DES PINS */
#define pinRelais 17                                    // Le pin 17 est utilisé pour activé ou désactivé le relais 230V

/* CONSTANTES */
const int NB_STEPS = 6;                                 // Nombre de pas de rotation
const int MIN_RATE_POWER = 50;                          // Humidité minimale requise pour activé la ventilation
const int MAX_RATE_POWER = 80;                          // Humidité à partir de laquelle la puissance est maximale
const float PERCENT_POWER_STEP = 16.67;                 // Pourcentage de puissance par pas de rotation
const int MIN_POWER = 17;                               // Puissance minimale du moteur peu importe la valeur d'humidité lue
const int ROTATION_MIN = 0;                             // Angle de rotation minimal
const int ROTATION_MAX = 990;                           // Angle de rotation maximal
const int ROTATION_STEP = 165;                          // Pas de rotation
const int STEPS_PER_REVOLUTION = 100;                   // Nombre de pas par révolution complète du moteur pas à pas
const int MOTOR_SPEED = 100;                            // Vitesse de rotation du moteur pas à pas
const int DELAY = 1800000;                              // Delais de temporisation avant chaque prise de mesure (30 minutes = 1 800 000 milliseconds)

/* VARIABLES GLOBALES */  
Adafruit_BME280 bme;                                    // Instance du capteur BME280
Adafruit_SSD1306 ecranOLED(128, 64, &Wire, -1);         // Instance de l'écran OLED
Stepper myStepper(STEPS_PER_REVOLUTION, 12, 13, 14, 15);// Instance du moteur pas à pas
int h_avant = -1;                                       // Valeur d'humidité précedente


/* CONFIGURATION */
void setup() {
  Serial.begin(9600);                                   // Initialisation du port série (pour l'envoi d'informations via le moniteur série de l'IDE Arduino)

  pinMode(pinRelais, OUTPUT);                           // Déclaration du pin de sortie pour le relais 230V
  digitalWrite(pinRelais, LOW);                         // Initialisation du relais à désactivé par défaut

  //Initialisation de la librairie BME280 en mode I2C
  Serial.println("Initialisation du BME280");
  if(!bme.begin(0x76)) {                                // Adresse hexadécimale I2C de l'écran BME280 : 0x76
    Serial.println("--> ÉCHEC…");
    while(1);                                           // Arrêt du programme, en cas d'échec de l'initialisation
  } 
  else {
    Serial.println("--> RÉUSSIE !");
  }

  //Initialisation de la librairie SSD1306 pour l'écran OLED
  Serial.println("Initialisation de l'écran OLED");
  if(!ecranOLED.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {    // Adresse hexadécimale I2C de l'écran BME280 : 0x3c
    Serial.println("--> ÉCHEC…");
    while(1);                                           // Arrêt du programme, en cas d'échec de l'initialisation
  } else {
    Serial.println("--> RÉUSSIE !");
  }

  //Initialisation de la vitesse du moteur
  Serial.println("Initialisation du moteur");
  myStepper.setSpeed(MOTOR_SPEED);                      // Initialisation de la vitesse de rotation du moteur pas à pas

  // Remise en position de par défaut (puissance minimale)
  myStepper.step(ROTATION_MAX * 2);
}

/* BOUCLE PRINCIPALE */
void loop() {

  float temperature = bme.readTemperature();            // Lecture de la valeur de température par le capteur BME280
  float humidite = bme.readHumidity();                  // Lecture de la valeur d'humidité par le capteur BME280
  float pression = bme.readPressure() / 100.0;          // Lecture de la pression atmosphérique par le capteur BME280

  long h = 0;

  if (h_avant == -1) {                                  // PREMIER PASSAGE --> INITIALISATION
    h_avant = humidite;                                 // Initialisation de la valeur précédente avec la première mesure effectuée
    ecranOLED.clearDisplay();                           // Initilisation de l'affichage sur l'écran
  }

  else {                                                // TOUS LES AUTRES PASSAGES --> TRAITEMENT DES DONNÉES

    h = round(humidite / 5) * 5;                        // Arrondi à 5 près de la valeur d'humidité pour simplifier les calculs
  
    int r = rotationMoteur(h);                          // Calcul de l'angle de la rotation du moteur
    int p = puissanceMoteur(h);                         // Calcul de la puissance effective du moteur

    // Affichages
    affichageEcranOLED(temperature, humidite, pression, p);
    /* DEBUG */ //affichageMoniteurSerie(temperature, humidite, pression, p);

    // Rotation moteur
    myStepper.step(r * ROTATION_STEP);                  // On multiplie le coefficient de rotation pas la valeur du pas de rotation du moteur pour avoir l'angle total de rotation

    h_avant = h;                                        // Sauvegarde de la valeur d'humidité précédente

    Serial.println("");

    delay(DELAY);                                       // Temporisation de 30 minutes avant nouvelle boucle
  }
}


int positionMoteur(int h){
  // Calcul de la position du moteur en fonction de l'humidité
  int pos = 0;

  if (h < 0) {
    pos = -1;
  }
  else if (h <= MIN_RATE_POWER) {
    pos = 0;
  }
  else if (h >= MAX_RATE_POWER) {
    pos = NB_STEPS;
  }
  else {
    pos = round((h - MIN_RATE_POWER) / 5);
  }

  return pos;
}

int rotationMoteur(int h) {
  // Calcul de la rotation du moteur en fonction de l'humidité
  int pos_h = positionMoteur(h);
  int pos_ha = positionMoteur(h_avant);

  int delta_pos = 0;
  
  // Contrôle des valeurs de positions calculées.
  // Par défaut, aucun mouvement
  if (pos_h >= 0 && pos_ha >= 0) {
    delta_pos = pos_h - pos_ha;
  }

  return -delta_pos;                                      // La rotation physique du moteur est inversée
}

int puissanceMoteur(int h) {
  // Calcul de la puissance du moteur en fonction de l'humidité
  int p = 0;

  if (h <= MIN_RATE_POWER) {
    p = MIN_POWER;
  }
  else if (h >= MAX_RATE_POWER) {
    p = 100;
  }
  else {
    p = round(((h - MIN_RATE_POWER) / 5) * PERCENT_POWER_STEP);
  }

  return p;
}

void affichageEcranOLED(float temperature, float humidite, float pression, int p) {
  // Raffraichissement écran OLED
  ecranOLED.clearDisplay();
  ecranOLED.setTextColor(SSD1306_WHITE);
  ecranOLED.setTextSize(2);
  ecranOLED.setCursor(0, 0);                             // Déplacement du curseur en position (0,0), c'est à dire dans l'angle supérieur gauche

  // Chargement des données de TEMPÉRATURE sur l'écran OLED
  ecranOLED.print(temperature, 1);
  ecranOLED.print(" ");
  ecranOLED.print((char)247);
  ecranOLED.println("C");
  // Chargement des données d'HUMIDITÉ sur l'écran OLED
  ecranOLED.print(humidite, 1);
  ecranOLED.println(" %");
  // Chargement des données de PRESSION sur l'écran OLED
  ecranOLED.print(pression, 2);
  ecranOLED.println("hPa");
  // Chargement des données de PUISSANCE MOTEUR sur l'écran OLED
  ecranOLED.println("ON  (" + String(p) + "%)");

  ecranOLED.display();
}

void affichageMoniteurSerie(float temperature, float humidite, float pression, int p) {
  Serial.println("Lecture des données...");

  // Affichage moniteur série de la TEMPÉRATURE
  Serial.print(F("Température = "));
  Serial.print(temperature);
  Serial.println(F(" °C"));

  // Affichage moniteur série du TAUX D'HUMIDITÉ
  Serial.print(F("Humidité = "));
  Serial.print(humidite);
  Serial.println(F(" %"));
  
  // Affichage moniteur série de la PRESSION ATMOSPHÉRIQUE
  Serial.print(F("Pression atmosphérique = "));
  Serial.print(pression);
  Serial.println(F(" hPa"));

  // Affichage moniteur série de la PUISSANCE MOTEUR
  Serial.print(F("Puissance moteur = "));
  Serial.print(p);
  Serial.println(F(" %"));
}

