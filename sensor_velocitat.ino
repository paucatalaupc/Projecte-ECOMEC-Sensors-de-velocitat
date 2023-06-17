/***************************************************************************************************************/
/*   _____ ______ _   _  _____  ____  _____     __      ________ _      ____   _____ _____ _______    _______  */
/*  / ____|  ____| \ | |/ ____|/ __ \|  __ \    \ \    / /  ____| |    / __ \ / ____|_   _|__   __|/\|__   __| */
/* | (___ | |__  |  \| | (___ | |  | | |__) |    \ \  / /| |__  | |   | |  | | |      | |    | |  /  \  | |    */
/*  \___ \|  __| | . ` |\___ \| |  | |  _  /      \ \/ / |  __| | |   | |  | | |      | |    | | / /\ \ | |    */
/*  ____) | |____| |\  |____) | |__| | | \ \       \  /  | |____| |___| |__| | |____ _| |_   | |/ ____ \| |    */
/* |_____/|______|_| \_|_____/ \____/|_|  \_\       \/   |______|______\____/ \_____|_____|  |_/_/    \_\_|    */
/*                                                                                                             */
/***************************************************************************************************************/

/* Includes */
#include <LiquidCrystal.h>

/* Configuracio del LCD */
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* Defines per les linies adalt i abaix */
#define LINE_UP   0
#define LINE_DOWN 1

/* Defines per les tecles polsades */
#define BUTTON_RIGHT  0
#define BUTTON_UP     1
#define BUTTON_DOWN   2
#define BUTTON_LEFT   3
#define BUTTON_SELECT 4
#define BUTTON_NONE   5

/* Teclada polsada i tecla anterior */
int lcd_key     = BUTTON_NONE;
int lcd_key_old = BUTTON_NONE;

/* Lectura del ADC per saber la tecla polsada */
int adc_key_in  = 0;

/* Maquina estats del sensor IR */
typedef enum {
  SV_INIT,
  SV_IDLE,
  SV_SENSOR_ENTRADA_INICIAL_LLEGIT,
  SV_SENSOR_ENTRADA_ERROR,
  SV_SENSOR_ENTRADA_PINTAR_VELOCITAT,
  SV_ESPERANT_SENSOR_SORTIDA,
  SV_SENSOR_SORTIDA_INICIAL_LLEGIT,
  SV_SENSOR_SORTIDA_ERROR,
  SV_SENSOR_SORTIDA_PINTAR_VELOCITAT,
  SV_LCD_CLEAR_DISPLAY,
}sensor_velocitat_status_e;

/* Estructura de cada sensor IR */
typedef struct {
  struct {
    uint16_t velocitat_error :  1;
    uint16_t vacios          : 15;
  } flags;
  uint16_t pin_sensor_inicial;
  uint16_t pin_sensor_final;
  uint16_t distancia_entre_sensors_mm;
  unsigned long tpo_sensor_inicial_ms;
  unsigned long tpo_sensor_final_ms;
  unsigned long tpo_total_ms;
  double velocitat_m_s;
}data_sensor_t;

/* Estructura del sensor de velocitat */
typedef struct {
  sensor_velocitat_status_e status;
  sensor_velocitat_status_e status_display;
  data_sensor_t sensor_entrada;
  data_sensor_t sensor_sortida;
  double acceleracio_m_s2;
}sensor_velocitat_t;

/* Selector pantalles del display */
typedef enum {
  SELECTOR_PANTALLA_DOS_SENSORS_VELOCITAT,
  SELECTOR_PANTALLA_S1_S2_AC,
}selector_pantalla_e;

/* Selector pantalla sensor */
typedef enum {
  SELECTOR_SENSOR_VELOCITAT_SENSOR_1,
  SELECTOR_SENSOR_VELOCITAT_SENSOR_2,
  SELECTOR_SENSOR_ACCELERACIO,
}selector_sensor_e;

/* Estructura selector pantalles display */
typedef struct {
  selector_pantalla_e selector_pantalla;
  selector_sensor_e   selector_sensor;
}selector_lcd_t;

/* Estructura temporitzadors */
typedef struct {
  unsigned long missatge_inicial_ms;
  unsigned long refresh_display_ms;
}temporitzadors_t;

/* Variables de treball */
sensor_velocitat_t gSensorVelocitat;
selector_lcd_t     gSelectorLCD;
temporitzadors_t   gTemporitzadors;
unsigned long      global_ms;

/*****************************************************************************/
/* Lectura dels botons a traves de la entrada analogica A0                   */
/*****************************************************************************/
int read_Buttons(void) {

  adc_key_in = analogRead(A0);

  if (adc_key_in > 900) return BUTTON_NONE;
  if (adc_key_in <  50) return BUTTON_RIGHT;
  if (adc_key_in < 250) return BUTTON_UP;
  if (adc_key_in < 450) return BUTTON_DOWN;
  if (adc_key_in < 650) return BUTTON_LEFT;
  if (adc_key_in < 850) return BUTTON_SELECT;

  return BUTTON_NONE;

}

/*****************************************************************************/
/* Llegim els botons del display i comprovem si ha modificat la tecla        */
/*****************************************************************************/
void gest_Buttons(void) {

  /* llegim la entrada analogica A0 */
  lcd_key = read_Buttons();

  /* si la tecla es diferent a la polsada anteriorment */
  if (lcd_key_old != lcd_key) {
    /* actualitzem la tecla */
    lcd_key_old = lcd_key;

    /* netejem el display */
    gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;

    switch (lcd_key) {
      case BUTTON_UP   :
      case BUTTON_DOWN :
        switch (gSelectorLCD.selector_pantalla) {
          default:
          case SELECTOR_PANTALLA_DOS_SENSORS_VELOCITAT : gSelectorLCD.selector_pantalla = SELECTOR_PANTALLA_S1_S2_AC;              break;
          case SELECTOR_PANTALLA_S1_S2_AC              : gSelectorLCD.selector_pantalla = SELECTOR_PANTALLA_DOS_SENSORS_VELOCITAT; break;
        }
        break;

      case BUTTON_LEFT :
        switch (gSelectorLCD.selector_sensor) {
          default : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_ACCELERACIO; break;
          case SELECTOR_SENSOR_VELOCITAT_SENSOR_1 : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_ACCELERACIO;        break;
          case SELECTOR_SENSOR_VELOCITAT_SENSOR_2 : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_VELOCITAT_SENSOR_1; break;
          case SELECTOR_SENSOR_ACCELERACIO        : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_VELOCITAT_SENSOR_2; break;
        }
        break;

      case BUTTON_RIGHT :
        switch (gSelectorLCD.selector_sensor) {
          default : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_ACCELERACIO; break;
          case SELECTOR_SENSOR_VELOCITAT_SENSOR_1 : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_VELOCITAT_SENSOR_2; break;
          case SELECTOR_SENSOR_VELOCITAT_SENSOR_2 : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_ACCELERACIO;        break;
          case SELECTOR_SENSOR_ACCELERACIO        : gSelectorLCD.selector_sensor = SELECTOR_SENSOR_VELOCITAT_SENSOR_1; break;
        }
        break;
    }
  }

}

/*****************************************************************************/
/* Calculem la velocitat (m/s)                                               */
/*****************************************************************************/
double calcula_Velocitat(uint16_t distancia_mm, unsigned long tpo_ms) {

  double velocitat_m_s = 12.36;

  velocitat_m_s = (distancia_mm * 10000.0) / tpo_ms;
  velocitat_m_s /= 10000.0;

  return velocitat_m_s;

}

/*****************************************************************************/
/* Calculem la acceleracio (m/s^2)                                           */
/*****************************************************************************/
double calcula_Acceleracio(double vEntrada_m_s, double vSortida_m_s, unsigned long tpoEntrada_ms, unsigned long tpoSortida_ms) {

  double acceleracio_m_s2 = 0.0;

  /* tenir en compte el signe, ja que pot accelerar o decelerar */
  acceleracio_m_s2 = (vSortida_m_s - vEntrada_m_s);// * 1000;

  /* es un diferencial de temps, no importa el signe */
  if (tpoSortida_ms > tpoEntrada_ms)
    acceleracio_m_s2 /= (tpoSortida_ms - tpoEntrada_ms);
  else
    acceleracio_m_s2 /= (tpoEntrada_ms - tpoSortida_ms);

  return (acceleracio_m_s2 * 1000);

}

/*****************************************************************************/
/* Gestio dels sensors IR                                                    */
/*****************************************************************************/
void gest_SensorsIR(void) {

  switch (gSensorVelocitat.status) {
    default:
    case SV_INIT :
      if (global_ms < gTemporitzadors.missatge_inicial_ms)
        break;

      gSensorVelocitat.status         = SV_IDLE;
      gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;
      break;

    case SV_IDLE :
      /* mentre no vegi el IR inicial del sensor entrada, deu! */
      if (digitalRead(gSensorVelocitat.sensor_entrada.pin_sensor_inicial) == HIGH)
        break;

      /* ens apuntem el temps i passem a esperar el IR final del sensor entrada */
      gSensorVelocitat.sensor_entrada.tpo_sensor_inicial_ms = global_ms;
      gSensorVelocitat.status         = SV_SENSOR_ENTRADA_INICIAL_LLEGIT;
      gSensorVelocitat.status_display = SV_SENSOR_ENTRADA_INICIAL_LLEGIT;
      /* no break */

    case SV_SENSOR_ENTRADA_INICIAL_LLEGIT :
      /* mentre no vegi el IR final del sensor entrada, deu! */
      if (digitalRead(gSensorVelocitat.sensor_entrada.pin_sensor_final) == HIGH)
        break;

      /* ens apuntem el temps i calculem el temps entre els 2 sensors */
      gSensorVelocitat.sensor_entrada.tpo_sensor_final_ms = global_ms;
      gSensorVelocitat.sensor_entrada.tpo_total_ms = gSensorVelocitat.sensor_entrada.tpo_sensor_final_ms - gSensorVelocitat.sensor_entrada.tpo_sensor_inicial_ms;

      /* si el temps entre sensors es 0, indica que els cables no estan connectats */
      if (gSensorVelocitat.sensor_entrada.tpo_total_ms == 0) {
        gSensorVelocitat.sensor_entrada.flags.velocitat_error = 1;
        gSensorVelocitat.sensor_entrada.velocitat_m_s = 0.0;
        /* passem a error */
        gSensorVelocitat.status         = SV_SENSOR_ENTRADA_ERROR;
        gSensorVelocitat.status_display = SV_SENSOR_ENTRADA_ERROR;
      } else {
        gSensorVelocitat.sensor_entrada.flags.velocitat_error = 0;
        /* calculem la velocitat en m/s */
        gSensorVelocitat.sensor_entrada.velocitat_m_s = calcula_Velocitat(gSensorVelocitat.sensor_entrada.distancia_entre_sensors_mm, gSensorVelocitat.sensor_entrada.tpo_total_ms);
        /* passem a pintar la velocitat */
        gSensorVelocitat.status         = SV_SENSOR_ENTRADA_PINTAR_VELOCITAT;
        gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;
      }
      break;

    case SV_SENSOR_ENTRADA_PINTAR_VELOCITAT :
      gSensorVelocitat.status         = SV_ESPERANT_SENSOR_SORTIDA;
      gSensorVelocitat.status_display = SV_ESPERANT_SENSOR_SORTIDA;
      break;

    case SV_ESPERANT_SENSOR_SORTIDA :
      /* quan vegi el IR inicial del sensor sortida */
      if (digitalRead(gSensorVelocitat.sensor_entrada.pin_sensor_inicial) == HIGH)
      //if (digitalRead(gSensorVelocitat.sensor_sortida.pin_sensor_inicial) == HIGH)
        break;

      /* ens apuntem el temps i passem a esperar el IR final del sensor sortida */
      gSensorVelocitat.sensor_sortida.tpo_sensor_inicial_ms = global_ms;
      gSensorVelocitat.status         = SV_SENSOR_SORTIDA_INICIAL_LLEGIT;
      gSensorVelocitat.status_display = SV_SENSOR_SORTIDA_INICIAL_LLEGIT;
      /* no break */

    case SV_SENSOR_SORTIDA_INICIAL_LLEGIT :
      /* quan vegi el IR inicial del sensor sortida */
      if (digitalRead(gSensorVelocitat.sensor_entrada.pin_sensor_final) == HIGH)
      //if (digitalRead(gSensorVelocitat.sensor_sortida.pin_sensor_final) == HIGH)
        break;

      /* ens apuntem el temps i calculem el temps entre els 2 sensors */
      gSensorVelocitat.sensor_sortida.tpo_sensor_final_ms = global_ms;
      gSensorVelocitat.sensor_sortida.tpo_total_ms = gSensorVelocitat.sensor_sortida.tpo_sensor_final_ms - gSensorVelocitat.sensor_sortida.tpo_sensor_inicial_ms;

      /* si el temps entre sensors es 0, indica que els cables no estan connectats */
      if (gSensorVelocitat.sensor_sortida.tpo_total_ms == 0) {
        gSensorVelocitat.sensor_sortida.flags.velocitat_error = 1;
        gSensorVelocitat.sensor_sortida.velocitat_m_s = 0.0;
        /* passem a error */
        gSensorVelocitat.status         = SV_SENSOR_SORTIDA_ERROR;
        gSensorVelocitat.status_display = SV_SENSOR_SORTIDA_ERROR;
      } else {
        gSensorVelocitat.sensor_sortida.flags.velocitat_error = 0;
        /* calculem la velocitat en m/s */
        gSensorVelocitat.sensor_sortida.velocitat_m_s = calcula_Velocitat(gSensorVelocitat.sensor_sortida.distancia_entre_sensors_mm, gSensorVelocitat.sensor_sortida.tpo_total_ms);
        /* calculem la acceleracio en m/s^2 */
        gSensorVelocitat.acceleracio_m_s2 = calcula_Acceleracio(gSensorVelocitat.sensor_entrada.velocitat_m_s        , gSensorVelocitat.sensor_sortida.velocitat_m_s,
                                                                gSensorVelocitat.sensor_entrada.tpo_sensor_inicial_ms, gSensorVelocitat.sensor_sortida.tpo_sensor_inicial_ms);
        /* passem a pintar la velocitat */
        gSensorVelocitat.status         = SV_SENSOR_SORTIDA_PINTAR_VELOCITAT;
        gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;
      }
      break;

    case SV_SENSOR_SORTIDA_PINTAR_VELOCITAT :
      gSensorVelocitat.status         = SV_IDLE;
      gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;//SV_IDLE;
      break;

    case SV_SENSOR_ENTRADA_ERROR :
      /* quan vegi el IR inicial del sensor entrada */
      if ((digitalRead(gSensorVelocitat.sensor_entrada.pin_sensor_inicial) == HIGH) && (digitalRead(gSensorVelocitat.sensor_entrada.pin_sensor_final) == HIGH)) {
        gSensorVelocitat.status         = SV_IDLE;
        gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;
      }
      break;

    case SV_SENSOR_SORTIDA_ERROR :
      /* quan vegi el IR inicial del sensor entrada */
      if ((digitalRead(gSensorVelocitat.sensor_sortida.pin_sensor_inicial) == HIGH) && (digitalRead(gSensorVelocitat.sensor_sortida.pin_sensor_final) == HIGH)) {
        gSensorVelocitat.status         = SV_IDLE;
        gSensorVelocitat.status_display = SV_LCD_CLEAR_DISPLAY;
      }
      break;
  }

}

/*****************************************************************************/
/* Gestio del display, refrescar cada 250 ms                                 */
/*****************************************************************************/
void gest_Display(void) {

  /* refrescar cada 250 ms */
  if (global_ms < gTemporitzadors.refresh_display_ms)
    return;

  /* carreguem el temporitzador */
  gTemporitzadors.refresh_display_ms = global_ms + 250;

  switch (gSensorVelocitat.status_display) {
    case SV_INIT :
      //0123456789012345
      //DEP.ENG.MECANICA
      //SENSOR VELOCITAT
      lcd.setCursor(0, LINE_UP  ); lcd.print("DEP.ENG.MECANICA");
      lcd.setCursor(0, LINE_DOWN); lcd.print("SENSOR VELOCITAT");
      break;

    case SV_SENSOR_ENTRADA_ERROR :
    case SV_SENSOR_SORTIDA_ERROR :
      //0123456789012345 | //0123456789012345
      //>SENSOR ENTRADA< | //>SENSOR SORTIDA<
      //Comprovar cables | //Comprovar cables
      lcd.setCursor(0, LINE_UP  ); lcd.print((gSensorVelocitat.status_display == SV_SENSOR_ENTRADA_ERROR) ? ">SENSOR ENTRADA<" : ">SENSOR SORTIDA<");
      lcd.setCursor(0, LINE_DOWN); lcd.print("Comprovar cables");
      break;

    case SV_LCD_CLEAR_DISPLAY :
      lcd.clear();
      /* no break */
    default:
      gSensorVelocitat.status_display = SV_IDLE;
      /* no break */
    case SV_IDLE :
    case SV_ESPERANT_SENSOR_SORTIDA :
      switch (gSelectorLCD.selector_pantalla) {
        default:
          gSelectorLCD.selector_pantalla = SELECTOR_PANTALLA_DOS_SENSORS_VELOCITAT;
          /* no break */
        case SELECTOR_PANTALLA_DOS_SENSORS_VELOCITAT :
          if (gSensorVelocitat.status_display == SV_IDLE) {
            //0123456789012345
            //>S1 v = 0.00 m/s
            // S2 v = 0.00 m/s
            lcd.setCursor(0, LINE_UP  ); lcd.print(">S1 v = ");
            lcd.setCursor(0, LINE_DOWN); lcd.print(" S2 v = ");
          } else if (gSensorVelocitat.status_display == SV_ESPERANT_SENSOR_SORTIDA) {
            //0123456789012345
            // S1 v = 0.00 m/s
            //>S2 v = 0.00 m/s
            lcd.setCursor(0, LINE_UP  ); lcd.print(" S1 v = ");
            lcd.setCursor(0, LINE_DOWN); lcd.print(">S2 v = ");
          } else {
            //0123456789012345
            // S1 v = 0.00 m/s
            //>S2 v = 0.00 m/s
            lcd.setCursor(0, LINE_UP  ); lcd.print(" S1 v = ");
            lcd.setCursor(0, LINE_DOWN); lcd.print(" S2 v = ");
          }
          
          lcd.setCursor(8, LINE_UP  ); lcd.print(gSensorVelocitat.sensor_entrada.velocitat_m_s); lcd.print(" m/s");
          lcd.setCursor(8, LINE_DOWN); lcd.print(gSensorVelocitat.sensor_sortida.velocitat_m_s); lcd.print(" m/s");
          /* blink */
          switch (gSensorVelocitat.status) {
            case SV_IDLE                    : lcd.setCursor(0, LINE_UP  ); lcd.blink(); break;
            case SV_ESPERANT_SENSOR_SORTIDA : lcd.setCursor(0, LINE_DOWN); lcd.blink(); break;
            default                         : lcd.noBlink(); break;
          }
          break;

        case SELECTOR_PANTALLA_S1_S2_AC :
            switch (gSelectorLCD.selector_sensor) {
              default :                                                                        //0123456789012345 
              case SELECTOR_SENSOR_VELOCITAT_SENSOR_1 : lcd.setCursor( 0, LINE_UP  ); lcd.print(" [S1]  S2   AC  ");
                                                                                               //0123456789012345
                                                                                               //v=0.01m/s 9999ms
                                                        lcd.setCursor( 0, LINE_DOWN); lcd.print("v=");
                                                        lcd.setCursor( 2, LINE_DOWN); lcd.print(gSensorVelocitat.sensor_entrada.velocitat_m_s); lcd.print("m/s");
                                                        lcd.setCursor(10, LINE_DOWN); lcd.print(gSensorVelocitat.sensor_entrada.tpo_total_ms ); lcd.print("ms");
                                                        break;
                                                                                               //0123456789012345 
              case SELECTOR_SENSOR_VELOCITAT_SENSOR_2 : lcd.setCursor( 0, LINE_UP  ); lcd.print("  S1  [S2]  AC  ");
                                                                                               //0123456789012345
                                                                                               //v=0.01m/s 9999ms
                                                        lcd.setCursor( 0, LINE_DOWN); lcd.print("v=");
                                                        lcd.setCursor( 2, LINE_DOWN); lcd.print(gSensorVelocitat.sensor_sortida.velocitat_m_s); lcd.print("m/s");
                                                        lcd.setCursor(10, LINE_DOWN); lcd.print(gSensorVelocitat.sensor_sortida.tpo_total_ms ); lcd.print("ms");
                                                        break;
                                                                                               //0123456789012345 
              case SELECTOR_SENSOR_ACCELERACIO        : lcd.setCursor( 0, LINE_UP  ); lcd.print("  S1   S2  [AC] ");
                                                                                               //0123456789012345
                                                                                               //a = 0.01 m/s^2
                                                        lcd.setCursor( 0, LINE_DOWN); lcd.print("a = ");
                                                        lcd.setCursor( 4, LINE_DOWN); lcd.print(gSensorVelocitat.acceleracio_m_s2); lcd.print(" m/s^2");
                                                        break;
            }
            lcd.noBlink();
            break;
      }
      break;
  }

}

/*****************************************************************************/
/* Configuracio inicial                                                      */
/*****************************************************************************/
void setup(void) {

  /* SENSOR ENTRADA */
  gSensorVelocitat.sensor_entrada.pin_sensor_inicial = A1;
  gSensorVelocitat.sensor_entrada.pin_sensor_final   = A2;
  pinMode(gSensorVelocitat.sensor_entrada.pin_sensor_inicial, INPUT);
  pinMode(gSensorVelocitat.sensor_entrada.pin_sensor_final  , INPUT);
  gSensorVelocitat.sensor_entrada.distancia_entre_sensors_mm = 50;

  /* SENSOR SORTIDA */
  gSensorVelocitat.sensor_sortida.pin_sensor_inicial = A3;
  gSensorVelocitat.sensor_sortida.pin_sensor_final   = A4;
  pinMode(gSensorVelocitat.sensor_sortida.pin_sensor_inicial, INPUT);
  pinMode(gSensorVelocitat.sensor_sortida.pin_sensor_final  , INPUT);
  gSensorVelocitat.sensor_sortida.distancia_entre_sensors_mm = 50;

  /* PARAMETRES */
  global_ms = 0;
  gTemporitzadors.missatge_inicial_ms = 1500;
  gTemporitzadors.refresh_display_ms  = 250;

  gSensorVelocitat.status         = SV_INIT;
  gSensorVelocitat.status_display = SV_INIT;

  gSensorVelocitat.sensor_entrada.velocitat_m_s = 0;
  gSensorVelocitat.sensor_sortida.velocitat_m_s = 0;
  gSensorVelocitat.acceleracio_m_s2 = 0.0;

  /* LCD */
  lcd.begin(16, 2);

  /* SELECTOR LCD */
  gSelectorLCD.selector_pantalla = SELECTOR_PANTALLA_DOS_SENSORS_VELOCITAT;
  gSelectorLCD.selector_sensor   = SELECTOR_SENSOR_ACCELERACIO;

  /* TECLAT */
  lcd_key = lcd_key_old = read_Buttons();

}

/*****************************************************************************/
/* Cicle principal                                                           */
/*****************************************************************************/
void loop(void) {

  /* TICK */
  global_ms = millis();

  /* TECLAT */
  gest_Buttons();

  /* SENSORS IR */
  gest_SensorsIR();

  /* DISPLAY */
  gest_Display();

}
